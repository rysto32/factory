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

#include "JobManager.h"

#include "EventLoop.h"
#include "Job.h"
#include "JobQueue.h"
#include "JobSharedMemory.h"
#include "MsgSocket.h"
#include "SharedMem.h"
#include "TempFile.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <cassert>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

// Not defined by any header(!)
extern char ** environ;

static int
StartChild(const std::vector<char *> & argp, const std::vector<char *> & envpm, int shm_fd,
    const Command & command) __attribute__((noreturn));

static int
StartChild(const std::vector<char *> & argp, const std::vector<char *> & envp, int shm_fd,
    const Command & command)
{
	int fd = dup2(shm_fd, SHARED_MEM_FD);
	if (fd < 0) {
		perror("Could not dup shm_fd");
		exit(1);
	}

	int error = fcntl(SHARED_MEM_FD, F_SETFD, 0);
	if (error < 0) {
		perror("Could not disable close-on-exec");
		exit(1);
	}

	error = chdir(command.GetWorkDir().c_str());
	if (error != 0) {
		err(1, "Could not change cwd to '%s'\n", command.GetWorkDir().c_str());
	}

	auto stdin = command.GetStdin();
	if (stdin) {
		fd = open(stdin->c_str(), O_RDONLY);
		if (fd < 0) {
			err(1, "Could not open '%s' for reading\n", stdin->c_str());
		}

		fd = dup2(fd, STDIN_FILENO);
		if (fd != STDIN_FILENO) {
			err(1, "Could not set fd %d", STDIN_FILENO);
		}
	}

	auto stdout = command.GetStdout();
	if (stdout) {
		fd = open(stdout->c_str(), O_WRONLY | O_CREAT, 0700);
		if (fd < 0) {
			err(1, "Could not open '%s' for writing\n", stdout->c_str());
		}

		fd = dup2(fd, STDOUT_FILENO);
		if (fd != STDOUT_FILENO) {
			err(1, "Could not set fd %d", STDOUT_FILENO);
		}
	}

	for (int i = STDERR_FILENO + 1; i < SHARED_MEM_FD; ++i) {
		(void)close(i);
	}
	closefrom(SHARED_MEM_FD + 1);

	execve(argp.at(0), &argp[0], &envp[0]);
	err(1, "execve %s failed", argp.at(0));
	_exit(1);
}

JobManager::JobManager(EventLoop & loop, TempFile *msgSock, JobQueue &q)
  : loop(loop),
    msgSock(msgSock),
    jobQueue(q),
    next_job_id(0)

{
	loop.RegisterSignal(this, SIGCHLD);
}

JobManager::~JobManager()
{
}

uint64_t
JobManager::AllocJobId()
{
	next_job_id++;
	return next_job_id;
}

Job*
JobManager::StartJob(Command & command, JobCompletion & completer)
{
	std::vector<char *>  argp;
	const ArgList & argList = command.GetArgList();
	std::ostringstream commandStr;

	for (const std::string & arg : argList) {
		// Blame POSIX for the const_cast :()
		argp.push_back(const_cast<char*>(arg.c_str()));
		commandStr << arg << " ";
	}
	argp.push_back(NULL);

	printf("Run: %s\n", commandStr.str().c_str());

	std::vector<char *> envp;
	for (int i = 0; environ[i] != NULL; ++i) {
		envp.push_back(environ[i]);
	}
	char ld_preload[] = "LD_PRELOAD=" LIB_LOCATION;
	envp.push_back(ld_preload);
	envp.push_back(NULL);

	uint64_t jobId = AllocJobId();
	auto shm = std::make_unique<JobSharedMemory>(msgSock, jobId);

	pid_t child = fork();
	if (child < 0)
		return NULL;

	if (child == 0) {
		StartChild(argp, envp, shm->GetFD(), command);
	} else {
		auto job = std::make_unique<Job>(command.GetPermissions(), completer, jobId, child, command.GetWorkDir());

		pidMap.insert(std::make_pair(child, job.get()));
		auto ins = jobMap.insert(std::make_pair(jobId, std::move(job)));
		assert (ins.second);
		return ins.first->second.get();
	}
}

void
JobManager::Dispatch(int sig, short flags)
{
	assert (sig == SIGCHLD);

	while (1) {
		int status;
		pid_t pid = wait3(&status, WEXITED | WNOHANG, NULL);
		if (pid == 0)
			break;

		if (pid == -1) {
			if (errno == ECHILD)
				break;
			else
				err(1, "wait3 failed");
		}

		auto it = pidMap.find(pid);
		if (it == pidMap.end()) {
			fprintf(stderr, "Unknown child %d exited!\n", pid);
			continue;
		}

		it->second->Complete(status);
		jobMap.erase(it->second->GetJobId());
		pidMap.erase(it);

		ScheduleJob();
	}
}

bool
JobManager::ScheduleJob()
{
	Command * command = jobQueue.RemoveNext();

	if (command == nullptr) {
		if (jobMap.empty())
			loop.SignalExit();
		return false;
	}

	StartJob(*command, *command);
	return true;
}

Job *
JobManager::RegisterSocket(uint64_t jobId, std::unique_ptr<MsgSocket> sock)
{
	auto it = jobMap.find(jobId);
	if (it == jobMap.end())
		err(1, "Process attempted to register against non-existnt job %ld",
		    jobId);

	it->second->RegisterSocket(std::move(sock));
	return it->second.get();
}
