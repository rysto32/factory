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

#ifndef PRELOAD_SANDBOXER_H
#define PRELOAD_SANDBOXER_H

#include "Sandbox.h"

#include "JobSharedMemory.h"

#include <memory>
#include <vector>

class JobSharedMemory;
class MsgSocket;
class Command;
struct SandboxMsg;

class PreloadSandboxer : public Sandbox
{
private:
	const Command & command;
	JobSharedMemory shm;
	std::vector<std::unique_ptr<MsgSocket>> sockets;
	int exec_fd;

	void SendResponse(MsgSocket * sock, int error);

public:
	PreloadSandboxer(uint64_t jobId, const Command & c, const TempFile *msgSock);
	~PreloadSandboxer();

	virtual int GetExecFd() override;
	virtual void Enable() override;
	virtual void EnvironAppend(std::vector<char*> & envp) override;

	void RegisterSocket(std::unique_ptr<MsgSocket> sock);
	void HandleMessage(MsgSocket * sock, const SandboxMsg &);
};

#endif

