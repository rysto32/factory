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

#include "MsgSocket.h"

#include "EventLoop.h"
#include "Job.h"
#include "MsgSocketServer.h"
#include "MsgType.h"

#include <assert.h>
#include <err.h>
#include <unistd.h>

MsgSocket::MsgSocket(int fd, MsgSocketServer *server, EventLoop &loop)
  : fd(fd),
    server(server),
    job(NULL)
{
	loop.RegisterSocket(this, fd);
}

MsgSocket::~MsgSocket()
{
	close(fd);
}

#include <stdio.h>

void
MsgSocket::Dispatch(int fd, short flags)
{
	SandboxMsg msg;

	assert (this->fd == fd);

	ssize_t bytes = recv(this->fd, &msg, sizeof(msg), 0);
	if (bytes == 0 && errno == EAGAIN) {
		return;
	}

	if (bytes != sizeof(msg)) {
		err(1, "recv() failed");
	}

	int t = msg.type;
	if (t >= MSG_TYPE_MAX)
		errx(1, "Got message of invalid type %d from fd %d", t, this->fd);

	if (t == MSG_TYPE_INIT) {
		job = server->CompleteSocket(this, msg.init.jid);
	} else {
		if (job == NULL)
			errx(1, "Got unexpected message type %d from fd %d", t, this->fd);

		job->HandleMessage(this, msg);
	}
}

void
MsgSocket::Send(const SandboxResp & msg)
{
	ssize_t bytes = send(fd, &msg, sizeof(msg), 0);
	if (bytes != sizeof(msg))
		err(1, "Failed to send message to job");
}
