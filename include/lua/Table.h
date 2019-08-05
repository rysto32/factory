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

#ifndef LUA_TABLE_H
#define LUA_TABLE_H

#include "lua/NamedValue.h"
#include "lua/View.h"

#include <cassert>
#include <err.h>
#include <functional>
#include <string>
#include <type_traits>

namespace Lua
{
class Table
{
private:
	Lua::View & lua;
	NamedValue value;
	int stackIndex;

	Table(Lua::View & l, const NamedValue & v)
	  : lua(l),
	    value(v),
	    stackIndex(l.absolute(v.GetStackIndex()))
	{

	}

	friend class View;

	template <typename F, typename IndexType>
	static constexpr bool takes_int_value()
	{
		return std::is_invocable_v<F, IndexType, int>;
	}

	template <typename F, typename IndexType>
	static constexpr bool takes_str_value()
	{
		return std::is_invocable_v<F, IndexType, const char *>;
	}

	template <typename F, typename IndexType>
	static constexpr bool takes_table_value()
	{
		return std::is_invocable_v<F, IndexType, Table&>;
	}

	template <typename F, typename IndexType>
	static constexpr bool callback_is_invocable()
	{
		return takes_int_value<F, IndexType>() ||
		   takes_str_value<F, IndexType>() ||
		    takes_table_value<F, IndexType>();
	}

	template <typename F, typename IndexType>
	void
	InvokeIntListFunc(const F & func, const NamedValue & subvalue, IndexType index, int stackPos)
	{
		if constexpr (takes_int_value<F, IndexType>())
			func(index, lua_tointeger(lua, stackPos));
		else
			errx(1, "Did not expect an int in %s", subvalue.ToString().c_str());
	}

	template <typename F, typename IndexType>
	void
	InvokeStrListFunc(const F & func, const NamedValue & subvalue, IndexType index, int stackPos)
	{
		if constexpr (takes_str_value<F, IndexType>())
			func(index, lua_tostring(lua, stackPos));
		else
			errx(1, "Did not expect a string in %s", subvalue.ToString().c_str());
	}

	template <typename F, typename IndexType>
	void
	InvokeTableListFunc(const F & func, const NamedValue & subvalue, IndexType index, int stackPos)
	{
		if constexpr (takes_table_value<F, IndexType>()) {
			View view(lua.GetLua());
			Table table(view, subvalue);
			func(index, table);
		} else {
			errx(1, "Did not expect a table in %s", subvalue.ToString().c_str());
		}
	}

	template <typename F, typename IndexType>
	void InvokeFunc(const F & func, IndexType index, int stackPos)
	{
		NamedValue subvalue(value, index, stackPos);
		if (lua_isinteger(lua, stackPos)) {
			InvokeIntListFunc(func, subvalue, index, stackPos);
		} else if (lua_isstring(lua, stackPos)) {
			InvokeStrListFunc(func, subvalue, index, stackPos);
		} else if (lua_istable(lua, stackPos)) {
			InvokeTableListFunc(func, subvalue, index, stackPos);
		} else {
			errx(1, "Invalid type in %s", subvalue.ToString().c_str());
		}
	}

	template <typename F>
	void InvokeListFunc(const F & func, int stackPos)
	{
		static_assert(callback_is_invocable<F, int>(),
		    "Callback function does not accept correct args");
		int keyPos = stackPos -  1;
		if (!lua_isinteger(lua, keyPos)) {
			errx(1, "Expected a list in %s", value.ToString().c_str());
		}

		InvokeFunc(func, lua_tointeger(lua, keyPos), stackPos);
	}

	template <typename F>
	void InvokeTableFunc(const F & func, int stackPos)
	{
		static_assert(callback_is_invocable<F, const char *>(),
		    "Callback function does not accept correct args");
		int keyPos = stackPos -  1;
		if (!lua_isstring(lua, keyPos)) {
			errx(1, "Expected a table in %s", value.ToString().c_str());
		}

		InvokeFunc(func, lua_tostring(lua, keyPos), stackPos);
	}

	template <typename F>
	void InvokeAnyFunc(const F & func, int stackPos)
	{
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
			InvokeFunc(func, lua_tointeger(lua, keyPos), stackPos);
		} else {
			assert (lua_isstring(lua, keyPos));
			InvokeFunc(func, lua_tostring(lua, keyPos), stackPos);
		}
	}

public:
	View & GetView()
	{
		return lua;
	}

	template <typename F>
	void IterateList(const F & func)
	{
		if (!lua_istable(lua, stackIndex))
			errx(1, "Expected a table in %s", value.ToString().c_str());

		lua_pushnil(lua);
		while (lua_next(lua, stackIndex) != 0) {
			InvokeListFunc(func, -1);
			lua_pop(lua, 1);
		}
	}

	template <typename F>
	void IterateTable(const F & func)
	{
		if (!lua_istable(lua, stackIndex))
			errx(1, "Expected a table in %s", value.ToString().c_str());

		lua_pushnil(lua);
		while (lua_next(lua, stackIndex) != 0) {
			InvokeTableFunc(func, -1);
			lua_pop(lua, 1);
		}
	}

	template <typename F>
	void Iterate(const F & func)
	{
		if (!lua_istable(lua, stackIndex))
			errx(1, "Expected a table in %s, got %s", value.ToString().c_str(), lua_typename(lua, stackIndex));

		lua_pushnil(lua);
		while (lua_next(lua, stackIndex) != 0) {
			InvokeAnyFunc(func, -1);
			lua_pop(lua, 1);
		}
	}

	const char * GetString(const char * name)
	{
		FetchValue(name);

		if (!lua_isstring(lua, -1))
			errx(1, "Field '%s' in %s is expected to be a string", name, value.ToString().c_str());
		const char * val = lua_tostring(lua, -1);
		lua_pop(lua, 1);
		return val;
	}

	void FetchValue(const char * name)
	{
		lua_pushstring(lua, name);
		lua_gettable(lua, stackIndex);

	}

	const NamedValue & GetNamedValue() const
	{
		return value;
	}
};
}

#endif

