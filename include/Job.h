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

#ifndef JOB_H
#define JOB_H

#include <sys/types.h>
#include <sys/nv.h>

#include <memory>
#include <vector>

#include "MsgType.h"

class JobCompletion;
class MsgSocket;

class Job
{
private:
	JobCompletion &completer;
	std::vector<std::unique_ptr<MsgSocket>> sockets;
	uint64_t jobId;
	pid_t pid;

public:
	Job(JobCompletion &, int id, pid_t pid);
	~Job();

	Job(const Job &) = delete;
	Job(Job &&) = delete;
	Job & operator=(const Job &) = delete;
	Job & operator=(Job &&) = delete;

	void RegisterSocket(std::unique_ptr<MsgSocket> sock);
	void HandleMessage(MsgSocket * sock, MsgType type, nvlist_t *msg);
	void Complete(int status);

	int GetJobId() const
	{
		return jobId;
	}

	pid_t GetPid() const
	{
		return pid;
	}
};

#endif
