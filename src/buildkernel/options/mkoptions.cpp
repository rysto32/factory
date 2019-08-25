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

#include "mkoptions/OptionMap.h"

#include "ConfigNode.h"
#include "ConfigParser.h"
#include "Visitor.h"

#include <err.h>
#include <stdio.h>

#include <list>
#include <memory>
#include <sstream>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <variant>
#include <vector>

struct OptionValue
{
	std::string name;
	std::variant<int, std::string> value;

	template <typename T>
	OptionValue(std::string && n, const T && v)
	  : name(std::move(n)),
	    value(std::move(v))
	{
	}
};

typedef std::list<OptionValue> OptionList;
typedef std::unordered_map<std::string, OptionList> HeaderContentMap;

std::string
MakeHeaderNameFromOption(const std::string & option)
{
	std::ostringstream sout;

	sout << "opt_";

	for (auto ch : option) {
		sout.put(std::tolower(ch));
	}

	sout << ".h";
	return sout.str();
}

const std::string *
GetEntryValue(const std::string & filename, const ConfigPairMap & map, const char * key)
{
	auto it = map.find(key);
	if (it == map.end()) {
		return nullptr;
	}

	const auto *value = std::get_if<std::string>(&it->second->GetValue());
	if (!value) {
		errx(1, "malformed options file '%s': expected '%s' entry to have string value",
		    filename.c_str(), key);
	}

	return value;

}

static void
ParseOptionEntry(const std::string & filename, const ConfigNode & entry, OptionMap & optionHeaders)
{
	const auto * map = std::get_if<ConfigPairMap>(&entry.GetValue());
	if (!map) {
		errx(1, "malformed options file '%s': expected entry to be an object", filename.c_str());
	}

	const std::string *option = GetEntryValue(filename, *map, "option");
	if (!option) {
		errx(1, "malformed options file '%s': expected entry to have 'option' key",
		    filename.c_str());
	}

	std::string header;
	const std::string *value = GetEntryValue(filename, *map, "header");
	if (value) {
		header = *value;
	} else {
		header = MakeHeaderNameFromOption(*option);
	}

	bool success = optionHeaders.AddHeader(*option, std::move(header));
	if (!success) {
		errx(1, "malformed options file '%s': option '%s' was previously defined",
		    filename.c_str(), option->c_str());
	}
}

static void
FindHeaders(const std::string & filename, const ConfigNode & node, OptionMap & optionHeaders)
{
	const auto & map = std::get<ConfigPairMap>(node.GetValue());

	if (map.size() != 1) {
		errx(1, "malformed options file '%s': expected 1 top-level key", filename.c_str());
	}

	const ConfigNodePtr & listNode = map.begin()->second;
	const auto * list = std::get_if<ConfigNodeList>(&listNode->GetValue());
	if (!list) {
		errx(1, "malformed options file '%s': expected a list", filename.c_str());
	}

	for (const ConfigNodePtr & entry : *list) {
		ParseOptionEntry(filename, *entry, optionHeaders);
	}
}

bool IsLower(const std::string & str)
{
	for (auto ch : str) {
		if (!std::islower(ch) && ch != '_') {
			return false;
		}
	}

	return true;
}

std::string
MakeDeviceOption(const std::string & option)
{
	std::ostringstream sout;

	sout << "DEV_";

	for (auto ch : option) {
		sout.put(std::toupper(ch));
	}

	return sout.str();
}

template <typename T>
static void
SetOption(const std::string & filename, std::string option,
    T value, HeaderContentMap & content, const OptionMap & optMap)
{
	const auto & optHeaders = optMap.GetOptionMap();

	auto oit = optHeaders.find(option);
	if (oit == optHeaders.end()) {
		// XXX hack.  If the option is all lower-case, it's likely a
		// device.  Try setting the corresponding DEV_ option for the
		// device.
		if (IsLower(option)) {
			SetOption(filename, MakeDeviceOption(option), value, content, optMap);
		}
		// Ignore; this option only controls files
		return;
	}

	auto cit = content.find(oit->second);
	assert (cit != content.end());
	cit->second.emplace_back(std::move(option), std::move(value));
}

