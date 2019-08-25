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

#ifndef LUA_VIEW_IMPL_H
#define LUA_VIEW_IMPL_H

#include "lua/Function.h"
#include "lua/NamedValue.h"
#include "lua/Table.h"

namespace Lua 
{

template <typename F, typename IndexType>
void
View::InvokeIntCallback(const F & func, const NamedValue & subvalue, IndexType index, int stackPos)
{
	if constexpr (takes_int_value<F, IndexType>())
		func(index, lua_tointeger(lua, stackPos));
	else
		throw InterpreterException("Did not expect an int in %s", subvalue.ToString().c_str());
}

template <typename F, typename IndexType>
void
View::InvokeStrCallback(const F & func, const NamedValue & subvalue, IndexType index, int stackPos)
{
	if constexpr (takes_str_value<F, IndexType>())
		func(index, lua_tostring(lua, stackPos));
	else
		throw InterpreterException("Did not expect a string in %s", subvalue.ToString().c_str());
}

template <typename F, typename IndexType>
void
View::InvokeTableCallback(const F & func, const NamedValue & subvalue, IndexType index, int stackPos)
{
	if constexpr (takes_table_value<F, IndexType>()) {
		Lua::View view(lua);
		func(index, Table(view, subvalue));
	} else {
		throw InterpreterException("Did not expect a table in %s", subvalue.ToString().c_str());
	}
}
template <typename F, typename IndexType>
void
View::InvokeFunctionCallback(const F & func, const NamedValue & subvalue, IndexType index, int stackPos)
{
	if constexpr (takes_func_value<F, IndexType>()) {
		func(index, Function(lua, stackPos));
	} else {
		throw InterpreterException("Did not expect a table in %s", subvalue.ToString().c_str());
	}
}

template <typename F, typename IndexType>
void 
View::InvokeCallback(const NamedValue & value, const F & func, IndexType index, int stackPos)
{
	NamedValue subvalue(value, index, stackPos);
	if (lua_isinteger(lua, stackPos)) {
		InvokeIntCallback(func, subvalue, index, stackPos);
	} else if (lua_isstring(lua, stackPos)) {
		InvokeStrCallback(func, subvalue, index, stackPos);
	} else if (lua_istable(lua, stackPos)) {
		InvokeTableCallback(func, subvalue, index, stackPos);
	} else if (lua_isfunction(lua, stackPos)) {
		InvokeFunctionCallback(func, subvalue, index, stackPos);
	} else {
		throw InterpreterException("Invalid type '%s' in %s",
		    lua_typename(lua, stackPos), subvalue.ToString().c_str());
	}
}
}

#endif
