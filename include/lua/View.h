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

#ifndef LUA_VIEW_H
#define LUA_VIEW_H

#include "lua/NamedValue.h"
#include "lua/Util.h"

#include "InterpreterException.h"

#include <lua.hpp>

#include <cassert>
#include <memory>

#include <err.h>

namespace Lua
{

class Function;
class NamedValue;
class Table;
template <typename... F>
class ValueParser;

class View
{
private:
	lua_State *lua;
	int startStackTop;

	friend class Lua::Function;
	friend class Lua::Table;
	template <typename...>
	friend class Lua::ValueParser;

	View(lua_State *l)
	  : lua(l),
	    startStackTop(lua_gettop(lua))
	{

	}
	
	Lua::Table MakeTable(const NamedValue & value);

	template <typename F, typename IndexType>
	static constexpr bool takes_int_value()
	{
		return std::is_invocable_v<F, IndexType, int64_t>;
	}

	template <typename F, typename IndexType>
	static constexpr bool takes_str_value()
	{
		return std::is_invocable_v<F, IndexType, const char *>;
	}

	template <typename F, typename IndexType>
	static constexpr bool takes_table_value()
	{
		return std::is_invocable_v<F, IndexType, Table&&>;
	}

	template <typename F, typename IndexType>
	static constexpr bool takes_func_value()
	{
		return std::is_invocable_v<F, IndexType, Function&&>;
	}

	template <typename F, typename IndexType>
	static constexpr bool callback_is_invocable()
	{
		return takes_int_value<F, IndexType>() ||
		    takes_str_value<F, IndexType>() ||
		    takes_table_value<F, IndexType>() ||
		    takes_func_value<F, IndexType>();
	}

	template <typename F, typename IndexType>
	void InvokeIntCallback(const F & func, const NamedValue & subvalue, IndexType index, int stackPos);

	template <typename F, typename IndexType>
	void InvokeStrCallback(const F & func, const NamedValue & subvalue, IndexType index, int stackPos);

	template <typename F, typename IndexType>
	void InvokeTableCallback(const F & func, const NamedValue & subvalue, IndexType index, int stackPos);

	template <typename F, typename IndexType>
	void InvokeFunctionCallback(const F & func, const NamedValue & subvalue, IndexType index, int stackPos);

	template <typename F, typename IndexType>
	void InvokeCallback(const NamedValue & value, const F & func, IndexType index, int stackPos);

	template <typename F>
	void InvokeListFunc(const NamedValue & value, const F & func, int stackPos)
	{
		static_assert(callback_is_invocable<F, int>(),
		    "Callback function does not accept correct args");
		int keyPos = stackPos -  1;
		if (!lua_isinteger(lua, keyPos)) {
			throw InterpreterException("Expected a list in %s", value.ToString().c_str());
		}

		InvokeCallback(value, func, lua_tointeger(lua, keyPos), stackPos);
	}

	template <typename F>
	void InvokeMapFunc(const NamedValue & value, const F & func, int stackPos)
	{
		static_assert(callback_is_invocable<F, const char *>(),
		    "Callback function does not accept correct args");
		int keyPos = stackPos -  1;
		if (lua_isinteger(lua, keyPos)) {
			throw InterpreterException("Expected a map in %s", value.ToString().c_str());
		}

		InvokeCallback(value, func, lua_tostring(lua, keyPos), stackPos);
	}

	template <typename F>
	void InvokeAnyFunc(const NamedValue & value, const F & func, int stackPos)
	{
		/* Sanity test that F can handle both lists and tables. */
		static_assert(!(!callback_is_invocable<F, int>() &&
		    callback_is_invocable<F, const char *>()),
		    "Callback function must accept int indices");
		static_assert(!(callback_is_invocable<F, int>() &&
		    !callback_is_invocable<F, const char *>()),
		    "Callback function must accept const char * indices");

		static_assert(callback_is_invocable<F, int>() &&
		    callback_is_invocable<F, const char *>(),
		    "Callback function does not accept correct args");

		int keyPos = stackPos -  1;
		if (lua_isinteger(lua, keyPos)) {
			InvokeCallback(value, func, lua_tointeger(lua, keyPos), stackPos);
		} else {
			assert (lua_isstring(lua, keyPos));
			InvokeCallback(value, func, lua_tostring(lua, keyPos), stackPos);
		}
	}

public:
	template <typename Free>
	View(const std::unique_ptr<lua_State, Free> & l)
	  : lua(l.get()),
	    startStackTop(lua_gettop(lua))
	{

	}

	~View()
	{
		assert (lua_gettop(lua) == startStackTop);
	}

	View(View &&) = default;
	View(const View*) = delete;

	View & operator=(View &&) = delete;
	View & operator=(const View &) = delete;

	operator lua_State*()
	{
		return lua;
	}

	lua_State* GetLua() const
	{
		return lua;
	}

	Table GetTable(const NamedValue & index);
	std::string_view GetString(const NamedValue & value);

	/*
	 * Conver an index to an equivalant absolute index that references the
	 * same object even as the stack is pushed or popped.
	 */
	int absolute(int relative)
	{
		return AbsoluteIndex(lua, relative);
	}

	int SaveToRegistry()
	{
		return luaL_ref(lua, LUA_REGISTRYINDEX);
	}
};
}

#endif

