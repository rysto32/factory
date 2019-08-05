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

#include <lua.hpp>

#include <cassert>
#include <memory>

#include <err.h>

namespace Lua
{

class NamedValue;
class Table;

class View
{
private:
	lua_State *lua;
	int startStackTop;

	friend class Lua::Table;

	View(lua_State *l)
	  : lua(l),
	    startStackTop(lua_gettop(lua))
	{

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

	bool isstring(int stackIndex)
	{
		return lua_isstring(lua, stackIndex);
	}

	bool isfunction(int index)
	{
		return lua_isfunction(lua, index);
	}

	const char * tostring(int stackIndex)
	{
		return lua_tostring(lua, stackIndex);
	}

	/*
	 * Conver an index to an equivalant absolute index that references the
	 * same object even as the stack is pushed or popped.
	 */
	int absolute(int relative)
	{
		return (relative > 0 || relative <= LUA_REGISTRYINDEX) ? relative
		    : lua_gettop(lua) + relative + 1;
	}

	int SaveToRegistry()
	{
		return luaL_ref(lua, LUA_REGISTRYINDEX);
	}
};
}

#endif

