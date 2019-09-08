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

#ifndef PRELOAD_SANDBOXER_FACTORY_H
#define PRELOAD_SANDBOXER_FACTORY_H

#include "SandboxFactory.h"

#include "MsgSocketServer.h"

#include <memory>
#include <unordered_map>

class EventLoop;
class MsgSocket;
class PreloadSandboxer;
class TempFileManager;

class PreloadSandboxerFactory : public SandboxFactory
{
	typedef std::unordered_map<uint64_t, std::unique_ptr<PreloadSandboxer>> JobMap;

	JobMap jobMap;
	MsgSocketServer server;

public:
	PreloadSandboxerFactory(TempFileManager &, EventLoop &, int maxJobs);
	virtual ~PreloadSandboxerFactory();

	virtual Sandbox & MakeSandbox(uint64_t jid, const Command &command) override;
	virtual void ReleaseSandbox(uint64_t jid) override;

	PreloadSandboxer *RegisterSocket(uint64_t jobId, std::unique_ptr<MsgSocket>);
};

#endif

