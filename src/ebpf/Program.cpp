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

#include "ebpf/Program.h"

#include <err.h>
#include <fcntl.h>

namespace Ebpf
{

Program::Program()
  : ebpf(NULL),
    fd(-1)
{

}

Program::Program(Program &&p)
  : ebpf(p.ebpf),
    name(std::move(p.name)),
    fd(p.fd)
{
	p.fd = -1;
}

Program::Program(GBPFDriver *ebpf, std::string && n, int type, struct ebpf_inst *prog, uint32_t prog_len)
  : ebpf(ebpf),
    name(std::move(n))
{
	fd = gbpf_load_prog(ebpf, type, prog, prog_len);
	if (fd < 0) {
		err(1, "Could not load program '%s'", name.c_str());
	}

	fcntl(fd, F_SETFD, FD_CLOEXEC);
// 	fprintf(stderr, "Map prog '%s' to FD %d\n", name.c_str(), fd);
}

Program &
Program::operator=(Program &&other)
{
	if (this == &other) {
		return *this;
	}

	Close();

	ebpf = other.ebpf;
	name = std::move(other.name);
	fd = other.fd;
	other.fd = -1;

	return *this;
}

void
Program::Close()
{
	if (*this) {
		gbpf_close_prog_desc(ebpf, fd);
	}
}

Program::~Program()
{
	Close();
}

int
Program::AttachProbe()
{
	return gbpf_attach_probe(ebpf, fd, "ebpf", "sc_rewrite", "",
	    name.c_str(), "enter", 0);
}

}
