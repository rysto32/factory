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

#include <elf.h>
#include <err.h>
#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>
#include <string.h>
#include <unistd.h>

CapsicumSandbox::PreopenDesc::~PreopenDesc()
{
	close(fd);
}

CapsicumSandbox::CapsicumSandbox(const Command & c)
  : ebpf(ebpf_dev_driver_create()),
    open_prog(-1),
    stat_prog(-1),
    fd_map(-1),
    fexec_fd(-1),
    interp_fd(-1)
{
	if (!ebpf) {
		err(1, "Could not create ebpf instance.");
	}

	FindInterpreter(c.GetExecutable());
	PreopenDescriptors(c.GetPermissions());
	CreateEbpfRules();
}

CapsicumSandbox::~CapsicumSandbox()
{
	if (open_prog >= 0)
		gbpf_close_prog_desc(&ebpf->base, open_prog);

	if (stat_prog >= 0)
		gbpf_close_prog_desc(&ebpf->base, stat_prog);

	if (fd_map >= 0)
		gbpf_close_map_desc(&ebpf->base, fd_map);

	if (interp_fd >= 0) {
		close(interp_fd);
	}

	if (fexec_fd >= 0) {
		close(fexec_fd);
	}

	if (ebpf)
		ebpf_dev_driver_destroy(ebpf);
}


void
CapsicumSandbox::FindInterpreter(Path exe)
{
	GElf_Phdr phdr;
	size_t filesize, i, phnum;
	int fd;
	Elf *elf;
	const char *s, *interp;

	fd = open(exe.c_str(), O_RDONLY);
	if (fd < 0) {
		err(1, "Could not open executable '%s'", exe.c_str());
	}

	elf = elf_begin(fd, ELF_C_READ, NULL);
	if (elf == nullptr) {
		errx(1, "Could not init elf object: %s", elf_errmsg(-1));
	}

	s = elf_rawfile(elf, &filesize);
	if (s == NULL) {
		errx(1, "elf_rawfile failed: %s", elf_errmsg(-1));
		return;
	}

	if (!elf_getphnum(elf, &phnum)) {
		errx(1, "elf_getphnum failed: %s", elf_errmsg(-1));
		return;
	}
	for (i = 0; i < phnum; i++) {
		if (gelf_getphdr(elf, i, &phdr) != &phdr) {
			errx(1, "elf_getphdr failed: %s", elf_errmsg(-1));
			continue;
		}
		if (phdr.p_type == PT_INTERP) {
			if (phdr.p_offset >= filesize) {
				warnx("invalid phdr offset");
				continue;
			}
			interp = s + phdr.p_offset;

			fexec_fd = open(interp, O_RDONLY | O_EXEC);
			if (fexec_fd < 0) {
				err(1, "Failed to open rtld '%s'", interp);
			}

			interp_fd = fd;
			interp_fd_str = std::to_string(fd);
			goto out;
		}
	}

	/* Did not find an interpreter; must be statically linked. */
	fexec_fd = fd;

out:
	elf_end(elf);
}

void
CapsicumSandbox::PreopenDescriptors(const PermissionList &permList)
{
	cap_rights_t rights;
	int fd, mode;

	for (const auto & [path, perm] : permList.GetPermMap()) {
		cap_rights_init(&rights, CAP_MMAP, CAP_LOOKUP, CAP_FSTAT);
		mode = 0;

		if (perm & Permission::READ) {
			cap_rights_set(&rights, CAP_READ, CAP_SEEK, CAP_MMAP_R);
			mode = O_RDONLY;
		}

		if (perm & Permission::WRITE) {
			cap_rights_set(&rights, CAP_WRITE, CAP_SEEK, CAP_MMAP_W);
			mode = O_RDWR | O_CREAT;
		}

		if (perm & Permission::EXEC) {
			cap_rights_set(&rights, CAP_FEXECVE, CAP_READ, CAP_MMAP_X);
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

	if (strcmp(name, "open_syscall_probe") == 0) {
		sandbox->open_prog = gbpf_load_prog(walker->driver, EBPF_PROG_TYPE_VFS,
		    prog, prog_len);
		if (sandbox->open_prog < 0)
			err(1, "Could not load open EBPF program");
	} else if (strcmp(name, "stat_syscall_probe") == 0) {
		sandbox->stat_prog = gbpf_load_prog(walker->driver, EBPF_PROG_TYPE_VFS,
		    prog, prog_len);
		if (sandbox->stat_prog < 0)
			err(1, "Could not load stat EBPF program");
	} else {
		errx(1, "Unexpected probe '%s'", name);
	}
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

	error = gbpf_walk_elf(&walker, &ebpf->base, "/home/rstone/repos/factory/src/capsicum/ebpf_progs/open/open.o");
	if (error != 0) {
		err(1, "Could not walk EBPF object");
	}

	if (open_prog < 0) {
		errx(1, "EBPF object did not define open program");
	}

	if (stat_prog < 0) {
		errx(1, "EBPF object did not define stat program");
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
CapsicumSandbox::ArgvPrepend(std::vector<char*> & argp)
{

	if (interp_fd >= 0) {
		// Blame POSIX for the const_cast :()
		argp.push_back(const_cast<char*>("rtld"));
		argp.push_back(const_cast<char*>("-f"));
		argp.push_back(const_cast<char*>(interp_fd_str.c_str()));
		argp.push_back(const_cast<char*>("--"));
	}
}

int
CapsicumSandbox::GetExecFd()
{
	return fexec_fd;
}

void
CapsicumSandbox::Enable()
{
	int error;

	error = gbpf_attach_probe(&ebpf->base, open_prog, "open_syscall_probe", 0);
	if (error != 0) {
		err(1, "Could not attach EBPF program to open probe");
	}
	error = gbpf_attach_probe(&ebpf->base, stat_prog, "stat_syscall_probe", 0);
	if (error != 0) {
		err(1, "Could not attach EBPF program to stat probe");
	}

	cap_enter();
}

void
CapsicumSandbox::ParentCleanup()
{
	/* Close all file descriptors we passed to the child process. */
	descriptors.clear();
}
