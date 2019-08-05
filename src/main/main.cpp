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

#include "CommandFactory.h"
#include "ConfigNode.h"
#include "ConfigParser.h"
#include "EventLoop.h"
#include "IngestManager.h"
#include "Interpreter.h"
#include "Job.h"
#include "JobCompletion.h"
#include "JobManager.h"
#include "JobQueue.h"
#include "MsgSocketServer.h"
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

int main(int argc, char **argv)
{
	EventLoop loop;
	TempFileManager tmpMgr;
	auto msgSock = tmpMgr.GetUnixSocket("msg_sock");
	if (!msgSock)
		err(1, "Failed to get msgsock");

	JobQueue jq;
	JobManager jobManager(loop, msgSock.get(), jq);
	MsgSocketServer server(std::move(msgSock), loop, jobManager);
	ProductManager productMgr(jq);
	CommandFactory commandFactory(productMgr);

	IngestManager ingestMgr;
	Interpreter interp(ingestMgr, commandFactory);

	interp.RunFile("/home/rstone/git/factory/src/lua_lib/basic.lua", ConfigNode(ConfigNodeList{}));
	interp.RunFile("factory.lua", ConfigNode(ConfigNodeList{}));

	while (true) {
		std::optional<IncludeFile> file = interp.GetNextInclude();
		if (!file.has_value())
			break;

		switch (file->type) {
			case IncludeFile::Type::SCRIPT: {
				interp.RunFile(file->path, *file->config);
				break;
			}
			case IncludeFile::Type::CONFIG: {
				ConfigParser parser(file->path);

				std::string errors;
				if (!parser.Parse(errors)) {
					errx(1, "Could not parse build definition: %s", errors.c_str());
				}

				const ConfigNode & config = parser.GetConfig();
				interp.ProcessConfig(*file->config, config);
				break;
			}
		}
	}

	productMgr.SubmitLeafJobs();

	if (!jobManager.ScheduleJob()) {
		printf("No work to build target\n");
		exit(0);
	}

	loop.Run();

	return (0);
}
