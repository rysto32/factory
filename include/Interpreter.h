
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

#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "ConfigNode.h"
#include "IngestManager.h"

#include <deque>
#include <lua.hpp>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

struct PermissionConf;
class CommandFactory;
struct CommandOptions;
class IngestManager;
class PermissionList;

namespace Lua
{
	class Function;
	class NamedValue;
	class Table;
	class View;
}

struct IncludeFile
{
	enum Type { CONFIG, SCRIPT };

	std::vector<std::string> paths;
	const Type type;
	std::unique_ptr<ConfigNode> config;

	IncludeFile(const std::vector<std::string> && p, Type t, std::unique_ptr<ConfigNode> && c)
	  : paths(p),
	    type(t),
	    config(std::move(c))
	{
	}

	IncludeFile(IncludeFile &&) = default;
};

class Interpreter
{
	class LuaStateFree
	{
	public:
		void operator()(lua_State *);
	};

	class ConfigVisitor
	{
		Interpreter &interp;
		ConfigVisitor(Interpreter & i)
		  : interp(i)
		{
		}

		friend class Interpreter;

	public:
		void operator()(int value) const;
		void operator()(const std::string & value) const;
		void operator()(const ConfigNodeList & value) const;
		void operator()(const ConfigPairMap & value) const;
	};

	friend class ConfigVisitor;

	typedef std::unique_ptr<lua_State, LuaStateFree> LuaStatePtr;
	LuaStatePtr luaState;
	IngestManager ingestMgr;
	CommandFactory & commandFactory;
	std::deque<IncludeFile> includeQueue;

	void RegisterModules();

	static const struct luaL_Reg factoryModule [];
	static const char INTERPRETER_REGISTRY_ENTRY;

	static Interpreter * GetInterpreter(lua_State *);
	static int AddDefinitionsWrapper(lua_State *);
	static int DefineCommandWrapper(lua_State *);
	static int EvaluateVarsWrapper(lua_State *);
	static int IncludeConfigWrapper(lua_State *);
	static int IncludeScriptWrapper(lua_State *);
	static int RealpathWrapper(lua_State *);

	static int ErrorHandler(lua_State *);

	int AddDefinitions();
	int DefineCommand();
	int EvaluateVars();
	template <IncludeFile::Type type>
	int Include();
	int Realpath();

	std::string_view GetIncludeFuncName(IncludeFile::Type type);

	void PushConfig(const ConfigNode & node);

	void ParseDefinition(Lua::Table & def);

	void AddStringValuePair(const char * name, const char * value);

	static std::vector<std::string> GetStringList(Lua::View &, const Lua::NamedValue & value);
	static std::vector<std::string> GetStringList(Lua::Table & configList);

	static std::unique_ptr<ConfigNode> SerializeConfig(Lua::Table & config);

	void ProcessSingleConfig(const ConfigNode & parentConfig, const ConfigNode & node);
	void ProcessMultiConfig(const ConfigNode & parent, const std::vector<ConfigNodePtr> & config);
	
	CommandOptions GetCommandOptions(Lua::Table &);

	auto FunctionField(Lua::Function & func);
	auto StringListField(std::vector<std::string> & list);

	template <typename T>
	auto StringField(T &);

	typedef int (Interpreter::*LuaFuncImpl)();

	template <LuaFuncImpl F>
	static int FuncImplWrapper(lua_State *lua);

public:
	Interpreter(CommandFactory &);
	~Interpreter();

	Interpreter(const Interpreter &) = delete;
	Interpreter(Interpreter &&) = delete;
	Interpreter &operator=(const Interpreter&) = delete;
	Interpreter &operator=(Interpreter &&) = delete;

	void RunFile(const std::string & path, const ConfigNode & config);
	void ProcessConfig(const ConfigNode & parent, const std::vector<ConfigNodePtr> & config);

	std::optional<IncludeFile> GetNextInclude();
};

#endif

