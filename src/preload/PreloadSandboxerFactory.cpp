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

#include "PreloadSandboxerFactory.h"

#include "MsgSocket.h"
#include "PreloadSandboxer.h"
#include "TempFileManager.h"
#include "TempFile.h"

PreloadSandboxerFactory::PreloadSandboxerFactory(TempFileManager & tmpMgr,
    EventLoop &loop, int maxJobs)
  : server(tmpMgr.GetUnixSocket("msg_sock", maxJobs), loop, *this)
{

}

PreloadSandboxerFactory::~PreloadSandboxerFactory()
{

}

Sandbox &
PreloadSandboxerFactory::MakeSandbox(uint64_t jid, const Command &command)
{
	auto [it, inserted] = jobMap.emplace(jid, std::make_unique<PreloadSandboxer>(jid, command, server.GetSock()));

	return *it->second;
}

void
PreloadSandboxerFactory::ReleaseSandbox(uint64_t jid)
{
	jobMap.erase(jid);
}

PreloadSandboxer *
PreloadSandboxerFactory::RegisterSocket(uint64_t jobId, std::unique_ptr<MsgSocket> sock)
{
	auto it = jobMap.find(jobId);
	if (it == jobMap.end()) {
		// Process never sent any queries before exiting, and we lost a
		// race and didn't see that it had started before we got notified
		// that it exited.  Just return, there's nothing to do.
		return nullptr;
	}

	it->second->RegisterSocket(std::move(sock));
	return it->second.get();
}
