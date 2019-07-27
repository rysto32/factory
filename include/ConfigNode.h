
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
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

class ConfigNode;

typedef std::unique_ptr<ConfigNode> ConfigNodePtr;
typedef std::vector<ConfigNodePtr> ConfigNodeList;

class ConfigNode
{
	typedef std::variant<int, std::string, ConfigNodeList, ConfigNodePtr> ValueType;
	typedef std::unordered_map<std::string, ValueType> PairMap;

	PairMap pairs;

public:
	ConfigNode() = default;
	ConfigNode(const ConfigNode &) = delete;
	ConfigNode(ConfigNode &&) = default;

	ConfigNode & operator=(const ConfigNode &) = delete;
	ConfigNode & operator=(ConfigNode &&) = delete;

	bool AddPair(std::string && name, ValueType && v)
	{
		auto success = pairs.emplace(std::move(name), std::move(v));
		return success.second;
	}
};

#endif

