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

#include "lua/Function.h"
#include "lua/NamedValue.h"
#include "lua/ValueParser.h"
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
			lua.InvokeListFunc(value, func, -1);
			lua_pop(lua, 1);
		}
	}

	template <typename F>
	void IterateMap(const F & func)
	{
		if (!lua_istable(lua, stackIndex))
			errx(1, "Expected a table in %s", value.ToString().c_str());

		lua_pushnil(lua);
		while (lua_next(lua, stackIndex) != 0) {
			lua.InvokeMapFunc(value, func, -1);
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
			lua.InvokeAnyFunc(value, func, -1);
			lua_pop(lua, 1);
		}
	}
	
	template <typename... C>
	void ParseMap(ValueParser<C...> & parser)
	{
		if (!lua_istable(lua, stackIndex))
			errx(1, "Expected a table in %s, got %s", value.ToString().c_str(),
			     lua_typename(lua, stackIndex));

		parser.Reset();

		lua_pushnil(lua);
		while (lua_next(lua, stackIndex) != 0) {
			if (lua_isinteger(lua, -2))
				errx(1, "Expected a map in %s", value.ToString().c_str());

			parser.Parse(lua, value, lua_tostring(lua, -2), -1);
			lua_pop(lua, 1);
		}

		parser.CheckRequiredFields(value);
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

#include "lua/ViewImpl.h"

#endif

