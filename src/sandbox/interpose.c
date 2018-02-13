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

#include "interpose.h"

#include "MsgType.h"
#include "SharedMem.h"

#include <dlfcn.h>
#include <err.h>
#include <sys/mman.h>
#include <sys/nv.h>
#include <unistd.h>

static void initialize(void) __attribute__((constructor));

fexecve_t * real_fexecve;
open_t *real_open;

int msg_sock_fd;

struct FactoryShm *shm;

static void
initialize(void)
{
	long page_size = sysconf(_SC_PAGE_SIZE);

	void *mem = mmap(NULL, page_size, PROT_READ, MAP_SHARED, SHARED_MEM_FD, 0);
	if (mem == MAP_FAILED) {
		err(1, "Failed to map shared memory header.");
	}

	struct FactoryShmHeader *header = mem;
	if (header->api_num != SHARED_MEM_API_NUM) {
		errx(1, "factory: shared memory api mismatch!");
	}

	size_t size = header->size;
	munmap(mem, page_size);

	mem = mmap(NULL, size, PROT_READ, MAP_SHARED, SHARED_MEM_FD, 0);
	if (mem == MAP_FAILED) {
		err(1, "Failed to map shared memory.");
	}
	shm = mem;

	real_fexecve = (fexecve_t *)dlsym(RTLD_NEXT, "fexecve");
	real_open = (open_t *)dlsym(RTLD_NEXT, "open");

	msg_sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (msg_sock_fd < 0)
		err(1, "Could not create msg socket");

	int error = connect(msg_sock_fd, (struct sockaddr*)&shm->msg_socket_path,
	    sizeof(shm->msg_socket_path));
	if (error != 0)
		err(1, "Could not connect msg socket");

	nvlist_t *msg = nvlist_create(NV_FLAG_IGNORE_CASE);
	if (msg == NULL)
		err(1, "Could not alloc nvlist msg");

	nvlist_add_number(msg, "type", MSG_TYPE_INIT);
	nvlist_add_number(msg, "jid", shm->jobId);

	error = nvlist_send(msg_sock_fd, msg);
	if (error != 0)
		err(1, "Could not send init msg");

// 	write(2, "initialize\n", sizeof("initialize\n"));
}
