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

#include "SharedMem.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <err.h>
#include <fcntl.h>
#include <unistd.h>

#include <cassert>
#include <memory>
#include <string>
#include <vector>

typedef std::vector<std::string> ArgList;

// Not defined by any header(!)
extern char ** environ;

#define LIB_LOCATION "/tmp/libfactory_sandbox.so.1"

template<typename T, typename U>
T RoundUp(T value, U mult)
{
	return ((value + (mult - 1)) / mult) * mult;
}

static int InitSharedMem(int shm_fd)
{
	long page_size = sysconf(_SC_PAGE_SIZE);

	size_t size = RoundUp(sizeof(struct FactoryShm), page_size);

	int error = ftruncate(shm_fd, size);
	if (error < 0)
		return error;

	void *mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	if (mem == MAP_FAILED)
		return (-1);

	auto * shm = static_cast<struct FactoryShm*>(mem);

	shm->header.size = size;
	shm->header.api_num = SHARED_MEM_API_NUM;
	strlcpy(shm->sandbox_lib, LIB_LOCATION, sizeof(shm->sandbox_lib));

	munmap(shm, size);
	return (0);
}

static int
StartChild(const std::vector<char *> & argp, const std::vector<char *> & envp, int shm_fd) __attribute__((noreturn));

static int
StartChild(const std::vector<char *> & argp, const std::vector<char *> & envp, int shm_fd)
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

	closefrom(SHARED_MEM_FD + 1);
	execve(argp.at(0), &argp[0], &envp[0]);
	err(1, "execve %s failed", argp.at(0));
	_exit(1);
}

int RunJob(const ArgList & argList)
{
	std::vector<char *>  argp;

	for (const std::string & arg : argList) {
		// Blame POSIX for the const_cast :()
		argp.push_back(const_cast<char*>(arg.c_str()));
	}
	argp.push_back(NULL);

	std::vector<char *> envp;
	for (int i = 0; environ[i] != NULL; ++i) {
		envp.push_back(environ[i]);
	}
	char ld_preload[] = "LD_PRELOAD=" LIB_LOCATION;
	envp.push_back(ld_preload);
	envp.push_back(NULL);

	int shm_fd = shm_open(SHM_ANON, O_CREAT | O_TRUNC | O_RDWR, 0600);
	if (shm_fd < 0)
		return (shm_fd);

	int error = InitSharedMem(shm_fd);
	if (error != 0)
		return (error);

	pid_t child = fork();
	if (child < 0)
		return child;

	if (child == 0) {
		StartChild(argp, envp, shm_fd);
	} else {
		int status;
		waitpid(child, &status, WEXITED);
		return (status);
	}
}