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
#include <paths.h>
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

static const char path_prefix[] = "PATH=";

/* Stolen from FreeBSD's libc/gen/exec.c */
static int
execvPe(const char *name, const char *path, char * const *argv,
    char * const *envp)
{
	const char **memp;
	size_t cnt, lp, ln;
	int eacces, save_errno;
	char *cur, buf[MAXPATHLEN];
	const char *p, *bp;
	struct stat sb;

	eacces = 0;

	/* If it's an absolute or relative path name, it's easy. */
	if (strchr(name, '/')) {
		bp = name;
		cur = NULL;
		goto retry;
	}
	bp = buf;

	/* If it's an empty path name, fail in the usual POSIX way. */
	if (*name == '\0') {
		errno = ENOENT;
		return (-1);
	}

	cur = (char*)alloca(strlen(path) + 1);
	if (cur == NULL) {
		errno = ENOMEM;
		return (-1);
	}
	strcpy(cur, path);
	while ((p = strsep(&cur, ":")) != NULL) {
		/*
		 * It's a SHELL path -- double, leading and trailing colons
		 * mean the current directory.
		 */
		if (*p == '\0') {
			p = ".";
			lp = 1;
		} else
			lp = strlen(p);
		ln = strlen(name);

		/*
		 * If the path is too long complain.  This is a possible
		 * security issue; given a way to make the path too long
		 * the user may execute the wrong program.
		 */
		if (lp + ln + 2 > sizeof(buf)) {
			fprintf(stderr, "execvP: %s path too long\n", p);
			continue;
		}
		bcopy(p, buf, lp);
		buf[lp] = '/';
		bcopy(name, buf + lp + 1, ln);
		buf[lp + ln + 1] = '\0';

retry:		(void)execve(bp, argv, envp);
		switch (errno) {
		case E2BIG:
			goto done;
		case ELOOP:
		case ENAMETOOLONG:
		case ENOENT:
			break;
		case ENOEXEC:
			for (cnt = 0; argv[cnt]; ++cnt)
				;
			memp = (const char**)alloca((cnt + 2) * sizeof(char *));
			if (memp == NULL) {
				/* errno = ENOMEM; XXX override ENOEXEC? */
				goto done;
			}
			memp[0] = "sh";
			memp[1] = bp;
			bcopy(argv + 1, memp + 2, cnt * sizeof(char *));
			(void)execve(_PATH_BSHELL,
			    __DECONST(char **, memp), envp);
			goto done;
		case ENOMEM:
			goto done;
		case ENOTDIR:
			break;
		case ETXTBSY:
			/*
			 * We used to retry here, but sh(1) doesn't.
			 */
			goto done;
		default:
			/*
			 * EACCES may be for an inaccessible directory or
			 * a non-executable file.  Call stat() to decide
			 * which.  This also handles ambiguities for EFAULT
			 * and EIO, and undocumented errors like ESTALE.
			 * We hope that the race for a stat() is unimportant.
			 */
			save_errno = errno;
			if (stat(bp, &sb) != 0)
				break;
			if (save_errno == EACCES) {
				eacces = 1;
				continue;
			}
			errno = save_errno;
			goto done;
		}
	}
	if (eacces)
		errno = EACCES;
	else
		errno = ENOENT;
done:
	return (-1);
}

static int
StartChild(const std::vector<char *> & argp, const std::vector<char *> & envpm, const char *path, int shm_fd,
    const std::optional<std::string> & workdir) __attribute__((noreturn));

static int
StartChild(const std::vector<char *> & argp, const std::vector<char *> & envp, const char *path, int shm_fd,
    const std::optional<std::string> & workdir)
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
	
	if (workdir) {
		int error = chdir(workdir->c_str());
		if (error != 0) {
			err(1, "Could not change cwd to '%s'\n", workdir->c_str());
		}
	}

	closefrom(SHARED_MEM_FD + 1);
	execvPe(argp.at(0), path, &argp[0], &envp[0]);
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
	const char * path = _PATH_DEFPATH;

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

		// -1 as we don't want to count the null-terminator
		size_t prefixLen = sizeof(path_prefix) - 1;
		if (strncmp(environ[i], path_prefix, prefixLen) == 0) {
			path = environ[i] + prefixLen;
		}
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
		StartChild(argp, envp, path, shm->GetFD(), command.GetWorkDir());
	} else {
		auto job = std::make_unique<Job>(command.GetPermissions(), completer, jobId, child);

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
