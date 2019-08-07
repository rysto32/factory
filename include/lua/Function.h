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

#ifndef LUA_FUNCTION_H
#define LUA_FUNCTION_H

#include "lua/Util.h"
#include "lua/View.h"

#include "ConfigNode.h"
#include "Visitor.h"

#include <lua.hpp>

namespace Lua
{
class Function
{
private:
	/*
	 * A Lua::View represents a lua stack state at a particular moment in
	 * time, so we cannot hold one in a class that is designed to persist
	 * longer than when that remains valid.  For that reason, we hold a
	 * direct pointer to a lua_State here.
	 */
	lua_State *lua;
	int ref;

	Function(Lua::View &l, int index)
	  : lua(l),
	    ref(LUA_NOREF)
	{
		lua_pushvalue(lua, index);
		ref = luaL_ref(lua, LUA_REGISTRYINDEX);
	}

	Function(const Function&) = delete;
	Function & operator=(const Function &) = delete;

	friend class Table;

	template <typename T, typename... Args>
	void DoCall(int handlerIndex, int argCount, const T & type, const Args &... rest)
	{
		PushArg(type);
		DoCall(handlerIndex, argCount, rest...);
	}

	void DoCall(int handlerIndex, int argCount)
	{
		int result = lua_pcall(lua, argCount, 0, handlerIndex);
		if (result != LUA_OK) {
			errx(1, "Error running lua callback: %s", lua_tostring(lua, -1));
		}

	}

	void PushArg(const ConfigNode *node)
	{
		PushArg(*node);
	}

	void PushArg(const ConfigNode &node)
	{
		//Lua::View view(lua);
		const ConfigNode::ValueType & config = node.GetValue();
		std::visit(make_visitor(
			[this](int value)
			{
				lua_pushinteger(lua, value);
			},
			[this](const std::string & value)
			{
				lua_pushstring(lua, value.c_str());
			},
			[this](const ConfigNodeList & list)
			{
				lua_createtable(lua, list.size(), 0);
				for (size_t i = 0; i < list.size(); ++i) {
					PushArg(*list.at(i));

					// +1 because lua arrays are 1-based
					lua_seti(lua, -2, i + 1);
				}
			},
			[this](const ConfigPairMap & map)
			{
				lua_createtable(lua, 0, map.size());
				for (auto & [name, node] : map) {
					lua_pushstring(lua, name.c_str());
					PushArg(*node);
					lua_settable(lua, -3);
				}
			}
		), config);
	}

	int PushErrorHandler()
	{
		lua_pushcfunction(lua, ErrorHandler);
		return Lua::AbsoluteIndex(lua, -1);\
	}

	void PopErrorHandler()
	{
		lua_pop(lua, 1);
	}

	void PushFunctionRef()
	{
		lua_rawgeti(lua, LUA_REGISTRYINDEX, ref);
	}

	static int ErrorHandler(lua_State *lua)
	{
		const char * error = lua_tostring(lua, -1);
		luaL_traceback(lua, lua, error, 1);
		errx(1, "error in lua script: %s", lua_tostring(lua, -1));
	}

	void Release()
	{
		if (lua)
			luaL_unref(lua, LUA_REGISTRYINDEX, ref);
	}

public:
	Function()
	  : lua(nullptr),
	    ref(LUA_NOREF)
	{

	}

	Function(Function&&f) noexcept
	  : lua(f.lua),
	    ref(f.ref)
	{
		f.lua = nullptr;
		f.ref =  LUA_NOREF;
	}

	Function & operator=(Function && f) noexcept
	{
		Release();
		lua = f.lua;
		ref = f.ref;

		f.lua = nullptr;
		f.ref = LUA_NOREF;
		return *this;
	}

	~Function() noexcept
	{
		Release();
	}

	template <typename... Args>
	void Call(const Args &... args)
	{
		Lua::View view(lua);
		int handlerIndex = PushErrorHandler();
		PushFunctionRef()
;
		DoCall(handlerIndex, sizeof...(args), args...);

		PopErrorHandler();
	}

	template <typename T>
	void VarargsCall(const std::vector<T> & args)
	{
		Lua::View view(lua);
		int handlerIndex = PushErrorHandler();
		PushFunctionRef();

		for (const T & a : args) {
			PushArg(a);
		}

		DoCall(handlerIndex, args.size());

		PopErrorHandler();
	}
};
}

#endif

