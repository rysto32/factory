/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Ryan Stone
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

#include "CapsicumSandboxFactory.h"
#include "EventLoop.h"
#include "Job.h"
#include "JobCompletion.h"
#include "JobManager.h"
#include "JobQueue.h"
#include "MsgSocketServer.h"
#include "Permission.h"
#include "PermissionList.h"
#include "PreloadSandboxerFactory.h"
#include "TempFileManager.h"
#include "TempFile.h"

#include <err.h>
#include <libelf.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cassert>
#include <string>
#include <vector>

class SimpleCompletion : public JobCompletion
{
private:
	EventLoop &loop;
	int code;

public:
	SimpleCompletion(EventLoop & loop)
	  : loop(loop), code(0)
	{
	}

	void JobComplete(Job * job, int status) override
	{
		if (WIFEXITED(status)) {
			printf("PID %d (jid %d) exited with code %d\n",
			    job->GetPid(), job->GetJobId(), WEXITSTATUS(status));
			code = WEXITSTATUS(status);
		} else if(WIFSIGNALED(status)) {
			printf("PID %d (jid %d) terminated on signal %d\n",
			    job->GetPid(), job->GetJobId(), WTERMSIG(status));
			code = 1;
		} else {
			printf("PID %d (jid %d) terminated on unknown code %d\n",
			    job->GetPid(), job->GetJobId(), status);
			code = 1;
		}

		loop.SignalExit();
	}

	int GetExitCode() const
	{
		return code;
	}

	void Abort() override
	{
		assert(0);
	}
};

static Permission
ParsePermission(const std::string & opt, Path &path)
{
	auto pos = opt.find_first_of(':');
	if (pos == std::string::npos) {
		path = opt;
		return Permission::READ;
	}

	Permission p = Permission::NONE;
	path = opt.substr(0, pos);
	pos++;
	while (pos < opt.length()) {
		switch (opt.at(pos)) {
		case 'a':
			p |= Permission::READ | Permission::WRITE | Permission::EXEC;
			break;
		case 'r':
			p |= Permission::READ;
			break;
		case 'w':
			p |= Permission::WRITE;
			break;
		case 'x':
			p |= Permission::EXEC;
			break;
		case 's':
			p |= Permission::STAT;
			break;
		default:
			errx(1, "Unknown permission flag '%c' in '%s'", opt.at(pos), opt.c_str());
			break;
		}
		++pos;
	}

	return p;
}

void
AddPerm(PermissionList & perms, const Path & cwd, const Path & path, Permission p)
{

	if (path.is_relative()) {
		perms.AddPermission(cwd / path, p);
	} else {
		perms.AddPermission(path, p);
	}
}

int
main(int argc, char **argv)
{
	PermissionList perms;
	ArgList list;
	std::error_code code;
	std::unique_ptr<SandboxFactory> sandboxFactory;
	int ch;

	Path cwd(std::filesystem::current_path());
	Path work_dir(cwd);

	EventLoop loop;
	TempFileManager tmpMgr;
	JobQueue jobQueue;

	while ((ch = getopt(argc, argv, "a:C:Pr:w:x:")) != -1) {
		Path permPath;
		Permission p;

		switch (ch) {
		case 'a':
			p = ParsePermission(optarg, permPath);
			if (permPath.is_relative()) {
				permPath = cwd / permPath;
			}
			perms.AddPermission(permPath, p);
			break;
		case 'r':
			perms.AddPermission(optarg, Permission::READ);
			break;
		case 'w':
			perms.AddPermission(optarg, Permission::READ | Permission::WRITE);
			break;
		case 'x':
			perms.AddPermission(optarg, Permission::READ | Permission::EXEC);
			break;
		case 'C':
			work_dir = Path(optarg).canonical(code);
			break;
		case 'P':
			sandboxFactory =
			    std::make_unique<PreloadSandboxerFactory>(tmpMgr, loop, 1);
			    break;
		}
	}

	argc -= optind;
	argv += optind;

	if (elf_version(EV_CURRENT) == EV_NONE)
		errx(1, "ELF library initialization failed: %s",
		    elf_errmsg(-1));

	if (argc < 1) {
		errx(1, "Usage: %s <prog> [args...]", getprogname());
	}

	perms.AddPermission(argv[0], Permission::READ | Permission::EXEC);
	for (int i = 0; i < argc; ++i)
		list.push_back(argv[i]);

	if (!sandboxFactory) {
		sandboxFactory = std::make_unique<CapsicumSandboxFactory>();
	}

	JobManager jobManager(loop, jobQueue, std::move(sandboxFactory), 1);
	SimpleCompletion completer(loop);
	Command pending({}, std::move(list), std::move(perms), std::move(work_dir), {}, {});

	Job * job = jobManager.StartJob(pending, completer);
	if (job == NULL)
		err(1, "Failed to start job");

	loop.Run();

	return (completer.GetExitCode());
}
