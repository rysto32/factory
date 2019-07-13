
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
AddSourceFile(const std::string & name, JobQueue &jq, ProductManager & productManager, PermissionList & perms)
{
	std::ostringstream srcPath, objPath;

	srcPath << "/home/rstone/src/tcplat" << "/" << name;

	Product *src = productManager.GetProduct(srcPath.str());

	auto suffixIndex = name.find_last_of(".");

	objPath << "/tmp/factory/" << name.substr(0, suffixIndex) << ".o";

	Product *obj = productManager.GetProduct(objPath.str());

	auto job = std::make_unique<PendingJob>(ProductList{obj},
	    ArgList{"/usr/local/bin/g++", "-c", "-O2", "-o", objPath.str(), srcPath.str()}, perms);
	obj->SetPendingJob(std::move(job));
	productManager.AddDependency(obj, src);

	return obj;
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
	ProductManager productManager(jq);

	perms.AddDirPermission("/", Permission::READ | Permission::WRITE | Permission::EXEC);

	std::vector<std::string> srcList{
	    "HistoInfo.cpp",
	    "KernelController.cpp",
	    "MsgSocket.cpp",
	    "RequestClient.cpp",
	    "SlaveControlStrategy.cpp",
	    "SlaveServer.cpp",
	    "SocketThread.cpp",
	    "tcplat.cpp",
	    "TestMaster.cpp",
	    "TestSlave.cpp",
	    "Thread.cpp",
	    "UserController.cpp"};

	for (auto srcFile : srcList) {
		Product * object = AddSourceFile(srcFile, jq, productManager, perms);
		objects.push_back(object);
	}

	Product * exe = productManager.GetProduct("/tmp/factory/tcplat");

	ArgList args{"/usr/local/bin/g++"};
	for (Product * o : objects) {
		args.push_back(o->GetPath());
	}
	args.push_back("-lpthread");
	args.push_back("-o");
	args.push_back(exe->GetPath());

	auto job = std::make_unique<PendingJob>(ProductList{exe}, std::move(args), perms);
	exe->SetPendingJob(std::move(job));

	for (Product * o : objects) {
		productManager.AddDependency(exe, o);
	}

	productManager.SubmitLeafJobs();

	if (!jobManager.ScheduleJob()) {
		printf("No work to build target\n");
		exit(0);
	}

	loop.Run();

	return (0);
}
