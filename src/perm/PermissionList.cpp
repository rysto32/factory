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
PermissionList::AddFilePermission(const std::string &path, Permission p)
{
	filePerm.insert(std::make_pair(path, p));
}

void
PermissionList::AddDirPermission(const std::string & path, Permission p)
{
	dirPerm.emplace_back(path, p);
}

void
PermissionList::Finalize()
{
	std::sort(dirPerm.rbegin(), dirPerm.rend());
}

Permission
PermissionList::ModeToPermission(mode_t mode)
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
PermissionList::CheckDirPerms(const std::string & path, mode_t mode) const
{
	for (auto & perm : dirPerm) {
		if (perm.Matches(path))
			return CheckPerm(perm.perm, mode);
	}

	return (EPERM);
}

int
PermissionList::CheckPerm(Permission allowed, mode_t mode) const
{
	Permission requested = ModeToPermission(mode);
	if ((allowed & requested) != requested)
		return (EPERM);

	return (0);
}

int
PermissionList::IsPermitted(const std::string & path, mode_t mode) const
{
	auto it = filePerm.find(path);
	if (it == filePerm.end())
		return (CheckDirPerms(path, mode));

	return (CheckPerm(it->second, mode));
}

bool PermissionList::DirectoryPerm::Matches(const std::string & candidate) const
{
	if (path == candidate)
		return true;

	if (candidate.length() <= path.length())
		return false;

	if (candidate.substr(0, path.length()) == path && candidate.at(path.length()) == '/')
		return true;

	return false;
}
