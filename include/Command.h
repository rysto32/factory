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

#ifndef PENDING_JOB_H
#define PENDING_JOB_H

#include <optional>
#include <string>
#include <vector>

#include "JobCompletion.h"
#include "Path.h"
#include "PermissionList.h"

class Job;
class Product;

typedef std::vector<Product*> ProductList;
typedef std::vector<std::string> ArgList;

class Command : public JobCompletion
{
	std::vector<Product*> products;

	ArgList argList;
	PermissionList permissions;
	Path workdir;
	std::optional<Path> stdin;
	std::optional<Path> stdout;
	bool queued;

public:
	Command(ProductList && products, ArgList && a, PermissionList && p, Path && wd,
	    std::optional<Path> && in, std::optional<Path> && out);
	virtual ~Command() = default;

	Command (const Command &) = delete;
	Command ( Command &&) = delete;

	Command & operator=(const Command &) = delete;
	Command & operator=( Command &&) = delete;

	virtual void JobComplete(Job * job, int status) override;
	virtual void Abort() override;

	const ArgList & GetArgList() const
	{
		return argList;
	}

	const PermissionList & GetPermissions() const
	{
		return permissions;
	}

	Path GetExecutable() const
	{
		return argList.at(0);
	}
	
	const Path & GetWorkDir() const
	{
		return workdir;
	}

	const std::optional<Path> & GetStdin() const
	{
		return stdin;
	}

	const std::optional<Path> & GetStdout() const
	{
		return stdout;
	}

	bool WasQueued() const
	{
		return queued;
	}

	void SetQueued()
	{
		queued = true;
	}
};

typedef std::unique_ptr<Command> CommandPtr;

#endif

