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

#ifndef PERMSSION_LIST_H
#define PERMSSION_LIST_H

#include "Permission.h"

#include <sys/types.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class PermissionList
{
private:
	struct DirectoryPerm
	{
		std::string path;
		Permission perm;

		DirectoryPerm(const std::string & path, Permission perm)
		  : path(path), perm(perm)
		{
		}

		bool operator<(const DirectoryPerm & other) const
		{
			return path < other.path;
		}

		bool Matches(const std::string &) const;
	};

	typedef std::unordered_map<std::string, Permission> PermMap;
	typedef std::vector<DirectoryPerm> DirPermList;

	PermMap filePerm;
	DirPermList dirPerm;

	static Permission ModeToPermission(int);

	int CheckDirPerms(const std::string & path, int mode) const;
	int CheckPerm(Permission allowed, int mode) const;

public:
	PermissionList(const PermissionList &) = delete;
	PermissionList(PermissionList &&) = delete;
	PermissionList &operator=(const PermissionList&) = delete;
	PermissionList &operator=(PermissionList &&) = delete;

	void AddFilePermission(const std::string &, Permission);
	void AddDirPermission(const std::string &, Permission);
	void Finalize();

	int IsPermitted(const std::string &, int) const;
};

#endif
