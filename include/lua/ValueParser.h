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

#ifndef LUA_VALUE_PARSER_H
#define LUA_VALUE_PARSER_H

#include "lua/View.h"

#include <string>

namespace Lua
{
template <typename... Callbacks>
class ValueParser;

template <typename Callback>
class FieldSpec
{
	std::string_view name;
	Callback callback;
	bool optional;
	bool used;

	void Reset()
	{
		used = false;
	}

	template <typename...>
	friend class ValueParser;

public:
	FieldSpec(std::string_view && n, Callback &&c)
	  : name(n),
	    callback(c),
	    optional(false),
	    used(false)
	{
	}

	FieldSpec & Optional(bool val = true)
	{
		optional = val;
		return *this;
	}
};

template <typename... Callbacks>
class ValueParser
{
	std::tuple<FieldSpec<Callbacks>...> callbackList;

	template <typename F, typename... Rest>
	static void TryParser(Lua::View & lua, const NamedValue & value, const char * name,
	    int valuePos, FieldSpec<F> & cb, Rest &... rest)
	{
		if (cb.name == name) {
			cb.used = true;
			lua.InvokeMapFunc(value, cb.callback, valuePos);
			return;
		}

		TryParser(lua, value, name, valuePos, rest...);
	}

	static void TryParser(Lua::View & lua, const NamedValue & value, const char * name,
	    int valuePos)
	{
		errx(1, "In %s: unexpected field '%s'", value.ToString().c_str(), name);
	}

	template <typename F, typename... Rest>
	static void CheckRequired(const NamedValue & value, const FieldSpec<F> & cb, Rest... rest)
	{
		if (!cb.optional && !cb.used) {
			errx(1, "In %s: required field '%s' not specified", value.ToString().c_str(),
			     cb.name.data());
		}

		CheckRequired(value, rest...);
	}

	static void CheckRequired(const NamedValue & value)
	{
	}

public:
	explicit ValueParser(FieldSpec<Callbacks>... cb)
	  : callbackList(std::make_tuple(cb...))
	{
	}

	void Reset()
	{
		// This convoluted expression simply calls Reset() on
		// every FieldSpec in callbackList
		std::apply([](FieldSpec<Callbacks> & ... cb)
			{
				((cb.Reset()), ...);
			}, callbackList);
	}

	void Parse(Lua::View & lua, const NamedValue & value, const char * name, int valuePos)
	{
		std::apply([&lua,&value,name,valuePos](FieldSpec<Callbacks> &... cb)
		{
			TryParser(lua, value, name, valuePos, cb...);
		}, callbackList);
	}

	void CheckRequiredFields(const NamedValue & value)
	{
		std::apply([&value](FieldSpec<Callbacks> &... cb)
			{
				CheckRequired(value, cb...);
			}
			, callbackList);
	}
};
}

#endif
