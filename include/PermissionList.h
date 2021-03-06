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

#include "Path.h"
#include "Permission.h"

#include <sys/types.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class PermissionList
{
public:
	typedef std::unordered_map<Path, Permission> PermMap;

private:
	PermMap filePerm;

	static Permission ModeToPermission(int);

	int CheckPerm(Permission allowed, int mode) const;

public:
	PermissionList() = default;
	PermissionList(PermissionList &&) = default;

	PermissionList(const PermissionList &) = delete;
	PermissionList &operator=(const PermissionList&) = delete;
	PermissionList &operator=(PermissionList &&) = delete;

	void AddPermission(const Path &, Permission);

	int IsPermitted(const Path & cwd, const Path &, int) const;

	const PermMap & GetPermMap() const
	{
		return filePerm;
	}
};

#endif
