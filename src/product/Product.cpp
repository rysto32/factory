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

#include "Product.h"

#include "ProductManager.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <err.h>

Product::Product(const Path & p, ProductManager & mgr)
  : path(p),
    command(nullptr),
    productManager(mgr),
    needsBuild(false),
    isDirectory(false),
    statusValid(false)
{

}

bool
Product::SetCommand(Command * c)
{
	if (command)
		return false;

	command = std::move(c);
	return true;
}

void
Product::AddDependency(Product *d)
{

	dependencies.insert(d);
	d->dependees.push_back(this);
}

void
Product::DependencyComplete(Product * d)
{
	if (!command) {
		errx(1, "Internal error: product '%s' has no defined command", path.c_str());
	}

	dependencies.erase(d);
	if (dependencies.empty())
		productManager.ProductReady(this);
}

void
Product::BuildComplete(int status, uintmax_t jobId)
{
	if (WIFEXITED(status)) {
		int code = WEXITSTATUS(status);
		if (code == 0) {
			fprintf(stderr, "Job %jd: '%s' is built\n", jobId, path.c_str());
			for (Product * d : dependees)
				d->DependencyComplete(this);

		} else {
			fprintf(stderr, "Job %jd: %s: job exited with code %d\n",
			     jobId, path.c_str(), code);
			exit(1);
		}
	} else if(WIFSIGNALED(status)) {
		fprintf(stderr, "Job %jd: %s: job terminated on signal %d\n",
		     jobId, path.c_str(), WTERMSIG(status));
		exit(1);
	} else {
		fprintf(stderr, "Job %jd: %s: job terminated on unknown code %d\n",
		     jobId, path.c_str(), status);
		exit(1);
	}
}

void
Product::SetNeedsBuild()
{
	if (needsBuild)
		return;

	needsBuild = true;
	for (Product * p : dependees) {
// 		fprintf(stderr, "'%s' needs build because '%s' needs build\n", p->path.c_str(), path.c_str());
		p->SetNeedsBuild();
	}
}

bool
Product::IsReady()
{

	auto it = dependencies.begin();
	while (it != dependencies.end()) {
		Product * input = *it;
		if (!input->NeedsBuild()) {
			it = dependencies.erase(it);
		} else {
			++it;
// 			fprintf(stderr, "%s is not ready: %s needs build\n", path.c_str(), input->path.c_str());
		}
	}

	return dependencies.empty();
}

void
Product::CacheStatus() const
{
	status = std::filesystem::status(path);
	modTime = std::filesystem::last_write_time(path);
	statusValid = true;
}

const std::filesystem::file_status &
Product::GetStatus() const
{
	if (!statusValid) {
		CacheStatus();
	}

	return status;
}

std::filesystem::file_time_type
Product::GetModifyTime() const
{
	if (!statusValid) {
		CacheStatus();
	}

	return modTime;
}
