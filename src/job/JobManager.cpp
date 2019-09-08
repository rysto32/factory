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
#include "MsgSocket.h"
#include "Sandbox.h"
#include "SandboxFactory.h"

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
StartChild(const std::vector<char *> & argp, const std::vector<char *> & envpm,
    Sandbox & boxer, const Command & command) __attribute__((noreturn));

static int
StartChild(const std::vector<char *> & argp, const std::vector<char *> & envp,
    Sandbox & sandbox, const Command & command)
{
	int fd, error;

	error = chdir(command.GetWorkDir().c_str());
	if (error != 0) {
		err(1, "Could not change cwd to '%s'\n", command.GetWorkDir().c_str());
	}

	/* Create a new process group and put this process in it. */
	error = setpgid(0, 0);
	if (error != 0) {
		err(1, "Could not create process group");
	}

	auto stdin = command.GetStdin();
	const char * stdin_file;
	if (stdin) {
		stdin_file = stdin->c_str();
	} else {
		stdin_file = "/dev/null";
	}

	fd = open(stdin_file, O_RDONLY);
	if (fd < 0) {
		err(1, "Could not open '%s' for reading\n", stdin_file);
	}

	fd = dup2(fd, STDIN_FILENO);
	if (fd != STDIN_FILENO) {
		err(1, "Could not set fd %d", STDIN_FILENO);
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

	fd = open(argp.at(0), O_RDONLY | O_EXEC);
	if (fd < 0) {
		err(1, "Could not open '%s' for exec", argp.at(0));
	}

	sandbox.Enable();

	fexecve(fd, &argp[0], &envp[0]);
	err(1, "execve %s failed", argp.at(0));
	_exit(1);
}

JobManager::JobManager(EventLoop & loop, JobQueue &q, std::unique_ptr<SandboxFactory> &&f, int max)
  : loop(loop),
    jobQueue(q),
    sandboxFactory(std::move(f)),
    maxRunning(max),
    next_job_id(0)

{
	loop.RegisterSignal(this, SIGCHLD);
}

JobManager::~JobManager()
{
	for (auto & [pid, job] : pidMap) {
		job->Abort();
	}
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

	uint64_t jobId = AllocJobId();
	Sandbox &sandbox = sandboxFactory->MakeSandbox(jobId, command);

	fprintf(stderr, "Run: \"%s\" as job %lld\n", commandStr.str().c_str(), (long long)jobId);

	std::vector<char *> envp;
	for (int i = 0; environ[i] != NULL; ++i) {
		envp.push_back(environ[i]);
	}
	sandbox.EnvironAppend(envp);
	envp.push_back(NULL);

	pid_t child = fork();
	if (child < 0)
		return NULL;

	if (child == 0) {
		StartChild(argp, envp, sandbox, command);
	} else {
		sandbox.ParentCleanup();

		auto job = std::make_unique<Job>(completer, jobId, child, command.GetWorkDir());

		auto ins = pidMap.insert(std::make_pair(child, std::move(job)));
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
		sandboxFactory->ReleaseSandbox(it->second->GetJobId());
		pidMap.erase(it);

		ScheduleJob();
	}
}

bool
JobManager::ScheduleJob()
{
	while (pidMap.size() < maxRunning) {
		Command * command = jobQueue.RemoveNext();

		if (command == nullptr) {
			if (pidMap.empty())
				loop.SignalExit();
			break;
		}

		StartJob(*command, *command);
	}
	return pidMap.size() > 0;
}
