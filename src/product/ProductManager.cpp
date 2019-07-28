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

#include "ProductManager.h"

#include "JobQueue.h"
#include "Product.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>

ProductManager::ProductManager(JobQueue & jq)
  : jobQueue(jq)
{
}

Product *
ProductManager::GetProduct(const Path & path)
{
	auto it = products.find(path);
	if (it != products.end())
		return it->second.get();

	auto product = std::make_unique<Product>(path, *this);
	Product * ptr = product.get();
	products.insert(std::make_pair(path, std::move(product)));
	fullyBuilt.insert(ptr);
	return ptr;
}

void
ProductManager::AddDependency(Product * dependant, Product * dependee)
{
	struct stat statDependee, statDependant;
	int error;

	dependant->AddDependency(dependee);

	if (NeedsBuild(dependee)) {
		SetNeedsBuild(dependant);
		return;
	}

	error = stat(dependee->GetPath().c_str(), &statDependee);
	if (error != 0) {
		SetNeedsBuild(dependant);
		return;
	}

	error = stat(dependant->GetPath().c_str(), &statDependant);
	if (error != 0) {
		SetNeedsBuild(dependant);
		return;
	}

	printf("Dependant: mtime %ld.%09ld %s\n", statDependant.st_mtim.tv_sec, statDependant.st_mtim.tv_nsec, dependant->GetPath().c_str());
	printf("Dependee:  mtime %ld.%09ld %s\n", statDependee.st_mtim.tv_sec, statDependee.st_mtim.tv_nsec, dependee->GetPath().c_str());
	if (statDependant.st_mtim.tv_sec < statDependee.st_mtim.tv_sec ||
	    (statDependant.st_mtim.tv_sec == statDependee.st_mtim.tv_sec &&
	    statDependant.st_mtim.tv_nsec < statDependee.st_mtim.tv_nsec)) {
		    SetNeedsBuild(dependant);
	}
}

void
ProductManager::SetNeedsBuild(const Product *p)
{
	fprintf(stderr, "'%s' needs build\n", p->GetPath().c_str());
	if (fullyBuilt.erase(const_cast<Product*>(p)) > 0) {
		for (const Product * d : p->GetDependees()) {
			SetNeedsBuild(d);
		}
	}
}

bool
ProductManager::NeedsBuild(const Product *p) const
{
	return fullyBuilt.count(const_cast<Product*>(p)) == 0;
}

void
ProductManager::SubmitLeafJobs()
{
	struct stat sb;
	int error;

	for (Product * p : fullyBuilt) {
		error = stat(p->GetPath().c_str(), &sb);
		if (error != 0) {
			if (errno == ENOENT) {
				errx(1, "No rule to build file '%s'", p->GetPath().c_str());
			}
			err(1, "Failed to stat '%s'", p->GetPath().c_str());
		}
		p->BuildComplete(0);
	}
}

void
ProductManager::ProductReady(Product *p)
{
	if (NeedsBuild(p))
		jobQueue.Submit(p->GetPendingJob());
}
