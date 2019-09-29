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

#include "PermissionList.h"

#include <errno.h>
#include <fcntl.h>

void
PermissionList::AddPermission(const Path &path, Permission p)
{
	auto [it, success] = filePerm.emplace(path.lexically_normal(), p);
	if (!success) {
		it->second |= p;
	}
}

Permission
PermissionList::ModeToPermission(int mode)
{
	Permission p = Permission::NONE;

	switch (mode & O_ACCMODE) {
	case O_RDONLY:
		p = Permission::READ;
		break;
	case O_WRONLY:
		p = Permission::WRITE;
		break;
	case O_RDWR:
		p = Permission::READ | Permission::WRITE;
		break;
	}

	if (mode & O_EXEC)
		p |= Permission::EXEC;

	return (p);
}

int
PermissionList::CheckPerm(Permission allowed, int mode) const
{
	Permission requested = ModeToPermission(mode);
	if ((allowed & requested) != requested)
		return (EPERM);

	return (0);
}

int
PermissionList::IsPermitted(const Path & workdir, const Path & origPath, int mode) const
{
	Path path;
	if (origPath.is_relative()) {
		path = workdir / origPath;
	} else {
		path = origPath;
	}

	while (true) {
		auto it = filePerm.find(path);
		if (it != filePerm.end()) {
			return (CheckPerm(it->second, mode));
		}

		if (path == path.root_path()) {
			return (EPERM);
		}

		path = path.parent_path();
	}
}
