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

#include "JobSharedMemory.h"

#include "SharedMem.h"
#include "TempFile.h"

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/un.h>

#include <err.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

template<typename T, typename U>
T RoundUp(T value, U mult)
{
	return ((value + (mult - 1)) / mult) * mult;
}

JobSharedMemory::JobSharedMemory(const TempFile *msgSock, uint64_t jobId)
{
	shm_fd = shm_open(SHM_ANON, O_CREAT | O_TRUNC | O_RDWR, 0600);
	if (shm_fd < 0)
		err(1, "shm_open failed");

	long page_size = sysconf(_SC_PAGE_SIZE);

	size_t size = RoundUp(sizeof(struct FactoryShm), page_size);

	int error = ftruncate(shm_fd, size);
	if (error < 0)
		err(1, "Failed to set size of shared memory region");

	void *mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	if (mem == MAP_FAILED)
		err(1, "Failed to map shared memory region");

	auto * shm = static_cast<struct FactoryShm*>(mem);

	shm->header.size = size;
	shm->header.api_num = SHARED_MEM_API_NUM;
	strlcpy(shm->sandbox_lib, LIB_LOCATION, sizeof(shm->sandbox_lib));
	InitUnixAddr(shm->msg_socket_path, msgSock);
	shm->jobId = jobId;

	munmap(shm, size);
}

JobSharedMemory::~JobSharedMemory()
{
	close(shm_fd);
	shm_fd = -1;
}

void
JobSharedMemory::InitUnixAddr(struct sockaddr_un &addr, const TempFile *msgSock)
{
	addr.sun_len = sizeof(addr);
	addr.sun_family = AF_UNIX;
	strlcpy(addr.sun_path, msgSock->GetPath().c_str(), sizeof(addr.sun_path));
}
