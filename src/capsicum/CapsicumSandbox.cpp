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

#include "CapsicumSandbox.h"

#include "Command.h"
#include "Permission.h"
#include "PermissionList.h"

#include <sys/capsicum.h>

#include <err.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

CapsicumSandbox::PreopenDesc::~PreopenDesc()
{
	close(fd);
}

CapsicumSandbox::CapsicumSandbox(const Command & c)
  : ebpf(ebpf_dev_driver_create()),
    open_prog(-1),
    fd_map(-1)
{
	if (!ebpf) {
		err(1, "Could not create ebpf instance.");
	}

	PreopenDescriptors(c.GetPermissions());
	CreateEbpfRules();
}

CapsicumSandbox::~CapsicumSandbox()
{
	if (open_prog >= 0)
		gbpf_close_prog_desc(&ebpf->base, open_prog);

	if (fd_map >= 0)
		gbpf_close_map_desc(&ebpf->base, fd_map);

	if (ebpf)
		ebpf_dev_driver_destroy(ebpf);
}

void
CapsicumSandbox::PreopenDescriptors(const PermissionList &permList)
{
	cap_rights_t rights;
	int fd, mode;

	for (const auto & [path, perm] : permList.GetPermMap()) {
		cap_rights_init(&rights, CAP_MMAP, CAP_LOOKUP);
		mode = 0;

		if (perm & Permission::READ) {
			cap_rights_set(&rights, CAP_READ, CAP_SEEK);
			mode = O_RDONLY;
		}

		if (perm & Permission::WRITE) {
			cap_rights_set(&rights, CAP_WRITE, CAP_SEEK);
			mode = O_RDWR | O_CREAT;
		}

		if (perm & Permission::EXEC) {
			cap_rights_set(&rights, CAP_FEXECVE, CAP_READ);
			mode |= O_EXEC;
		}

		fd = open(path.c_str(), mode, 0600);
		if (fd < 0) {
			err(1, "Could not open '%s'", path.c_str());
		}

		if (cap_rights_limit(fd, &rights) < 0 && errno != ENOSYS) {
			err(1, "cap_rights_limit() failed");
		}

		descriptors.emplace_back(path, fd);
	}
}

void
CapsicumSandbox::DefineProgram(GBPFElfWalker *walker, const char *name,
    struct ebpf_inst *prog, uint32_t prog_len)
{
	CapsicumSandbox *sandbox = reinterpret_cast<CapsicumSandbox*>(walker->data);

	assert (strcmp(name, "open_syscall_probe") == 0);
	sandbox->open_prog = gbpf_load_prog(walker->driver, EBPF_PROG_TYPE_VFS,
	    prog, prog_len);
	if (sandbox->open_prog < 0)
		err(1, "Could not load EBPF program");
}

void
CapsicumSandbox::DefineMap(GBPFElfWalker *walker, const char *name, int desc,
    struct ebpf_map_def *map)
{
	CapsicumSandbox *sandbox = reinterpret_cast<CapsicumSandbox*>(walker->data);

	assert (strcmp(name, "fd_map") == 0);
	sandbox->fd_map = desc;
}

void
CapsicumSandbox::CreateEbpfRules()
{
	int error;
	GBPFElfWalker walker = {
		.on_prog = DefineProgram,
		.on_map = DefineMap,
		.data = this,
	};

	error = gbpf_walk_elf(&walker, &ebpf->base, "/home/rstone/src/ebpf/sample.o");
	if (error != 0) {
		err(1, "Could not walk EBPF object");
	}

	if (open_prog < 0) {
		errx(1, "EBPF object did not define program");
	}

	if (fd_map < 0) {
		errx(1, "EBPF object did not define map");
	}

	for (const auto & desc : descriptors) {
		char path[MAXPATHLEN];

		bzero(path, sizeof(path));
		strlcpy(path, desc.path.c_str(), sizeof(path));

		int fd = desc.fd;
		gbpf_map_update_elem(&ebpf->base, fd_map, path, &fd, EBPF_NOEXIST);
	}
}

void
CapsicumSandbox::Enable()
{
	int error;

	error = gbpf_attach_probe(&ebpf->base, open_prog, "open_syscall_probe", 0);
	if (error != 0) {
		err(1, "Could not attach EBPF program to open probe");
	}

	cap_enter();
}

void
CapsicumSandbox::ParentCleanup()
{
	/* Close all file descriptors we passed to the child process. */
	descriptors.clear();
}
