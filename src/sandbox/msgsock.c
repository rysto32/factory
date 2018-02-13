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

#include "msgsock.h"

#include "MsgType.h"

#include <sys/socket.h>
#include <sys/un.h>

#include <err.h>
#include <unistd.h>

static int msg_sock_fd;

void
msgsock_init(const struct sockaddr_un *addr, uint64_t jobId)
{
	struct FactoryMsg msg;

	msg_sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (msg_sock_fd < 0)
		err(1, "Could not create msg socket");

	int error = connect(msg_sock_fd, (struct sockaddr*)addr, sizeof(*addr));
	if (error != 0)
		err(1, "Could not connect msg socket");


	msg.type = MSG_TYPE_INIT;
	msg.init.jobId = jobId;

	error = msgsock_send(&msg);
	if (error != 0)
		err(1, "Could not send init msg");
}

int
msgsock_send(const struct FactoryMsg *msg)
{
	ssize_t sent;

	sent = send(msg_sock_fd, msg, sizeof(*msg), 0);
	if (sent != sizeof(*msg))
		return (-1);

	return (0);
}

int
msgsock_recv(struct FactoryMsg *msg)
{
	uint8_t *next;
	ssize_t recvd;
	size_t size;

	next = (uint8_t*)msg;
	size = sizeof(*msg);

	do {
		recvd = recv(msg_sock_fd, next, size, 0);
		if (recvd < 0)
			return (-1);

		next += recvd;
		size -= recvd;
	} while (size > 0);

	return (0);
}