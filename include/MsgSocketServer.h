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

#ifndef MSG_SOCKET_SERVER_H
#define MSG_SOCKET_SERVER_H

#include "Event.h"

#include <memory>
#include <unordered_map>

class EventLoop;
class Job;
class JobManager;
class MsgSocket;
class TempFile;

class MsgSocketServer : public Event
{
private:
	std::unique_ptr<TempFile> listenSock;
	std::unordered_map<int, std::unique_ptr<MsgSocket> > incompleteSockets;
	EventLoop &eventLoop;
	JobManager &jobMgr;

public:
	MsgSocketServer(std::unique_ptr<TempFile> fd, EventLoop & loop,
	    JobManager &jobMgr);
	~MsgSocketServer();

	MsgSocketServer(const MsgSocketServer&) = delete;
	MsgSocketServer(MsgSocketServer &&) = delete;
	MsgSocketServer & operator=(const MsgSocketServer &) = delete;
	MsgSocketServer & operator=(MsgSocketServer &&) = delete;

	void Dispatch(int fd, short flags) override;

	Job * CompleteSocket(MsgSocket *, uint64_t jobId);
};

#endif
