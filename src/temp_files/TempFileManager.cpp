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

#include "TempFileManager.h"

#include "TempDir.h"
#include "TempFile.h"

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

TempFileManager::TempFileManager()
  : tempDir(std::make_shared<TempDir>())
{
}

std::unique_ptr<TempFile>
TempFileManager::GetUnixSocket(const std::string & name, int maxConnect)
{
	std::string path(tempDir->GetPath());
	path += '/';
	path += name;

	struct sockaddr_un addr;
	addr.sun_len = sizeof(addr);
	addr.sun_family = AF_UNIX;
	strlcpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path));

	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return std::unique_ptr<TempFile>();

	int error = bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
	if (error != 0) {
		close(fd);
		return std::unique_ptr<TempFile>();
	}

	error = listen(fd, maxConnect);
	if (error != 0) {
		close(fd);
		return std::unique_ptr<TempFile>();
	}

	return std::make_unique<TempFile>(path, tempDir, FileDesc(fd));
}
