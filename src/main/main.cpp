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
#include "Interpreter.h"
#include "Job.h"
#include "JobManager.h"
#include "JobQueue.h"
#include "PreloadSandboxerFactory.h"
#include "Product.h"
#include "ProductManager.h"
#include "TempFileManager.h"
#include "TempFile.h"

#include <err.h>
#include <unistd.h>

#include <limits>
#include <sstream>
#include <string>
#include <vector>

std::unique_ptr<SandboxFactory>
GetSandboxerFactory(TempFileManager & tmpMgr, EventLoop &loop, int maxJobs)
{
	return std::make_unique<PreloadSandboxerFactory>(tmpMgr, loop, maxJobs);
}

class Main
{
private:
	EventLoop loop;
	TempFileManager tmpMgr;
	JobQueue jq;
	JobManager jobManager;
	ProductManager productMgr;
	CommandFactory commandFactory;
	Interpreter interp;

	void IncludeScript(Interpreter & interp, const IncludeFile & file);
	void IncludeConfig(Interpreter & interp, const IncludeFile & file);

public:
	Main(int maxJobs)
	  : jobManager(loop, jq, GetSandboxerFactory(tmpMgr, loop, maxJobs), maxJobs),
	    productMgr(jq),
	    commandFactory(productMgr),
	    interp(commandFactory)
	{
	}

	int Run();
};

void
Main::IncludeScript(Interpreter & interp, const IncludeFile & file)
{
	if (file.paths.size() != 1) {
		errx(1, "Cannot include multiple scripts at once.");
	}

	interp.RunFile(file.paths.front(), *file.config);
}

void
Main::IncludeConfig(Interpreter & interp, const IncludeFile & file)
{
	std::vector<ConfigNodePtr> configList;

	for (const std::string & path : file.paths) {
		ConfigParser parser(path);

		std::string errors;
		if (!parser.Parse(errors)) {
			errx(1, "Could not parse build definition %s: %s",
			    path.c_str(), errors.c_str());
		}

		configList.push_back(parser.TakeConfig());
	}
	interp.ProcessConfig(*file.config, configList);
}

int
Main::Run()
{
	interp.RunFile("/home/rstone/git/factory/src/lua_lib/basic.lua", ConfigNode(ConfigNodeList{}));
	interp.RunFile("factory.lua", ConfigNode(ConfigNodeList{}));

	while (true) {
		std::optional<IncludeFile> file = interp.GetNextInclude();
		if (!file.has_value())
			break;

		switch (file->type) {
			case IncludeFile::Type::SCRIPT:
				IncludeScript(interp, *file);
				break;
			case IncludeFile::Type::CONFIG:
				IncludeConfig(interp, *file);
				break;
		}
	}

	productMgr.SubmitLeafJobs();

	if (!jobManager.ScheduleJob()) {
		printf("No work to build target\n");
		return 0;
	}

	loop.Run();

	productMgr.CheckBlockedCommands();

	return (0);
}

/*
 * Because this is a global object, its destructor should be called for any
 * call to exit, ensuring that we clean up all resources (e.g. delete temp
 * files) on exit.
 */
std::unique_ptr<Main> mainObj;

int main(int argc, char **argv)
{
	char *endp;
	u_long maxJobs = 1;
	int ch;

	while ((ch = getopt(argc, argv, "j:")) != -1) {
		switch (ch) {
		case 'j':
			maxJobs = strtoul(optarg, &endp, 0);
			if (optarg[0] == '\0' || *endp != '\0' ||
			    maxJobs == 0 || maxJobs > std::numeric_limits<int>::max()) {
				errx(1, "-j <jobs> parameter must be a positive int");
			}
			break;
		}
	}

	mainObj = std::make_unique<Main>(maxJobs);
	return mainObj->Run();
}
