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

CapsicumSandbox::CapsicumSandbox(const Path & exec, const PermissionList &perms, const Path &work_dir)
  : ebpf(ebpf_dev_driver_create()),
    work_dir(work_dir),
    is_rtld(false)
{
	if (!ebpf) {
		err(1, "Could not create ebpf instance.");
	}

	FindInterpreter(exec);
	PreopenDescriptors(perms);
	CreateEbpfRules();
}

CapsicumSandbox::~CapsicumSandbox()
{
	probe_programs.clear();
	maps.clear();

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
			cap_rights_set(&rights, CAP_READ, CAP_SEEK, CAP_MMAP_R,
			    CAP_FCHDIR | CAP_FCNTL);
		}

		if (perm & Permission::WRITE) {
			cap_rights_set(&rights, CAP_WRITE, CAP_SEEK, CAP_MMAP_W,
			    CAP_CREATE, CAP_FTRUNCATE, CAP_RENAMEAT_SOURCE,
			    CAP_RENAMEAT_TARGET, CAP_UNLINKAT | CAP_MKDIRAT |
			    CAP_SYMLINKAT | CAP_FUTIMES | CAP_FCHMODAT |
			    CAP_FCHOWN | CAP_LINKAT_SOURCE | CAP_LINKAT_TARGET |
			    CAP_FCHFLAGS);
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
			err(1, "Could not preopen '%s'", path.c_str());
		}

		if (cap_rights_limit(fd, &rights) < 0 && errno != ENOSYS) {
			err(1, "cap_rights_limit() failed");
		}

		descriptors.emplace_back(path, std::move(filename), std::move(fd));
	}
}

void
CapsicumSandbox::DefineProgram(GBPFElfWalker *walker, const char *n,
    struct ebpf_inst *prog, uint32_t prog_len)
{
	CapsicumSandbox *sandbox = reinterpret_cast<CapsicumSandbox*>(walker->data);

	sandbox->probe_programs.emplace(n, Ebpf::Program(walker->driver, n,
	    EBPF_PROG_TYPE_VFS, prog, prog_len));

// 		fprintf(stderr, "Map prog '%s' to FD %d\n", n, sandbox->probe_programs.back().GetFD());
}

void
CapsicumSandbox::DefineMap(GBPFElfWalker *walker, const char *n, int desc,
    struct ebpf_map_def *map)
{
	CapsicumSandbox *sandbox = reinterpret_cast<CapsicumSandbox*>(walker->data);

	sandbox->maps.emplace(std::string(n), Ebpf::Map(walker->driver, n, desc));
}

void
CapsicumSandbox::UpdateProgMap(const std::string & mapName, const std::string & progName)
{
	auto & map = maps[mapName];
	if (!map) {
		errx(1, "Map '%s' not defined by object", mapName.c_str());
	}
	
	const auto & prog = probe_programs[progName];
	if (!prog) {
		errx(1, "Program '%s' not defined by object", progName.c_str());
	}

	int index = 0;
	int fd = prog.GetFD();

	int error = map.UpdateElem(&index, &fd, 0);
	if (error != 0) {
		err(1, "Failed to insert program %s in map", prog.GetName().c_str());
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

	error = gbpf_walk_elf(&walker, &ebpf->base, "/home/rstone/git/factory/src/capsicum/ebpf_progs/open/open.o");
	if (error != 0) {
		err(1, "Could not walk EBPF object");
	}

	auto & fd_map = maps["fd_map"];
	if (!fd_map) {
		errx(1, "EBPF object did not define fd map");
	}
	
	auto & file_lookup_map = maps["file_lookup_map"];
	if (!file_lookup_map) {
		errx(1, "EBPF object did not define file_lookup_map");
	}
	
	auto & fd_filename_map = maps["fd_filename_map"];
	if (!fd_filename_map) {
		errx(1, "EBPF object did not define fd_filename_map");
	}
		

	int nextIndex = 0;
	for (const auto & desc : descriptors) {
		char path[MAXPATHLEN];

		bzero(path, sizeof(path));
		strlcpy(path, desc.lookup.c_str(), sizeof(path));

		error = file_lookup_map.UpdateElem(path, &nextIndex, EBPF_NOEXIST);
		if (error != 0) {
			err(1, "Could not insert '%s' at index %d in file_lookup_map", path, nextIndex);
		}

// 		fprintf(stderr, "Insert %s -> %d\n", path, nextIndex);

		int fd = desc.fd;
		error = fd_map.UpdateElem(&nextIndex, &fd, 0);
		if (error != 0) {
			err(1, "Could not insert '%s' at index %d in fd_map", path, nextIndex);
		}
// 		fprintf(stderr, "Insert %d -> %d\n", nextIndex, fd);

		bzero(path, NAME_MAX);
		strlcpy(path, desc.filename.c_str(), NAME_MAX);
		error = fd_filename_map.UpdateElem(&nextIndex, path, 0);
		if (error != 0) {
			err(1, "Could not insert '%s' at index %d in fd_filename_map", path, nextIndex);
		}
// 		fprintf(stderr, "Insert %d -> '%s'\n", nextIndex, path);

		nextIndex++;
	}

	UpdateProgMap("pdwait_prog", "defer_wait4");
	UpdateProgMap("kevent_prog", "defer_kevent");
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

static bool
HasEnding(std::string const &fullString, std::string const &ending)
{
	if (fullString.length() >= ending.length()) {
		return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
	} else {
		return false;
	}
}

void
CapsicumSandbox::Enable()
{
	int error;
	pid_t pid;
	char path[MAXPATHLEN];

	pid = getpid();

	bzero(path, sizeof(path));
	strlcpy(path, work_dir.c_str(), sizeof(path));
	auto & cwd_name_map = maps["cwd_name_map"];
	error = cwd_name_map.UpdateElem(&pid, path, EBPF_NOEXIST);
	if (error != 0) {
		err(1, "Failed to update cwd_name_map (fd %d)", cwd_name_map.GetFD());
	}

	for (auto & [name, prog] : probe_programs) {
		if (HasEnding(name, "_probe")) {
			error = prog.AttachProbe();
			if (error != 0) {
				err(1, "Could not attach to '%s' ebpf probe",
				    name.c_str());
			}
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
