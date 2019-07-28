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

Product::Product(const Path & p, ProductManager & mgr)
  : path(p), productManager(mgr)
{

}

bool
Product::SetPendingJob(PendingJobPtr && j)
{
	if (pendingJob)
		return false;

	pendingJob = std::move(j);
	return true;
}

void
Product::AddDependency(Product *d)
{
	assert (pendingJob);

	dependencies.insert(d);
	d->dependees.push_back(this);
}

void
Product::DependencyComplete(const Product * d)
{
	assert (pendingJob);

	dependencies.erase(d);
	if (dependencies.empty())
		productManager.ProductReady(this);
}

void
Product::BuildComplete(int status)
{
	if (WIFEXITED(status)) {
		int code = WEXITSTATUS(status);
		if (code == 0) {
			fprintf(stderr, "'%s' is built\n", path.c_str());
			for (Product * d : dependees)
				d->DependencyComplete(this);

		} else {
			fprintf(stderr, "%s: job exited with code %d\n",
			     path.c_str(), code);
			exit(1);
		}
	} else if(WIFSIGNALED(status)) {
		fprintf(stderr, "%s: job terminated on signal %d\n",
		     path.c_str(), WTERMSIG(status));
		exit(1);
	} else {
		fprintf(stderr, "%s: job terminated on unknown code %d\n",
		     path.c_str(), status);
		exit(1);
	}
}
