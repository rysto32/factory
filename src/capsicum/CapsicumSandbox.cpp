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
#include <sys/syslimits.h>
#include <unistd.h>

CapsicumSandbox::CapsicumSandbox(const Command & c)
  : ebpf(ebpf_dev_driver_create()),
    work_dir(c.GetWorkDir()),
    work_dir_fd(-1),
    is_rtld(false)
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
	defer_programs.clear();
	probe_programs.clear();
	maps.clear();

	fd_map.Close();
	defer_map.Close();

	if (ebpf)
		ebpf_dev_driver_destroy(ebpf);
}


void
CapsicumSandbox::FindInterpreter(Path exe)
{
	GElf_Phdr phdr;
	size_t filesize, i, phnum;
	FileDesc fd;
	Elf *elf;
	const char *s, *interp;

	fd = FileDesc::Open(exe.c_str(), O_RDONLY);
	if (!fd) {
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

			fexec_fd = FileDesc::Open(interp, O_RDONLY | O_EXEC);
			if (!fexec_fd) {
				err(1, "Failed to open rtld '%s'", interp);
			}

			is_rtld = true;
			goto out;
		}
	}

	/* Did not find an interpreter; must be statically linked. */
	fexec_fd = std::move(fd);

out:
	elf_end(elf);
}

void
CapsicumSandbox::PreopenDescriptors(const PermissionList &permList)
{
	cap_rights_t rights;
	FileDesc fd;
	int mode;

	for (const auto & [path, perm] : permList.GetPermMap()) {
		cap_rights_init(&rights, CAP_LOOKUP, CAP_FSTAT);

		/*
		 * Even if we're giving write permission to a file, we actually
		 * wind up opening its parent directory and you must open a
		 * directory with O_RDONLY.
		 */
		mode = O_RDONLY;

		if (perm & Permission::READ) {
			cap_rights_set(&rights, CAP_READ, CAP_SEEK, CAP_MMAP_R);
		}

		if (perm & Permission::WRITE) {
			cap_rights_set(&rights, CAP_WRITE, CAP_SEEK, CAP_MMAP_W,
			    CAP_CREATE, CAP_FTRUNCATE, CAP_RENAMEAT_SOURCE,
			    CAP_RENAMEAT_TARGET, CAP_UNLINKAT | CAP_MKDIRAT);
		}

		if (perm & Permission::EXEC) {
			cap_rights_set(&rights, CAP_FEXECVE, CAP_READ, CAP_MMAP_X);
			mode |= O_EXEC;
		}

		std::error_code code;
		Path openPath, filename;
		if (std::filesystem::is_directory(path)) {
			openPath = path;
		} else {
			openPath = path.parent_path();
			filename = path.filename();
		}

		fd = FileDesc::Open(openPath.c_str(), mode, 0600);
		if (!fd) {
			err(1, "Could not open '%s'", path.c_str());
		}

		if (cap_rights_limit(fd, &rights) < 0 && errno != ENOSYS) {
			err(1, "cap_rights_limit() failed");
		}

		if (path == work_dir) {
			work_dir_fd = fd;
		}

		descriptors.emplace_back(path, std::move(filename), std::move(fd));
	}
}

/* Indexes here must be kept in sync with the indexes used in the EBPF program */
int
CapsicumSandbox::GetDeferredIndex(const std::string & name)
{

	if (name == "defer_wait4") {
		return 0;
	} else {
		errx(1, "Unexpected deferred program '%s'", name.c_str());
	}
}

void
CapsicumSandbox::DefineProgram(GBPFElfWalker *walker, const char *n,
    struct ebpf_inst *prog, uint32_t prog_len)
{
	CapsicumSandbox *sandbox = reinterpret_cast<CapsicumSandbox*>(walker->data);

	std::string name(n);

	if (name.find("defer_") == 0) {
		sandbox->defer_programs.emplace_back(walker->driver, std::move(name),
		    EBPF_PROG_TYPE_VFS, prog, prog_len);

// 		fprintf(stderr, "Map prog '%s' to FD %d\n", n, sandbox->defer_programs.back().GetFD());
	} else {
		sandbox->probe_programs.emplace_back(walker->driver, std::move(name),
		    EBPF_PROG_TYPE_VFS, prog, prog_len);

// 		fprintf(stderr, "Map prog '%s' to FD %d\n", n, sandbox->probe_programs.back().GetFD());
	}
}

void
CapsicumSandbox::DefineMap(GBPFElfWalker *walker, const char *n, int desc,
    struct ebpf_map_def *map)
{
	CapsicumSandbox *sandbox = reinterpret_cast<CapsicumSandbox*>(walker->data);

	std::string name(n);

	if (name == "fd_map") {
		sandbox->fd_map = Ebpf::Map(walker->driver, std::move(name), desc);
	} else if (name == "fd_filename_map") {
		sandbox->fd_filename_map = Ebpf::Map(walker->driver, std::move(name), desc);
	} else if (name == "file_lookup_map") {
		sandbox->file_lookup_map = Ebpf::Map(walker->driver, std::move(name), desc);
	} else if (name == "defer_map") {
		sandbox->defer_map = Ebpf::Map(walker->driver, std::move(name), desc);
	} else if (name == "cwd_map") {
		sandbox->cwd_map = Ebpf::Map(walker->driver, std::move(name), desc);
	} else {
		sandbox->maps.emplace_back(walker->driver, std::move(name), desc);
	}
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

	if (!fd_map) {
		errx(1, "EBPF object did not define fd map");
	}

	int nextIndex = 0;
	for (const auto & desc : descriptors) {
		char path[MAXPATHLEN];

		bzero(path, sizeof(path));
		strlcpy(path, desc.lookup.c_str(), sizeof(path));

		file_lookup_map.UpdateElem(path, &nextIndex, EBPF_NOEXIST);

		int fd = desc.fd;
		fd_map.UpdateElem(&nextIndex, &fd, 0);

		bzero(path, NAME_MAX);
		strlcpy(path, desc.filename.c_str(), NAME_MAX);
		fd_filename_map.UpdateElem(&nextIndex, path, 0);

		nextIndex++;
	}

	for (const auto & prog : defer_programs) {
		int index = GetDeferredIndex(prog.GetName());
		int fd = prog.GetFD();

		int error = defer_map.UpdateElem(&index, &fd, 0);
		if (error != 0) {
			err(1, "Failed to insert program %s in defer_map", prog.GetName().c_str());
		}
	}
}

void
CapsicumSandbox::ArgvPrepend(std::vector<char*> & argp)
{

	if (is_rtld) {
		// Blame POSIX for the const_cast :(
		argp.push_back(const_cast<char*>("rtld"));
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
	pid_t pid;

	if (work_dir_fd != -1) {
		pid = getpid();
		error = cwd_map.UpdateElem(&pid, &work_dir_fd, EBPF_NOEXIST);
		if (error != 0) {
			err(1, "Failed to update cwd_map");
		}
	}

	for (Ebpf::Program & prog : probe_programs) {
		error = prog.AttachProbe();
		if (error != 0) {
			err(1, "Could not attach to '%s' ebpf probe",
			    prog.GetName().c_str());
		}
	}

	cap_enter();
}

void
CapsicumSandbox::ParentCleanup()
{
	/* Close all file descriptors we passed to the child process. */
	descriptors.clear();
}
