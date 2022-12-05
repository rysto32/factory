
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

#ifndef CONFIG_NODE_H
#define CONFIG_NODE_H

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

class ConfigNode;

typedef std::unique_ptr<ConfigNode> ConfigNodePtr;
typedef std::vector<ConfigNodePtr> ConfigNodeList;
typedef std::unordered_map<std::string, ConfigNodePtr> ConfigPairMap;

class ConfigNode
{
public:
	typedef std::variant<int64_t, bool, std::string, ConfigNodeList, ConfigPairMap> ValueType;

private:

	ValueType value;

	/*
	 * A cache of this node, converted to a string.  We cache it for two
	 * reasons:
	 * 1. Performance in case this node is evaluated multiple times.
	 * 2. So that our caller can depend on the returned string not being
	 *    freed.  This will be invoked from code that interacts with C
	 *    libraries so memory management here is awkward.
	 */
	mutable std::optional<std::string> stringForm;

public:
	ConfigNode(ValueType && v)
	  : value(std::move(v))
	{

	}

	ConfigNode(const char * s)
	  : value(std::string(s))
	{
	}

	ConfigNode(const ConfigNode &) = delete;
	ConfigNode(ConfigNode &&) = default;

	ConfigNode & operator=(const ConfigNode &) = delete;
	ConfigNode & operator=(ConfigNode &&) = delete;

	const ValueType & GetValue() const
	{
		return value;
	}

	const std::string_view EvalAsString() const;
};

#endif

