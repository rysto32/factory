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

typedef std::unique_ptr<ucl_object_t, UclObjectDeleter> UclObjPtr;

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
ConfigParser::Parse(std::string & errors, const VarMap &vars)
{
	auto parser = std::unique_ptr<ucl_parser, UclParserDeleter>(ucl_parser_new(UCL_PARSER_DEFAULT));

	if (!parser) {
		errors = "Could not allocate parser";
		return false;
	}

	ParserContext context;
	context.parser = this;
	context.ucl = parser.get();

	Path parent = filename.parent_path();
	
	UclObjPtr incPaths(ucl_object_typed_new(UCL_ARRAY));
	ucl_array_append(incPaths.get(), ucl_object_fromstring(parent.c_str()));

	// XXX a future libucl version can return an error from this function.
	ucl_parser_register_macro(parser.get(), "if", IfMacroHandler, &context);
	ucl_parser_register_context_macro(parser.get(), "append", AppendMacroHandler, &context);

	void * varsArg = const_cast<VarMap*>(&vars);
	ucl_parser_set_variables_handler(parser.get(), VariableHandler, varsArg);
	
	if (!ucl_set_include_path(parser.get(), incPaths.get())) {
		const char * errmsg = ucl_parser_get_error(parser.get());
		if (errmsg != nullptr) {
			errors = errmsg;
			return false;
		}
		
		errors = "Could not set " + parent.string() + " as parser include path";
		return false;
	}
	

	if (!ucl_parser_add_file_full(parser.get(), filename.c_str(), 0,
	    UCL_DUPLICATE_MERGE, UCL_PARSE_UCL)) {
		const char * errmsg = ucl_parser_get_error(parser.get());
		if (errmsg != nullptr) {
			errors = errmsg;
			return false;
		}

		errors = "Could not open file '" + filename.string() + "' for reading";
		return false;
	}

	ConfigPairMap pairs;
	UclObjPtr obj(ucl_parser_get_object(parser.get()));
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
			case UCL_BOOLEAN:
				if (!AddNode(parent, obj, ucl_object_toboolean(obj), errors)) {
					return false;
				}
				break;
			case UCL_STRING:
				if (!AddNode(parent, obj, std::string(ucl_object_tostring(obj)), errors)) {
					return false;
				}
				break;
			default:
				// XXX
				errx(1, "Unhandled type %s", ucl_object_type_to_string(ucl_object_type(obj)));
		}
	}

	return true;
}

bool
ConfigParser::IfMacroHandler(const unsigned char *data, size_t len,
    const ucl_object_t *arguments, void* ud)
{
	ParserContext *context = reinterpret_cast<ParserContext*>(ud);

	return context->parser->IfMacro(data, len, arguments, context->ucl);
}

bool
ConfigParser::IfMacro(const unsigned char *data, size_t len,
    const ucl_object_t *args, ucl_parser *parser)
{
	if (ucl_object_type(args) != UCL_ARRAY) {
		errx(1, ".if macro with %s args type",
		    ucl_object_type_to_string(ucl_object_type(args)));
	}

	if (ucl_array_size(args) != 1) {
		errx(1, ".if macro syntax error: too many arguments");
	}

// 	const char *str;
	const ucl_object_t *condObj = ucl_array_head(args);
	bool cond;

	switch (ucl_object_type(condObj)) {
		case UCL_INT:
			fprintf(stderr, ".if macro: cond == %ld\n", ucl_object_toint(condObj));
			cond = ucl_object_toint(condObj) != 0;
			break;
		case UCL_BOOLEAN:
			fprintf(stderr, ".if macro: cond == %d\n", ucl_object_toboolean(condObj));
			cond = ucl_object_toboolean(condObj);
			break;
		case UCL_STRING:
			errx(1, ".if macro syntax error: string in condition: '%s'", ucl_object_tostring(condObj));
// 			/*str = ucl_object_tostring(condObj);
// 			if (strcmp(str, "1") == 0 || strcasecmp(str, "true") == 0) {
// 				cond = true;
// 			} else if (strcmp(str, "0") == 0 || strcasecmp(str, "false") == 0) {
// 				cond = false;
// 			} else {
// 				errx(1, ".if macro syntax error: unexpected bool value '%s'", str);
// 			}*/
// 			fprintf(stderr, ".if macro: cond == '%s'\n", ucl_object_tostring(condObj));
// 			cond = true;
// 			break;
		default:
			errx(1, ".if macro syntax error: Unhandled condition type %s", ucl_object_type_to_string(ucl_object_type(condObj)));
	}

	if (!cond) {
		return true;
	}

	ucl_parser_insert_chunk(parser, data, len);
	return true;
}

bool
ConfigParser::AppendMacroHandler(const unsigned char *data, size_t len,
    const ucl_object_t *arguments, const ucl_object_t *obj, void* ud)
{
	ParserContext *context = reinterpret_cast<ParserContext*>(ud);

	return context->parser->AppendMacro(data, len, arguments, obj, context->ucl);
}

bool
ConfigParser::AppendMacro(const unsigned char *data, size_t len,
    const ucl_object_t *args, const ucl_object_t *context, ucl_parser *parser)
{
	fprintf(stderr, "Type of context is %s; got %zd bytes\n", ucl_object_type_to_string(ucl_object_type(context)), len);
	return true;
}

bool
ConfigParser::VariableHandler(const unsigned char *data, size_t len,
	    unsigned char **replace, size_t *replace_len, bool *need_free, void* ud)
{
	const VarMap & vars = *reinterpret_cast<const VarMap*>(ud);

	std::string name((const char*)data, len);
	fprintf(stderr, "eval variable '%s'\n", name.c_str());

	auto it = vars.find(name);
	if (it == vars.end()) {
		return false;
	}

	fprintf(stderr, "expand to '%s'\n", it->second.c_str());

	*replace = (unsigned char*)(it->second.c_str());
	*replace_len = it->second.size();
	*need_free = false;
	return true;
}
