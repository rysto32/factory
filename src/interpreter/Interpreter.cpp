/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ryan Stone
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "Interpreter.h"

#include "CommandFactory.h"
#include "ConfigNode.h"
#include "IngestManager.h"
#include "PermissionList.h"

#include <cassert>
#include <memory>
#include <string>
#include <vector>

#include <err.h>

const char Interpreter::INTERPRETER_REGISTRY_ENTRY = 0;

const struct luaL_Reg Interpreter::factoryModule [] = {
	{"add_definitions", AddDefinitionsWrapper},
	{"define_command", DefineCommandWrapper},
	{nullptr, nullptr}
};

void
Interpreter::LuaStateFree::operator()(lua_State *luaState)
{
	lua_close(luaState);
}

Interpreter::Interpreter(IngestManager &ingest, CommandFactory & c)
  : luaState(LuaStatePtr(luaL_newstate())),
    ingestMgr(ingest),
    commandFactory(c)
{
	// XXX this adds the io standard lib -- do we want that?
	luaL_openlibs(luaState.get());

	RegisterModules();

	lua_pushlightuserdata(luaState.get(), this);
	lua_rawsetp(luaState.get(),  LUA_REGISTRYINDEX, &INTERPRETER_REGISTRY_ENTRY);

	lua_newtable(luaState.get());
}

Interpreter::~Interpreter()
{

}

void
Interpreter::RegisterModules()
{
	luaL_newlib(luaState.get(), factoryModule);
	lua_setglobal(luaState.get(), "factory");
}

void
Interpreter::RunFile(const std::string & path)
{
	lua_State *lua = luaState.get();

	if (luaL_loadfile(lua, path.c_str()) != 0) {
		errx(1, "Failed to parse script: %s", lua_tostring(lua, -1));
	}

	if (lua_pcall(lua, 0, 0, 0) != 0) {
		errx(1, "Failed to run script: %s", lua_tostring(lua, -1));
	}
}


void
Interpreter::ConfigVisitor::operator()(int value) const
{
	lua_pushinteger(interp.luaState.get(), value);
}

void
Interpreter::ConfigVisitor::operator()(const std::string & value) const
{
	lua_pushstring(interp.luaState.get(), value.c_str());
}

void
Interpreter::ConfigVisitor::operator()(const ConfigNodeList & list) const
{
	lua_State *lua = interp.luaState.get();

	lua_createtable(lua, list.size(), 0);
	for (size_t i = 0; i < list.size(); ++i) {
		const auto & node = list.at(i);
		const ConfigNode::ValueType & config = node->GetValue();
		std::visit(*this, config);
		// +1 because lua arrays are 1-based
		lua_seti(lua, -2, i + 1);
	}
}

void
Interpreter::ConfigVisitor::operator()(const ConfigPairMap & map) const
{
	lua_State *lua = interp.luaState.get();

	lua_createtable(lua, 0, map.size());
	for (auto & [name, node] : map) {
		const ConfigNode::ValueType & config = node->GetValue();

		lua_pushstring(lua, name.c_str());
		std::visit(*this, config);
		lua_settable(lua, -3);
	}
}

void
Interpreter::PushConfig(const ConfigNode & node)
{
	const auto & config = node.GetValue();

	std::visit(ConfigVisitor(*this), config);
}

void
Interpreter::AddStringValuePair(const char * name, const char * value)
{
	lua_State *lua = luaState.get();

	lua_pushstring(lua, name);
	lua_pushstring(lua, value);
	lua_settable(lua, -3);
}

void
Interpreter::ProcessConfig(const ConfigNode & node)
{
	lua_State *lua = luaState.get();
	const auto & config = node.GetValue();

	const auto & pairs = std::get<ConfigPairMap>(config);

	for (const auto & [name, value] : pairs) {
		int ref = ingestMgr.GetIngest(name);
		if (ref == LUA_NOREF) {
			warnx("No ingest for config '%s'", name.c_str());
			continue;
		}

		lua_pushcfunction(lua, ErrorHandler);
		lua_rawgeti(lua, LUA_REGISTRYINDEX, ref);
		lua_newtable(lua);
		//XXX hardcoded defaults
		AddStringValuePair("AR", "/usr/bin/ar");
		AddStringValuePair("CXX", "/usr/local/bin/clang++80");
		AddStringValuePair("LD", "/usr/local/bin/clang++80");

		PushConfig(*value);
		int result = lua_pcall(lua, 2, 0, -4);
		if (result != LUA_OK) {

			errx(1, "Error running lua callback: %s", lua_tostring(lua, -1));
		}
		/* Pop ErrorHandler off the stack. */
		lua_pop(lua, 1);
	}
}

