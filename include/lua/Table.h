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

#include "lua/View.h"

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
	const int stackIndex;

	Table(Lua::View & l, int relative)
	  : lua(l),
	    stackIndex(l.absolute(relative))
	{

	}

	friend class View;

	template <typename F, typename IndexType>
	void
	InvokeIntListFunc(const F & fun, IndexType index, int stackPos)
	{
		if constexpr (std::is_invocable<F, IndexType, int>::value)
			func(index, lua_tointeger(lua, stackPos));
		else
			errx(1, "Did not expect an int");
	}

	template <typename F, typename IndexType>
	void
	InvokeStrListFunc(const F & func, IndexType index, int stackPos)
	{
		if constexpr (std::is_invocable<F, IndexType, const char*>::value)
			func(index, lua_tostring(lua, stackPos));
		else
			errx(1, "Did not expect a string");
	}

	template <typename F, typename IndexType>
	void
	InvokeTableListFunc(const F & func, IndexType index, int stackPos)
	{
		if constexpr (std::is_invocable_v<F, IndexType, Table>) {
			Table table(lua, stackPos);
			func(index, table);
		} else {
			errx(1, "Did not expect a table");
		}
	}

	template <typename F, typename IndexType>
	void InvokeTableFunc(const F & func, IndexType index, int stackPos)
	{
		if (lua_isinteger(lua, stackPos)) {
			InvokeIntListFunc(func, index, stackPos);
		} else if (lua_isstring(lua, stackPos)) {
			InvokeStrListFunc(func, index, stackPos);
		} else if (lua_istable(lua, stackPos)) {
			InvokeTableListFunc(func, index, stackPos);
		} else {
			errx(1, "Invalid type");
		}
	}

	template <typename F>
	void InvokeListFunc(const F & func, int stackPos)
	{
		int keyPos = stackPos -  1;
		if (!lua_isinteger(lua, keyPos)) {
			errx(1, "Expected a list");
		}

		InvokeTableFunc(func, lua_tointeger(lua, keyPos), stackPos);
	}

	template <typename F>
	void InvokeTableFunc(const F & func, int stackPos)
	{
		int keyPos = stackPos -  1;
		if (!lua_isstring(lua, keyPos)) {
			errx(1, "Expected a list");
		}

		InvokeTableFunc(func, lua_tostring(lua, keyPos), stackPos);
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
			errx(1, "Expected a table");

		lua_pushnil(lua);
		while (lua_next(lua, stackIndex) != 0) {
			InvokeListFunc(func, -1);
			lua_pop(lua, 1);
		}
	}

	const char * GetString(const char * name)
	{
		FetchValue(name);

		if (!lua_isstring(lua, -1))
			errx(1, "Field '%s' is expected to be a string", name);
		const char * val = lua_tostring(lua, -1);
		lua_pop(lua, 1);
		return val;
	}

	void FetchValue(const char * name)
	{
		lua_pushstring(lua, name);
		lua_gettable(lua, stackIndex);

	}
};
}

#endif

