// Copyright (c) 2018 Ryan Stone.  All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.

#include "Job.h"

#include "JobCompletion.h"
#include "MsgSocket.h"
#include "PermissionList.h"

#include <cstdlib>
#include <errno.h>
#include <err.h>

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
Job::SendResponse(MsgSocket * sock, struct FactoryMsg & msg, int error)
{
	msg.resp.error = error;
	sock->Send(msg);
}

void
Job::HandleMessage(MsgSocket * sock, const struct FactoryMsg & msg)
{
	struct FactoryMsg resp;

	mode_t mode = msg.open.flags;

	SendResponse(sock, resp, perm.IsPermitted(msg.open.path, mode));
}
