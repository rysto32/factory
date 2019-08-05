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

#include <cstdlib>
#include <errno.h>
#include <err.h>
#include <sys/dnv.h>

Job::Job(const PermissionList &perm, JobCompletion &c, int id, pid_t pid)
  : perm(perm),
    completer(c),
    jobId(id),
    pid(pid)
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
Job::RegisterSocket(std::unique_ptr<MsgSocket> sock)
{
	sockets.push_back(std::move(sock));
}

void
Job::SendResponse(MsgSocket * sock, nvlist_t *resp, int error)
{
	nvlist_add_number(resp, "error", error);
	sock->Send(resp);
}

void
Job::HandleMessage(MsgSocket * sock, MsgType type, nvlist_t *msg)
{
	nvlist_t *resp = nvlist_create(NV_FLAG_IGNORE_CASE);
	if (msg == NULL)
		err(1, "Could not allocate nvlist message");

	if (!nvlist_exists_number(msg, "flags")) {
		SendResponse(sock, resp, EPROTO);
		return;
	}
	mode_t mode = nvlist_take_number(msg, "flags");

	auto path = std::unique_ptr<char[], decltype(&std::free)>(
		dnvlist_take_string(msg, "path", NULL), &std::free);
	if (path == NULL) {
		SendResponse(sock, resp, EPROTO);
		return;
	}

	if (!nvlist_empty(msg)) {
		SendResponse(sock, resp, EPROTO);
		return;
	}

	int permitted = perm.IsPermitted(path.get(), mode);
	if (permitted != 0) {
// 		fprintf(stderr, "Denied access to '%s'\n", path.get());
	}

	SendResponse(sock, resp, permitted);
}
