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

#ifndef JOB_MANAGER_H
#define JOB_MANAGER_H

#include "Event.h"
#include "PendingJob.h"

#include <sys/types.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class EventLoop;
class Job;
class JobCompletion;
class JobQueue;
class MsgSocket;
class PermissionList;
class TempFile;

class JobManager : private Event
{
private:
	typedef std::unordered_map<uint64_t, std::unique_ptr<Job>> JobMap;
	typedef std::unordered_map<pid_t, Job*> PidMap;

	JobMap jobMap;
	PidMap pidMap;
	EventLoop &loop;
	TempFile *msgSock;
	JobQueue & jobQueue;

	uint64_t next_job_id;

	uint64_t AllocJobId();

public:
	JobManager(EventLoop &, TempFile *, JobQueue &);
	~JobManager();

	JobManager(const JobManager &) = delete;
	JobManager(JobManager &&) = delete;
	JobManager & operator=(const JobManager &) = delete;
	JobManager & operator=(JobManager &&) = delete;

	Job * StartJob(PendingJob &, JobCompletion &);

	void Dispatch(int fd, short flags) override;
	bool ScheduleJob();

	Job *RegisterSocket(uint64_t jobId, std::unique_ptr<MsgSocket>);
};

#endif
