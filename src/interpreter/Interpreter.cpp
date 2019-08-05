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

#include "lua/NamedValue.h"
#include "lua/Parameter.h"
#include "lua/Table.h"
#include "lua/View.h"

#include <cassert>
#include <memory>
#include <string>
#include <vector>

#include <err.h>

template<typename ...Ts>
struct Visitor : Ts...
{
    Visitor(const Ts&... args) : Ts(args)...
    {
    }

    using Ts::operator()...;
};

template<typename... Ts>
auto make_visitor(Ts... lambdas)
{
    return Visitor<Ts...>(lambdas...);
}

const char Interpreter::INTERPRETER_REGISTRY_ENTRY = 0;

const struct luaL_Reg Interpreter::factoryModule [] = {
	{"add_definitions", AddDefinitionsWrapper},
	{"define_command", DefineCommandWrapper},
	{"include_script", IncludeScriptWrapper},
	{"include_config", IncludeConfigWrapper},
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
	lua_State *lua = luaState.get();

	lua_newtable(lua);
	lua_pushstring(lua, "internal");
	luaL_newlib(luaState.get(), factoryModule);
	lua_settable(lua, -3);
	lua_setglobal(luaState.get(), "factory");
}

void
Interpreter::RunFile(const std::string & path, const ConfigNode & config)
{
	lua_State *lua = luaState.get();

	if (luaL_loadfile(lua, path.c_str()) != 0) {
		errx(1, "Failed to parse script: %s", lua_tostring(lua, -1));
	}

	// XXX what does it mean for a script to be passed config?  We do nothing right now
	if (lua_pcall(lua, 0, 0, 0) != 0) {
		errx(1, "Failed to run script: %s", lua_tostring(lua, -1));
	}
}

std::optional<IncludeFile>
Interpreter::GetNextInclude()
{
	if (includeQueue.empty())
		return std::nullopt;

	IncludeFile include = std::move(includeQueue.front());
	includeQueue.pop_front();
	return include;
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
Interpreter::ProcessConfig(const ConfigNode & parentConfig, const ConfigNode & node)
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
		PushConfig(parentConfig);
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

int
Interpreter::IncludeConfigWrapper(lua_State *lua)
{
	return GetInterpreter(lua)->Include("factory.include_config", IncludeFile::CONFIG);
}

int
Interpreter::IncludeScriptWrapper(lua_State *lua)
{
	return GetInterpreter(lua)->Include("factory.include_script", IncludeFile::SCRIPT);
}

void
Interpreter::ParseDefinition(Lua::Table & def)
{
	Lua::View & lua = def.GetView();

	std::string name(def.GetString("name"));

	def.FetchValue("process");
	if (!lua.isfunction(-1))
		errx(1, "Field 'process' is expected to be a function");

	int ref = lua.SaveToRegistry();

	ingestMgr.AddIngest(std::move(name), ref);

}

/* Implements function factory.add_definitions(defs) */
int
Interpreter::AddDefinitions()
{
	Lua::View lua(luaState);
	Lua::Parameter defs("factory.add_definitions", "defs", 1);


	auto table = lua.GetTable(defs);
	table.IterateList([this] (int index, Lua::Table & def)
	{
		ParseDefinition(def);
	});

	return 0;
}

std::vector<std::string>
Interpreter::GetStringList(Lua::View & lua, const Lua::NamedValue & value)
{
	std::vector<std::string> list;
	auto configList = lua.GetTable(value);
	configList.IterateList([&list](int index, std::string str)
	{
		list.push_back(std::move(str));
	});
	return list;
}

// factory.define_command(products, inputs, arglist)
int
Interpreter::DefineCommand()
{
	Lua::View lua(luaState);

	Lua::Parameter productsArg("factory.define_command", "products", 1);
	Lua::Parameter inputsArg("factory.define_command", "inputs", 2);
	Lua::Parameter argListArg("factory.define_command", "argList", 3);
	Lua::Parameter tmpdirsArg("factory.define_command", "tmpdirs", 4);

	auto products = GetStringList(lua, productsArg);
	auto inputs = GetStringList(lua, inputsArg);
	auto argList = GetStringList(lua, argListArg);
	auto tmpdirs = GetStringList(lua, tmpdirsArg);

	commandFactory.AddCommand(products, inputs, std::move(argList), tmpdirs);

	return 0;
}

std::unique_ptr<ConfigNode>
Interpreter::SerializeConfig(Lua::View & lua, Lua::Table & config)
{
	ConfigNodeList list;
	ConfigPairMap map;

	config.Iterate(make_visitor(
		[&list](int key, int value)
		{
			list.emplace_back(std::make_unique<ConfigNode>(value));
		},
		[&list](int key, const char * value)
		{
			list.emplace_back(std::make_unique<ConfigNode>(value));
		},
		[&list,&lua](int key, Lua::Table & value)
		{
			list.emplace_back(SerializeConfig(lua, value));
		},
		[&map](const char * key, int value)
		{
			map.emplace(key, std::make_unique<ConfigNode>(value));
		},
		[&map](const char * key, const char * value)
		{
			map.emplace(key, std::make_unique<ConfigNode>(value));
		},
		[&map,&lua](const char * key, Lua::Table & value)
		{
			map.emplace(key, SerializeConfig(lua, value));
		}
	));

	if (!list.empty() && !map.empty()) {
		errx(1, "Unsupported mixture of table and list in %s",
		    config.GetNamedValue().ToString().c_str());
	}

	if (!map.empty()) {
		return std::make_unique<ConfigNode>(std::move(map));
	} else {
		// If both are empty we get an empty list
		return std::make_unique<ConfigNode>(std::move(list));
	}
}

int
Interpreter::Include(const char * funcName, IncludeFile::Type t)
{
	Lua::View lua(luaState);

	Lua::Parameter files(funcName, "files", 1);
	Lua::Parameter configArg(funcName, "config", 2);

	Lua::Table fileList = lua.GetTable(files);
	Lua::Table configTable = lua.GetTable(configArg);

	auto configUniq = SerializeConfig(lua, configTable);
	std::shared_ptr<ConfigNode> config(configUniq.release());

	fileList.IterateList([this, t, &config](int i, const char * path)
	{
		includeQueue.emplace_back(path, t, config);
	});

	return 0;
}

int
Interpreter::ErrorHandler(lua_State *lua)
{
	const char * error = lua_tostring(lua, -1);
	luaL_traceback(lua, lua, error, 1);
	errx(1, "error in lua script: %s", lua_tostring(lua, -1));
}