static void
SetOption(const std::string & filename, std::string option,
    HeaderContentMap & content, const OptionMap & optHeaders)
{
	// Boolean options get set to 1
	SetOption(filename, option, 1, content, optHeaders);
}

static void
UnsetOption(const std::string & filename, std::string option,
    HeaderContentMap & content, const OptionMap & optMap)
{
	const auto & optHeaders = optMap.GetOptionMap();

	auto oit = optHeaders.find(option);
	if (oit == optHeaders.end()) {
		// XXX hack.  If the option is all lower-case, it's likely a
		// device.  Try setting the corresponding DEV_ option for the
		// device.
		if (IsLower(option)) {
			UnsetOption(filename, MakeDeviceOption(option), content, optMap);
		}
		// Ignore; this option only controls files
		return;
	}

	auto cit = content.find(oit->second);
	if (cit == content.end()) {
		// Option is already unset; ignore
		return;
	}

	OptionList & optlist = cit->second;
	auto lit = optlist.begin();
	while (lit != optlist.end()) {
		if (lit->name == option) {
			lit = optlist.erase(lit);
			break;
		} else {
			++lit;
		}
	}
}

static void
SetOptionBool(const std::string & filename, const std::string & option,
    bool value, HeaderContentMap & content, const OptionMap & optHeaders)
{
	if (value)
		SetOption(filename, option, content, optHeaders);
	else
		UnsetOption(filename, option, content, optHeaders);
}

static void
SetOptionFromObject(const std::string & filename, const ConfigPairMap & map,
    HeaderContentMap & content, const OptionMap & optHeaders)
{
	for (const auto & [o, node] : map) {
		// Have to re-declare this to appease the lambdas
		const std::string & option = o;
		std::visit(make_visitor(
			[&filename, &option, &content, &optHeaders](const std::string & value)
			{
				SetOption(filename, option, value, content, optHeaders);
			},
			[&filename, &option, &content, &optHeaders](int64_t value)
			{
				SetOption(filename, option, value, content, optHeaders);
			},
			[&filename, &option, &content, &optHeaders](bool value)
			{
				SetOptionBool(filename, option, value, content, optHeaders);
			},
			[&filename, &option](const auto & value)
			{
				errx(1, "Malformed kernconf file %s: option '%s' must set to a string, int or bool",
				     filename.c_str(), option.c_str());
			}
		), node->GetValue());
	}
}

static void
FillContent(const std::string & filename, const ConfigPairMap & map,
    HeaderContentMap & content, const OptionMap & optHeaders)
{
	auto it = map.find("options");
	if (it == map.end()) {
		return;
	}

	const ConfigNode::ValueType & optList = it->second->GetValue();
	const auto * list = std::get_if<ConfigNodeList>(&optList);
	if (!list) {
		errx(1, "Malformed kernconf file '%s': options must be a list", filename.c_str());
	}

	for (const ConfigNodePtr & optionNode : *list) {
		std::visit(make_visitor(
			[&filename, &content, &optHeaders](const std::string & option)
			{
				SetOption(filename, option, content, optHeaders);
			},
			[&filename, &content, &optHeaders](const ConfigPairMap & object)
			{
				SetOptionFromObject(filename, object, content, optHeaders);
			},
			[&filename](const auto &)
			{
				errx(1, "Malformed kernconf file '%s': option must be a string or an object", filename.c_str());
			}
		), optionNode->GetValue());
	}
}

static void
FillContentFromList(const std::string & filename, const ConfigNode & kernconf,
    HeaderContentMap & content, const OptionMap & optHeaders)
{
	const auto * map = std::get_if<ConfigPairMap>(&kernconf.GetValue());
	if (!map) {
		errx(1, "Malformed kernconf file '%s': top-level value must be an object", filename.c_str());
	}

	FillContent(filename, *map, content, optHeaders);
}