Interpreter *
Interpreter::GetInterpreter(lua_State *lua)
{
	Interpreter *interp;

	lua_rawgetp(lua, LUA_REGISTRYINDEX, &INTERPRETER_REGISTRY_ENTRY);
	interp = static_cast<Interpreter*>(lua_touserdata(lua, -1));

	assert (interp->luaState.get() == lua);

	lua_pop(lua, 1);

	return interp;
}

int
Interpreter::AddDefinitionsWrapper(lua_State *lua)
{
	return GetInterpreter(lua)->AddDefinitions();
}

int
Interpreter::DefineCommandWrapper(lua_State *lua)
{
	return GetInterpreter(lua)->DefineCommand();
}

void
Interpreter::ParseDefinition()
{
	lua_State * lua = luaState.get();

	luaL_checktype(lua, -1, LUA_TTABLE);

	lua_pushliteral(lua, "name");
	lua_gettable(lua, -2);
	luaL_checktype(lua, -1, LUA_TSTRING);
	std::string name = lua_tostring(lua, -1);
	lua_pop(lua, 1);

	lua_pushliteral(lua, "process");
	lua_gettable(lua, -2);
	luaL_checktype(lua, -1, LUA_TFUNCTION);
	int ref = luaL_ref(lua, LUA_REGISTRYINDEX);

	ingestMgr.AddIngest(std::move(name), ref);

}

/* Implements function factory.add_definitions(defs) */
int
Interpreter::AddDefinitions()
{
	int i;
	int numElements;
	lua_State * lua = luaState.get();

	/* Ensure that our first argument is a table. */
	luaL_checktype(lua, 1, LUA_TTABLE);

	numElements = luaL_len(lua, 1);
	for (i = 1; i <= numElements; ++i) {
		lua_geti(lua, 1, i);
		ParseDefinition();
		lua_pop(lua, 1);
	}

	return 0;
}

/*
 * Conver an index to an equivalant absolute index that references the same object
 * even as the stack is pushed or popped.
 */
int
Interpreter::LuaAbsoluteIndex(lua_State *lua, int relative)
{
	return (relative > 0 || relative <= LUA_REGISTRYINDEX) ? relative
	    : lua_gettop(lua) + relative + 1;
}

std::vector<std::string>
Interpreter::GetStringList(int relative)
{
	lua_State * lua = luaState.get();
	int stackIndex = LuaAbsoluteIndex(lua, relative);

	if (lua_isstring(lua, stackIndex)) {
		std::string str = lua_tostring(lua, stackIndex);
		return {str};
	}

	luaL_checktype(lua, stackIndex, LUA_TTABLE);

	std::vector<std::string> list;

	/* Start with the first entry in table */
	lua_pushnil(lua);
	while (lua_next(lua, stackIndex) != 0) {
		luaL_checktype(lua, -1, LUA_TSTRING);
		list.push_back(lua_tostring(lua, -1));
		lua_pop(lua, 1);
	}
	return list;
}

std::unordered_map<std::string, std::vector<std::string>>
Interpreter::GetInputs(int stackIndex)
{
	lua_State * lua = luaState.get();
	std::unordered_map<std::string, std::vector<std::string>> inputList;

	lua_pushnil(lua);
	while (lua_next(lua, stackIndex) != 0) {
		luaL_checktype(lua, -2, LUA_TSTRING);
		std::string permType = lua_tostring(lua, -2);
		std::vector<std::string> paths = GetStringList(-1);
		lua_pop(lua, 1);

		inputList.emplace(std::move(permType), std::move(paths));
	}

	return inputList;
}

// factory.define_command(products, inputs, arglist)
int
Interpreter::DefineCommand()
{
	std::vector<std::string> products = GetStringList(1);
	auto perms = GetInputs(2);
	std::vector<std::string> argList = GetStringList(3);

	commandFactory.AddCommand(products, perms, std::move(argList));

	return 0;
}

int
Interpreter::ErrorHandler(lua_State *lua)
{
	luaL_traceback(lua, lua, "error in lua script", 1);
	errx(1, "%s", lua_tostring(lua, -1));
}
