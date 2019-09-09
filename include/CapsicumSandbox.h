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

#ifndef CAPSICUM_SANDBOX_H
#define CAPSICUM_SANDBOX_H

#include "Sandbox.h"

#include "Path.h"

extern "C" {
#include <gbpf/gbpf.h>
}

#include <string>
#include <vector>

class Command;
class PermissionList;

class CapsicumSandbox : public Sandbox
{
	struct PreopenDesc
	{
		PreopenDesc(Path p, int f)
		  : path(std::move(p)), fd(f)
		{
		}

		PreopenDesc(const PreopenDesc &) = delete;

		PreopenDesc(PreopenDesc && d) noexcept
		  : path(std::move(d.path)), fd(d.fd)
		{
			d.fd = -1;
		}

		PreopenDesc & operator=(const PreopenDesc &) = delete;
		PreopenDesc & operator=(PreopenDesc &&) = delete;

		~PreopenDesc();

		Path path;
		int fd;
	};

	std::vector<PreopenDesc> descriptors;
	EBPFDevDriver *ebpf;

	int open_prog;
	int fd_map;

	int fexec_fd;
	int interp_fd;
	std::string interp_fd_str;

	void FindInterpreter(Path exe);
	void PreopenDescriptors(const PermissionList &);
	void CreateEbpfRules();

	static void DefineProgram(GBPFElfWalker *walker, const char *name,
			struct ebpf_inst *prog, uint32_t prog_len);
	static void DefineMap(GBPFElfWalker *walker, const char *name, int desc,
		       struct ebpf_map_def *map);

public:
	CapsicumSandbox(const Command & c);
	~CapsicumSandbox();

	CapsicumSandbox(CapsicumSandbox &&) = delete;
	CapsicumSandbox(const CapsicumSandbox &) = delete;

	CapsicumSandbox & operator=(CapsicumSandbox &&) = delete;
	CapsicumSandbox & operator=(const CapsicumSandbox &) = delete;

	virtual int GetExecFd() override;
	virtual void ArgvPrepend(std::vector<char*> & argp) override;
	virtual void Enable() override;
	virtual void ParentCleanup() override;
};

#endif

