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

#include "ConfigParser.h"

#include <err.h>

struct UclParserDeleter
{
	void operator()(struct ucl_parser *p)
	{
		ucl_parser_free(p);
	}
};

struct UclObjectDeleter
{
	void operator()(ucl_object_t *p)
	{
		ucl_object_unref(p);
	}
};

static auto
AddKeyValue()
{
	return [](ConfigPairMap & parent, const ucl_object_t *obj, ConfigNode::ValueType && value, std::string & errors)
	{
		const char *key = ucl_object_key(obj);
		auto child = std::make_unique<ConfigNode>(std::move(value));
		auto success = parent.emplace(std::string(key), std::move(child));
		if (!success.second) {
			errors = std::string("key ") + key + " repeated";
			return false;
		}
// 		fprintf(stderr, "Add key %s\n", key);
		return true;
	};
}

static auto
AppendList()
{
	return [](ConfigNodeList & parent, const ucl_object_t *obj, ConfigNode::ValueType && value, std::string & errors)
	{
		auto child = std::make_unique<ConfigNode>(std::move(value));
		parent.emplace_back(std::move(child));
		return true;
	};
}

bool
ConfigParser::Parse(std::string & errors)
{
	auto parser = std::unique_ptr<ucl_parser, UclParserDeleter>(ucl_parser_new(0));

	if (!parser) {
		errors = "Could not allocate parser";
		return false;
	}

	if (!ucl_parser_add_file(parser.get(), filename.c_str())) {
		errors = "Could not open file '" + filename + "' for reading";
		return false;
	}

	const char * errmsg = ucl_parser_get_error(parser.get());
	if (errmsg != nullptr) {
		errors = errmsg;
		return false;
	}

	ConfigPairMap pairs;
	auto obj = std::unique_ptr<ucl_object_t, UclObjectDeleter>(ucl_parser_get_object(parser.get()));
	if (ucl_object_type(obj.get()) != UCL_OBJECT) {
		errors = "Illegal top-level node (must be OBJECT)";
		return false;
	}

	bool success = WalkConfig(obj.get(), pairs, errors, AddKeyValue());
	if (!success)
		return false;

	top = std::make_unique<ConfigNode>(std::move(pairs));
	return true;
}

template <typename Container, typename AddNodeFunc>
bool
ConfigParser::WalkConfig(const ucl_object_t *parentObj, Container & parent, std::string & errors, const AddNodeFunc & AddNode)
{
	ucl_object_iter_t it = nullptr;
	const ucl_object_t *obj;

	while ((obj = ucl_iterate_object(parentObj, &it, true)) != nullptr) {
		switch (ucl_object_type(obj)) {
			case UCL_OBJECT:{
				ConfigPairMap pairs;

				bool success = WalkConfig(obj, pairs, errors, AddKeyValue());
				if (!success)
					return false;

				if (!AddNode(parent, obj, std::move(pairs), errors)) {
					return false;
				}
				break;
			}
			case UCL_ARRAY:  {
				ConfigNodeList list;

				bool success = WalkConfig(obj, list, errors, AppendList());
				if (!success)
					return false;

				if (!AddNode(parent, obj, std::move(list), errors)) {
					return false;
				}
				break;
			}
			case UCL_INT:
				if (!AddNode(parent, obj, ucl_object_toint(obj), errors)) {
					return false;
				}
				break;
			case UCL_STRING:
				if (!AddNode(parent, obj, ucl_object_tostring(obj), errors)) {
					return false;
				}
				break;
			default:
				// XXX
				errx(1, "Unhandled type %d", ucl_object_type(obj));
		}
	}

	return true;
}
