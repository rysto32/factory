
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

#include "ConfigNode.h"
#include "ConfigParser.h"
#include "EventLoop.h"
#include "Job.h"
#include "JobCompletion.h"
#include "JobManager.h"
#include "JobQueue.h"
#include "MsgSocketServer.h"
#include "PendingJob.h"
#include "Permission.h"
#include "PermissionList.h"
#include "Product.h"
#include "ProductManager.h"
#include "TempFileManager.h"
#include "TempFile.h"

#include <err.h>
#include <unistd.h>

#include <sstream>
#include <string>
#include <vector>

Product *
AddSourceFile(const std::string & name, const std::vector<std::string> cflags, ProductManager & productMgr, PermissionList & perms)
{
	std::ostringstream srcPath, objPath;

	srcPath << "/home/rstone/src/tcplat/" << name;

	Product *src = productMgr.GetProduct(srcPath.str());

	auto suffixIndex = name.find_last_of(".");

	objPath << "/tmp/tcplat/" << name.substr(0, suffixIndex) << ".o";

	Product *obj = productMgr.GetProduct(objPath.str());

	auto job = std::make_unique<PendingJob>(ProductList{obj},
	    ArgList{"/usr/local/bin/clang++80", "-c", "-O2", "-o", objPath.str(), srcPath.str()}, perms);
	obj->SetPendingJob(std::move(job));
	productMgr.AddDependency(obj, src);

	return obj;
}

std::string GetArchivePath(const std::string & libname)
{
	std::ostringstream arName;
	arName << "/tmp/tcplat/lib" << libname << ".a";

	return arName.str();
}

void GetStringList(std::vector<std::string> & list, const ConfigNode::ValueType & conf)
{
	if (std::holds_alternative<std::string>(conf)) {
		list.push_back(std::get<std::string>(conf));
		return;
	}

	auto confList = std::get_if<ConfigNodeList>(&conf);

	if (!confList) {
		errx(1, "Needed a list or a string");
	}

	for (const ConfigNodePtr & node : *confList) {
		const ConfigNode::ValueType & value = node->GetValue();

		if (!std::holds_alternative<std::string>(value)) {
			errx(1, "string list contains non-string");
		}

		list.emplace_back(std::get<std::string>(value));
	}
}

void GetLibList(std::vector<Product*> & list, const ConfigNode::ValueType & conf, ProductManager & productMgr)
{
	if (std::holds_alternative<std::string>(conf)) {
		std::string arName(GetArchivePath(std::get<std::string>(conf)));
		list.push_back(productMgr.GetProduct(arName));
		return;
	}

	auto confList = std::get_if<ConfigNodeList>(&conf);

	if (!confList) {
		errx(1, "Needed a list or a string");
	}

	for (const ConfigNodePtr & node : *confList) {
		const ConfigNode::ValueType & value = node->GetValue();

		if (!std::holds_alternative<std::string>(value)) {
			errx(1, "string list contains non-string");
		}

		std::string arName(GetArchivePath(std::get<std::string>(value)));
		list.push_back(productMgr.GetProduct(arName));
	}
}

void AddLibraryDef(const ConfigNode & node, ProductManager & productMgr, PermissionList & perms)
{
	std::string libname;
	std::vector<std::string> cflags;
	std::vector<std::string> srcs;

	const auto & config = node.GetValue();

	auto pairs = std::get_if<ConfigPairMap>(&config);
	if (!pairs) {
		errx(1, "library must be an object");
	}

	//const ConfigPairMap & pairs = *pairsPtr;
	for (const auto & [name, value] : *pairs) {
		const auto & valueConf = value->GetValue();
		if (name == "name") {
			if (!std::holds_alternative<std::string>(valueConf))
				errx(1, "library name must be a string");
			libname = std::get<std::string>(valueConf);
		} else if (name == "cflags") {
			GetStringList(cflags, valueConf);
		} else if (name == "srcs") {
			GetStringList(srcs, valueConf);
		}
	}

	std::vector<Product*> objList;
	for (const std::string & file : srcs) {
		objList.push_back(AddSourceFile(file, cflags, productMgr, perms));
	}

	std::string arName(GetArchivePath(libname));
	Product *archive = productMgr.GetProduct(arName);

	ArgList args{"/usr/bin/ar", "crs", arName};
	for (Product * o : objList) {
		args.push_back(o->GetPath());
	}
	auto job = std::make_unique<PendingJob>(ProductList{archive},
	    std::move(args), perms);
	archive->SetPendingJob(std::move(job));
	for (Product * o : objList) {
		productMgr.AddDependency(archive, o);
	}
}

void AddProgramDef(const ConfigNode & node, ProductManager & productMgr, PermissionList & perms)
{
	std::string path;
	std::vector<Product*> libList;
	std::vector<std::string> stdlibs;

	const auto & config = node.GetValue();

	auto pairs = std::get_if<ConfigPairMap>(&config);
	if (!pairs) {
		errx(1, "library must be an object");
	}

	//const ConfigPairMap & pairs = *pairsPtr;
	for (const auto & [name, value] : *pairs) {
		const auto & valueConf = value->GetValue();
		if (name == "path") {
			if (!std::holds_alternative<std::string>(valueConf))
				errx(1, "program path must be a string");
			path = std::get<std::string>(valueConf);
		} else if (name == "stdlibs") {
			GetStringList(stdlibs, valueConf);
		} else if (name == "libs") {
			GetLibList(libList, valueConf, productMgr);
		}
	}

	Product * exe = productMgr.GetProduct(path);

	ArgList args{"/usr/local/bin/clang++80"};
	for (Product * a : libList) {
		args.push_back(a->GetPath());
	}
	for (const std::string & lib : stdlibs) {
		args.push_back("-l" + lib);
	}
	args.push_back("-o");
	args.push_back(exe->GetPath());

	auto job = std::make_unique<PendingJob>(ProductList{exe}, std::move(args), perms);
	exe->SetPendingJob(std::move(job));

	for (Product * a : libList) {
		productMgr.AddDependency(exe, a);
	}
}

void FindRules(const ConfigNode & node, ProductManager & productMgr, PermissionList & perms)
{
	const auto & config = node.GetValue();

	const auto & pairs = std::get<ConfigPairMap>(config);

	for (const auto & [name, value] : pairs) {
		if (name == "library") {
			AddLibraryDef(*value, productMgr, perms);
		} else if (name == "program") {
			AddProgramDef(*value, productMgr, perms);
		} else {
			errx(1, "Unrecognized object name '%s'", name.c_str());
		}
	}
}

int main(int argc, char **argv)
{
	PermissionList perms;
	std::vector<Product*> objects;

	EventLoop loop;
	TempFileManager tmpMgr;
	auto msgSock = tmpMgr.GetUnixSocket("msg_sock");
	if (!msgSock)
		err(1, "Failed to get msgsock");

	JobQueue jq;
	JobManager jobManager(loop, msgSock.get(), jq);
	MsgSocketServer server(std::move(msgSock), loop, jobManager);
	ProductManager productMgr(jq);

	perms.AddDirPermission("/", Permission::READ | Permission::WRITE | Permission::EXEC);

	ConfigParser parser("/home/rstone/repos/factory/src/sample/build/build.ucl");

	std::string errors;
	if (!parser.Parse(errors)) {
		errx(1, "Could not parse build definition: %s", errors.c_str());
	}

	const ConfigNode & config = parser.GetConfig();
	FindRules(config, productMgr, perms);

	productMgr.SubmitLeafJobs();

	if (!jobManager.ScheduleJob()) {
		printf("No work to build target\n");
		exit(0);
	}

	loop.Run();

	return (0);
}
