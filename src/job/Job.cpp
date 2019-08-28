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

#include "Job.h"

#include "JobCompletion.h"
#include "MsgSocket.h"
#include "PermissionList.h"

#include <sys/types.h>
#include <sys/wait.h>

#include <cstdlib>
#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <signal.h>

Job::Job(const PermissionList &perm, JobCompletion &c, int id, pid_t pid, Path wd)
  : perm(perm),
    completer(c),
    jobId(id),
    pid(pid),
    workdir(std::move(wd))
{
}

Job::~Job()
{
}

void
Job::Complete(int status)
{
	completer.JobComplete(this, status);
}

void
Job::Abort()
{
	int status;

	kill(-pid, SIGTERM);
	waitpid(pid, &status, 0);
	completer.Abort();
}

void
Job::RegisterSocket(std::unique_ptr<MsgSocket> sock)
{
	sockets.push_back(std::move(sock));
}

void
Job::SendResponse(MsgSocket * sock, int error)
{
	SandboxResp resp;

	resp.type = MSG_TYPE_OPEN_REQUEST;
	resp.error = error;
	sock->Send(resp);
}

void
Job::HandleMessage(MsgSocket * sock, const SandboxMsg & msg)
{
	Path path = Path(msg.open.path).lexically_normal();
	int permitted = perm.IsPermitted(workdir, path, msg.open.flags & O_ACCMODE);
	if (permitted != 0) {
		fprintf(stderr, "Denied access to '%s' for %x\n", path.c_str(), msg.open.flags & O_ACCMODE);
	}

	SendResponse(sock, permitted);
}
