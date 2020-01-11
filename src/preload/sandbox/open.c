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
#include "SharedMem.h"
#include "MsgType.h"

#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#define DEBUG(msg) write(2, (msg), sizeof(msg))

int
open(const char * path, int flags, ...)
{
	struct SandboxMsg msg;
	struct SandboxResp resp;
	mode_t mode;
	int error;

	DEBUG("intercept open(2)\n");

	if (msg_sock_fd < 0)
		initialize();

	msg.type = MSG_TYPE_OPEN_REQUEST;
	msg.open.flags = flags;
	strlcpy(msg.open.path, path, sizeof(msg.open.path));

	error = send_sandbox_msg(&msg);
	if (error != 0) {
		err(1, "Failed to send to factory");
	}

	ssize_t bytes = recv(msg_sock_fd, &resp, sizeof(resp), 0);
	if (bytes != sizeof(resp)) {
		err(1, "Failed to receive from factory");
	}

	if (resp.error != 0) {
		errno = resp.error;
		return (-1);
	}

	if (flags & O_CREAT) {
		va_list va;

		va_start(va, flags);
		mode = va_arg(va, int);

		return (real_open(path, flags, mode));
	} else {
		return (real_open(path, flags));
	}
}

int
_open(const char * path, int flags, mode_t mode)
{
	DEBUG("intercept _open()\n");
	return (open(path, flags, mode));
}
