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

#include "EventLoop.h"
#include "Job.h"
#include "JobCompletion.h"
#include "JobManager.h"
#include "MsgSocketServer.h"
#include "Permission.h"
#include "PermissionList.h"
#include "TempFileManager.h"
#include "TempFile.h"

#include <err.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string>
#include <vector>

class SimpleCompletion : public JobCompletion
{
private:
	EventLoop &loop;
public:
	SimpleCompletion(EventLoop & loop)
	  : loop(loop)
	{
	}

	void JobComplete(Job * job, int status) override
	{
		if (WIFEXITED(status)) {
			printf("PID %d (jid %d) exited with code %d\n",
			    job->GetPid(), job->GetJobId(), WEXITSTATUS(status));
		} else if(WIFSIGNALED(status)) {
			printf("PID %d (jid %d) terminated on signal %d\n",
			    job->GetPid(), job->GetJobId(), WTERMSIG(status));
		} else {
			printf("PID %d (jid %d) terminated on unknown code %d\n",
			    job->GetPid(), job->GetJobId(), status);
		}

		loop.SignalExit();
	}
};

static void
ParsePermission(const std::string & opt, PermissionList & perms)
{
	auto pos = opt.find_first_of(":");
	if (pos == std::string::npos) {
		perms.AddFilePermission(opt, Permission::READ);
		return;
	}

	Permission p = Permission::NONE;
	std::string path(opt.substr(0, pos));
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
		default:
			errx(1, "Unknown permission flag '%c' in '%s'", opt.at(pos), opt.c_str());
			break;
		}
		++pos;
	}

	perms.AddFilePermission(opt, p);
}

int main(int argc, char **argv)
{
	PermissionList perms;
	ArgList list;
	int ch;

	while ((ch = getopt(argc, argv, "a:")) != -1) {
		switch (ch) {
		case 'a':
			ParsePermission(optarg, perms);
			perms.AddFilePermission(optarg, Permission::READ);
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 2) {
		errx(1, "Usage: %s <prog> [args...]", argv[0]);
	}

	for (int i = 0; i < argc; ++i)
		list.push_back(argv[i]);

	EventLoop loop;
	TempFileManager tmpMgr;
	auto msgSock = tmpMgr.GetUnixSocket("msg_sock");
	if (!msgSock)
		err(1, "Failed to get msgsock");

	JobManager jobManager(loop, msgSock.get());
	MsgSocketServer server(std::move(msgSock), loop, jobManager);
	SimpleCompletion completer(loop);

	Job * job = jobManager.StartJob(perms, completer, list);
	if (job == NULL)
		err(1, "Failed to start job");

	loop.Run();

	return (0);
}