static HeaderContentMap
GetContent(const std::string & filename, const ConfigNode & top, const OptionMap & optHeaders)
{
	HeaderContentMap content;

	for (const std::string & header : optHeaders.GetHeaderList()) {
		content.emplace(header, OptionList());
	}

	const auto & map = std::get<ConfigPairMap>(top.GetValue());

	if (map.size() != 1) {
		errx(1, "malformed kernconf file '%s': expected 1 top-level key", filename.c_str());
	}

	const ConfigNodePtr & kernconf = map.begin()->second;
	std::visit(make_visitor(
		[&filename,&optHeaders,&content](const ConfigNodeList & list)
		{
			for (const ConfigNodePtr & c : list) {
				FillContentFromList(filename, *c, content, optHeaders);
			}
		},
		[&filename,&optHeaders,&content](const ConfigPairMap & map)
		{
			FillContent(filename, map, content, optHeaders);
		},
		[&filename](const auto &)
		{
			errx(1, "Malformed kernconf file '%s': top-level value must be an object", filename.c_str());
		}
	), kernconf->GetValue());

	return content;
}

static void
GenerateHeaders(const std::string outdir, const std::string & filename, const ConfigNode & top, const OptionMap & optHeaders)
{
	HeaderContentMap content = GetContent(filename, top, optHeaders);

	for (const auto &  [optfile, optlist] : content) {
		std::ostringstream optpath;
		optpath << outdir << "/" << optfile;
		FILE * fout = fopen(optpath.str().c_str(), "w");

		for (const OptionValue & opt : optlist) {
			fprintf(fout, "#define %s", opt.name.c_str());
			std::visit(make_visitor(
				[fout](const std::string & value)
				{
					fprintf(fout, " \"%s\"", value.c_str());
				},
				[fout](int value)
				{
					fprintf(fout, " %d", value);
				}
			), opt.value);
			fprintf(fout, "\n");
		}
	}
}

static void
usage(const char *progname)
{
	fprintf(stderr, "usage: %s -f <confFile> -O options -O options.arch -o outdir\n", progname);
}

int main(int argc, char **argv)
{
	int ch;
	std::string confFile;
	std::string outdir;
	std::vector<std::string> optionFiles;
	OptionMap optionHeaders;

	while ((ch = getopt(argc, argv, "f:o:O:")) != -1) {
		switch (ch) {
		case 'f':
			if (!confFile.empty()) {
				fprintf(stderr, "%s: -f cannot be used multiple times",
				    argv[0]);
				usage(argv[0]);
				exit(1);
			}
			confFile = optarg;
			break;
		case 'o':
			if (!outdir.empty()) {
				fprintf(stderr, "%s: -o cannot be used multiple times",
				    argv[0]);
				usage(argv[0]);
				exit(1);
			}
			outdir = optarg;
			break;
		case 'O':
			optionFiles.push_back(optarg);
			break;
		}
	}

	if (confFile.empty()) {
		fprintf(stderr, "%s: -f option is mandatory.\n", argv[0]);
		usage(argv[0]);
		exit(1);
	}

	if (outdir.empty()) {
		fprintf(stderr, "%s: -o option is mandatory.\n", argv[0]);
		usage(argv[0]);
		exit(1);
	}

	if (optionFiles.empty()) {
		fprintf(stderr, "%s: -O option is mandatory.\n", argv[0]);
		usage(argv[0]);
		exit(1);
	}

	ConfigParser confParser(confFile);
	std::string errors;
	if (!confParser.Parse(errors)) {
		fprintf(stderr, "Could not parse '%s': %s\n", confFile.c_str(), errors.c_str());
		exit(1);
	}

	for (const std::string & path : optionFiles) {
		ConfigParser optionParser(path);
		if (!optionParser.Parse(errors)) {
			fprintf(stderr, "Could not parse '%s': %s\n", path.c_str(), errors.c_str());
			exit(1);
		}

		FindHeaders(path, optionParser.GetConfig(), optionHeaders);
	}

	GenerateHeaders(outdir, confFile, confParser.GetConfig(), optionHeaders);
}
