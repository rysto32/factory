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

#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include "ConfigNode.h"
#include "Path.h"
#include "VarMap.h"

#include <string>

#include "ucl.h"

class ConfigParser
{
	Path filename;
	ConfigNodePtr top;

	template <typename Container, typename AddNodeFunc>
	static bool WalkConfig(const ucl_object_t *obj, Container & node, std::string & errors, const AddNodeFunc & AddNode);

	static bool IfMacroHandler(const unsigned char *data, size_t len,
	    const ucl_object_t *arguments, void* ud);
	bool IfMacro(const unsigned char *data, size_t len,
	    const ucl_object_t *args, ucl_parser *parser);

	static bool AppendMacroHandler(const unsigned char *data, size_t len,
	    const ucl_object_t *arguments, const ucl_object_t *context, void* ud);
	bool AppendMacro(const unsigned char *data, size_t len,
	    const ucl_object_t *args, const ucl_object_t *context, ucl_parser *parser);

	static bool VariableHandler(const unsigned char *data, size_t len,
	    unsigned char **replace, size_t *replace_len, bool *need_free, void* ud);

	struct ParserContext
	{
		ConfigParser *parser;
		ucl_parser *ucl;
	};

public:
	explicit ConfigParser(const Path & file)
	  : filename(file)
	{
	}

	ConfigParser(const ConfigParser &) = delete;
	ConfigParser(ConfigParser &&) = delete;
	ConfigParser &operator=(const ConfigParser &) = delete;
	ConfigParser &operator=(ConfigParser &&) = delete;

	bool Parse(std::string & errors, const VarMap &);

	const ConfigNode & GetConfig() const
	{
		return *top;
	}

	ConfigNodePtr&& TakeConfig()
	{
		return std::move(top);
	}
};

#endif
