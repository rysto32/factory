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
ProductManager::SetNeedsBuild(Product *p)
{
	fprintf(stderr, "'%s' needs build\n", p->GetPath().c_str());
	fullyBuilt.erase(p);
}

void
ProductManager::SubmitLeafJobs()
{
	for (Product * p : fullyBuilt) {
		p->BuildComplete(0);
	}
}

void
ProductManager::ProductReady(Product *p)
{
	if (fullyBuilt.count(p) == 0)
		jobQueue.Submit(p->GetPendingJob());
}
