/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ryan Stone
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

#ifndef FILEDESC_H
#define FILEDESC_H

#include <cassert>
#include <fcntl.h>
#include <unistd.h>

class FileDesc
{
	int fd;

public:
	FileDesc()
	  : fd(-1)
	{
	}

	explicit FileDesc(int f)
	  : fd(f)
	{
	}

	FileDesc(FileDesc && f) noexcept
	  : fd(f.fd)
	{
		f.fd = -1;
	}

	~FileDesc()
	{
		Close();
	}

	FileDesc(const FileDesc &) = delete;

	FileDesc & operator=(FileDesc &&f)
	{
		if (this == &f)
			return *this;

		Close();
		fd = f.fd;
		f.fd = -1;

		return *this;
	}

	FileDesc & operator=(const FileDesc &) = delete;

	static FileDesc Open(const char * name, int flags, int mode = 0)
	{
		return FileDesc(open(name, flags, mode));
	}

	void Close()
	{
		if (fd > 0) {
			close(fd);
			fd = -1;
		}
	}

	operator int() const
	{
		return fd;
	}
};

#endif

