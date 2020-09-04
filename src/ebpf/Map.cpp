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

#include "ebpf/Map.h"

#include <errno.h>
#include <fcntl.h>

namespace Ebpf {

Map::Map()
  : ebpf(nullptr),
    fd(-1)
{
}

Map::Map(GBPFDriver *ebpf, std::string && name, int fd)
  : ebpf(ebpf),
    name(std::move(name)),
    fd(fd)
{
	fcntl(fd, F_SETFD, FD_CLOEXEC);
}

Map::Map(Map &&map)
  : ebpf(map.ebpf),
    name(std::move(map.name)),
    fd(map.fd)
{
	map.fd = -1;
}

Map::~Map()
{
	Close();
}

Map &
Map::operator=(Map &&map)
{
	Close();

	ebpf = map.ebpf;
	name = std::move(map.name);
	fd = map.fd;

	map.ebpf = nullptr;
	map.fd = -1;

	return *this;
}

void
Map::Close()
{
	if (fd >= 0) {
		gbpf_close_map_desc(ebpf, fd);
		ebpf = nullptr;
		fd = -1;
	}
}

int
Map::UpdateElem(const void * key, const void * value, int flags)
{

	if (!*this) {
		return (EBADF);
	}

	return gbpf_map_update_elem(ebpf, fd, const_cast<void*>(key),
	    const_cast<void*>(value), flags);
}

}
