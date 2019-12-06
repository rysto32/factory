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

#include "lua/NamedValue.h"

#include "lua/Parameter.h"

#include <cassert>
#include <sstream>

namespace Lua
{
NamedValue::NamedValue(const Parameter & p)
	: parent(&p),
	  tableIndex(""),
	  stackIndex(p.GetIndex())
{
}

NamedValue::NamedValue(const NamedValue & p, const std::string & key, int stackIndex)
	: parent(&p),
	  tableIndex("." + key),
	  stackIndex(stackIndex)
{
}

NamedValue::NamedValue(const NamedValue & p, int key, int stackIndex)
	: parent(&p),
	  tableIndex(MakeIndexedString(key)),
	  stackIndex(stackIndex)
{
	// Try to detect reversed parameters
	assert (key > 0);
}

std::string
NamedValue::MakeIndexedString(int key)
{
	std::ostringstream sout;

	sout << "[" << key << "]";
	return sout.str();
}

std::string
NamedValue::ToString() const
{
	const auto * const* param = std::get_if<const Parameter *>(&parent);
	if (param) {
		return (*param)->ToString();
	}

	const auto * const* value = std::get_if<const NamedValue *>(&parent);
	assert (value);

	return (*value)->ToString() + tableIndex;
}
}
