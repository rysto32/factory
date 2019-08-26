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
#include "InterpreterException.h"
#include "PermissionList.h"
#include "VariableExpander.h"
#include "VectorUtil.h"

#include "lua/Function.h"
#include "lua/NamedValue.h"
#include "lua/Parameter.h"
#include "lua/Table.h"
#include "lua/ValueParser.h"
#include "lua/View.h"

#include <cassert>
#include <memory>
#include <string>
#include <vector>

#include <err.h>

const char Interpreter::INTERPRETER_REGISTRY_ENTRY = 0;

const struct luaL_Reg Interpreter::factoryModule [] = {
	{"add_definitions", AddDefinitionsWrapper},
	{"define_command", DefineCommandWrapper},
	{ "evaluate_vars", EvaluateVarsWrapper},
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
Interpreter::AddStringValuePair(const char * name, const char * value)
{
	lua_State *lua = luaState.get();

	lua_pushstring(lua, name);
	lua_pushstring(lua, value);
	lua_settable(lua, -3);
}

void
Interpreter::ProcessConfig(const ConfigNode & parentConfig, const std::vector<ConfigNodePtr> & configList)
{
	if (configList.size() == 1) {
		ProcessSingleConfig(parentConfig, *configList.front());
	} else {
		ProcessMultiConfig(parentConfig, configList);
	}
}

void
Interpreter::ProcessMultiConfig(const ConfigNode & parentConfig, const std::vector<ConfigNodePtr> & configList)
{
	std::vector<std::string> argTypes;
	std::vector<const ConfigNode*> args;
	args.push_back(&parentConfig);

	for (const ConfigNodePtr & node : configList) {
		const ConfigNode::ValueType & config = node->GetValue();
		const auto & pairs = std::get<ConfigPairMap>(config);

		if (pairs.size() != 1) {
			errx(1, "Only one config key allowed in a multiinclude");
		}

		argTypes.push_back(pairs.begin()->first);
		args.push_back(pairs.begin()->second.get());
	}

	Lua::Function * func = ingestMgr.GetIngest(argTypes);
	if (!func) {
		warnx("No ingest for config '%s'", VectorToString(argTypes).c_str());
	}

	func->VarargsCall(args);
}

void
Interpreter::ProcessSingleConfig(const ConfigNode & parentConfig, const ConfigNode & node)
{
	const auto & config = node.GetValue();

	const auto & pairs = std::get<ConfigPairMap>(config);

	for (const auto & [name, value] : pairs) {
		Lua::Function * func = ingestMgr.GetIngest({name});
		if (!func) {
			warnx("No ingest for config '%s'", name.c_str());
			continue;
		}

		func->Call(parentConfig, *value);
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

template <typename F>
int
Interpreter::FuncImplementationWrapper(lua_State *lua, const F & implementation)
{
	try {
		return implementation();
	} catch (InterpreterException & e)
	{
		luaL_traceback(lua, lua, e.what(), 1);
		errx(1, "error in lua script: %s", lua_tostring(lua, -1));
	}
}

int
Interpreter::AddDefinitionsWrapper(lua_State *lua)
{
	return FuncImplementationWrapper(lua,
	    [lua]()
	    {
		return GetInterpreter(lua)->AddDefinitions();
	    });
}

int
Interpreter::DefineCommandWrapper(lua_State *lua)
{
	return FuncImplementationWrapper(lua,
	    [lua]()
	    {
		return GetInterpreter(lua)->DefineCommand();
	    });
}

int
Interpreter::EvaluateVarsWrapper(lua_State *lua)
{
	return FuncImplementationWrapper(lua,
	    [lua]()
	    {
		return GetInterpreter(lua)->EvaluateVars();
	    });
}

int
Interpreter::IncludeConfigWrapper(lua_State *lua)
{
	return FuncImplementationWrapper(lua,
	    [lua]()
	    {
		return GetInterpreter(lua)->Include("factory.include_config", IncludeFile::CONFIG);
	    });
}

int
Interpreter::IncludeScriptWrapper(lua_State *lua)
{
	return FuncImplementationWrapper(lua,
	    [lua]()
	    {
		return GetInterpreter(lua)->Include("factory.include_script", IncludeFile::SCRIPT);
	    });
}

auto
Interpreter::FunctionField(Lua::Function & func)
{
	return [&func](const std::string & name, Lua::Function && f)
		{
			func = std::move(f);
		};
}

template <typename T>
auto
Interpreter::StringField(T & str)
{
	return [&str](const std::string & name, std::string && value)
		{
			str = std::move(value);
		};
}

auto
Interpreter::StringListField(std::vector<std::string> & list)
{
	return make_visitor(
		[&list](const std::string & name, std::string && value)
		{
			list.emplace_back(std::move(value));
		},
		[&list](const std::string & name, Lua::Table && table)
		{
			list = GetStringList(table);
		}
	);
}

void
Interpreter::ParseDefinition(Lua::Table & def)
{
	std::vector<std::string> ingestedConfigs;
	Lua::Function callback;

	Lua::ValueParser parser {
		Lua::FieldSpec("name", StringListField(ingestedConfigs)),
		Lua::FieldSpec("process", FunctionField(callback))
	};

	def.ParseMap(parser);

	ingestMgr.AddIngest(std::move(ingestedConfigs), std::move(callback));
}

/* Implements function factory.add_definitions(defs) */
int
Interpreter::AddDefinitions()
{
	Lua::View lua(luaState);
	Lua::Parameter defs("factory.add_definitions", "defs", 1);


	auto table = lua.GetTable(defs);
	table.IterateList([this] (int index, Lua::Table && def)
	{
		ParseDefinition(def);
	});

	return 0;
}

std::vector<std::string>
Interpreter::GetStringList(Lua::View & lua, const Lua::NamedValue & value)
{
	Lua::Table table(lua.GetTable(value));
	return GetStringList(table);
}


std::vector<std::string>
Interpreter::GetStringList(Lua::Table & configList)
{
	std::vector<std::string> list;

	configList.IterateList([&list](int index, std::string && str)
	{
		list.push_back(std::move(str));
	});
	return list;
}

CommandOptions
Interpreter::GetCommandOptions(Lua::Table &table)
{
	CommandOptions opt;

	Lua::ValueParser parser {
		Lua::FieldSpec("tmpdirs", StringListField(opt.tmpdirs)).Optional(true),
		Lua::FieldSpec("workdir", StringField(opt.workdir)).Optional(true),
		Lua::FieldSpec("stdin", StringField(opt.stdin)).Optional(true),
		Lua::FieldSpec("stdout", StringField(opt.stdout)).Optional(true)
	};

	table.ParseMap(parser);

	return opt;
}

// factory.define_command(products, inputs, arglist)
int
Interpreter::DefineCommand()
{
	Lua::View lua(luaState);

	Lua::Parameter productsArg("factory.define_command", "products", 1);
	Lua::Parameter inputsArg("factory.define_command", "inputs", 2);
	Lua::Parameter argListArg("factory.define_command", "argList", 3);
	Lua::Parameter optionsArg("factory.define_command", "options", 4);

	auto products = GetStringList(lua, productsArg);
	auto inputs = GetStringList(lua, inputsArg);
	auto argList = GetStringList(lua, argListArg);

	auto optTable = lua.GetTable(optionsArg);
	CommandOptions options = GetCommandOptions(optTable);

	if (argList.empty()) {
		throw InterpreterException("In %s: cannot be empty", argListArg.ToString().c_str());
	}

	commandFactory.AddCommand(products, inputs, std::move(argList), std::move(options));

	return 0;
}

std::unique_ptr<ConfigNode>
Interpreter::SerializeConfig(Lua::Table & config)
{
	ConfigNodeList list;
	ConfigPairMap map;

	config.Iterate(make_visitor(
		[&list](int key, int64_t value)
		{
			list.emplace_back(std::make_unique<ConfigNode>(value));
		},
		[&list](int key, const char * value)
		{
			list.emplace_back(std::make_unique<ConfigNode>(value));
		},
		[&list](int key, Lua::Table && value)
		{
			list.emplace_back(SerializeConfig(value));
		},
		[&map](const char * key, int64_t value)
		{
			map.emplace(key, std::make_unique<ConfigNode>(value));
		},
		[&map](const char * key, const char * value)
		{
			map.emplace(key, std::make_unique<ConfigNode>(value));
		},
		[&map](const char * key, Lua::Table && value)
		{
			map.emplace(key, SerializeConfig(value));
		}
	));

	if (!list.empty() && !map.empty()) {
		throw InterpreterException("Unsupported mixture of table and list in %s",
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

	auto fileList = GetStringList(lua, files);

	Lua::Table configTable = lua.GetTable(configArg);
	auto config = SerializeConfig(configTable);

	includeQueue.emplace_back(std::move(fileList), t, std::move(config));

	return 0;
}

int
Interpreter::EvaluateVars()
{
	std::string output;
	{
		Lua::View lua(luaState);

		Lua::Parameter strArg("evaluate_vars", "str", 1);
		Lua::Parameter varsArg("evaluate_vars", "vars", 2);

		std::string_view str = lua.GetString(strArg);
		Lua::Table varsTable = lua.GetTable(varsArg);

		VariableExpander::VarMap vars;

		varsTable.IterateMap([&vars](const char * key, const char * value)
			{
				vars.emplace(key, value);
			});

		VariableExpander expander(std::move(vars));

		output = expander.ExpandVars(str);
	}

	lua_pushstring(luaState.get(), output.c_str());
	return 1;
}
