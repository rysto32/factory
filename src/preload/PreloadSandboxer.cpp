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

#include "PreloadSandboxer.h"

#include "Command.h"
#include "JobSharedMemory.h"
#include "MsgSocket.h"
#include "MsgType.h"
#include "Path.h"
#include "SharedMem.h"

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

static char ld_preload[] = "LD_PRELOAD=" LIB_LOCATION;

PreloadSandboxer::PreloadSandboxer(uint64_t jobId, const Command & c, const TempFile *msgSock)
  : command(c),
    shm(msgSock, jobId)
{
	exec_fd = open(c.GetExecutable().c_str(), O_RDONLY | O_EXEC);
	if (exec_fd < 0) {
		err(1, "Could not open '%s' for exec", c.GetExecutable().c_str());
	}
}

PreloadSandboxer::~PreloadSandboxer()
{
	close(exec_fd);
}

int
PreloadSandboxer::GetExecFd()
{
	return exec_fd;
}

void
PreloadSandboxer::Enable()
{
	int fd = dup2(shm.GetFD(), SHARED_MEM_FD);
	if (fd < 0) {
		perror("Could not dup shm_fd");
		exit(1);
	}

	int error = fcntl(SHARED_MEM_FD, F_SETFD, 0);
	if (error < 0) {
		perror("Could not disable close-on-exec");
		exit(1);
	}

	for (int i = STDERR_FILENO + 1; i < SHARED_MEM_FD; ++i) {
		(void)close(i);
	}
	closefrom(SHARED_MEM_FD + 1);
}

void
PreloadSandboxer::EnvironAppend(std::vector<char*> & envp)
{

	envp.push_back(ld_preload);
}

void
PreloadSandboxer::RegisterSocket(std::unique_ptr<MsgSocket> sock)
{
	sockets.push_back(std::move(sock));
}

void
PreloadSandboxer::SendResponse(MsgSocket * sock, int error)
{
	SandboxResp resp;

	resp.type = MSG_TYPE_OPEN_REQUEST;
	resp.error = error;
	sock->Send(resp);
}

void
PreloadSandboxer::HandleMessage(MsgSocket * sock, const SandboxMsg & msg)
{
	Path path = Path(msg.open.path).lexically_normal();
	const PermissionList & perms = command.GetPermissions();
	const Path & workdir = command.GetWorkDir();

	int permitted = perms.IsPermitted(workdir, path, msg.open.flags & O_ACCMODE);
	if (permitted != 0) {
		fprintf(stderr, "Denied access to '%s' for %x\n", path.c_str(), msg.open.flags & O_ACCMODE);
	}

	SendResponse(sock, permitted);
}
