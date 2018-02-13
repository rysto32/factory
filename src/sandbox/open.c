/*-
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
#include <sys/nv.h>

int
open(const char * path, int flags, ...)
{
	nvlist_t *msg;
	mode_t mode;
	int error;

	msg = nvlist_create(NV_FLAG_IGNORE_CASE);
	if (msg == NULL) {
		errno = ENOMEM;
		return (-1);
	}

	nvlist_add_number(msg, "type", MSG_TYPE_OPEN_REQUEST);
	nvlist_add_string(msg, "path", path);
	nvlist_add_number(msg, "flags", flags);

	error = nvlist_error(msg);
	if (error != 0) {
		errno = error;
		return (-1);
	}

	error = nvlist_send(msg_sock_fd, msg);
	if (error != 0) {
		err(1, "Failed to send to factory");
	}

	nvlist_destroy(msg);

	msg = nvlist_recv(msg_sock_fd, NV_FLAG_IGNORE_CASE);
	if (msg == NULL)
		err(1, "Failed to receive response from factory");

	if (nvlist_error(msg) != 0)
		err(1, "Response from factory had an error");

	error = nvlist_get_number(msg, "error");
	nvlist_destroy(msg);

	if (error != 0) {
		errno = error;
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
	return (open(path, flags, mode));
}
