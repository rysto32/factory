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
	assert (this->fd == fd);

	nvlist_t *msg = nvlist_recv(this->fd, NV_FLAG_IGNORE_CASE);

	// XXX should these err() calls be handled better?
	if (msg == NULL) {
		if (errno == ENOTCONN)
			return;
		err(1, "Got invalid message from fd %d (flags=%x)\n", this->fd, flags);
	}

	if (!nvlist_exists_number(msg, "type")) {
		nvlist_fdump(msg, stderr);
		errx(1, "Got message without type from fd %d\n", this->fd);
	}

	int t = nvlist_take_number(msg, "type");
	if (t >= MSG_TYPE_MAX)
		errx(1, "Got message of invalid type %d from fd %d", t, this->fd);

	if (t == MSG_TYPE_INIT) {
		if (job != NULL)
			errx(1, "Got duplicate init message from fd %d", this->fd);

		if (!nvlist_exists_number(msg, "jid"))
			errx(1, "Got init message without jid from fd %d", this->fd);

		uint64_t jobId = nvlist_take_number(msg, "jid");

		if (!nvlist_empty(msg)) {
			void *cookie;
			const char * first = nvlist_next(msg, NULL, &cookie);
			errx(1, "Got init message with unexpected field '%s' from fd %d",
			    first, this->fd);
		}

		job = server->CompleteSocket(this, jobId);
	} else {
		if (job == NULL)
			errx(1, "Got unexpected message type %d from fd %d", t, this->fd);

		job->HandleMessage(this, static_cast<MsgType>(t), msg);
	}

	nvlist_destroy(msg);
}

void
MsgSocket::Send(nvlist_t * msg)
{
	int error = nvlist_send(fd, msg);
	if (error != 0)
		err(1, "Could not send message to job");

	nvlist_destroy(msg);
}
