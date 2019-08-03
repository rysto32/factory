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

namespace fs = std::filesystem;

ProductManager::ProductManager(JobQueue & jq)
  : jobQueue(jq)
{
}

Product *
ProductManager::GetProduct(const Path & path, Product::Type type)
{
	auto it = products.find(path);
	if (it != products.end())
		return it->second.get();

	auto product = std::make_unique<Product>(path, type, *this);
	Product * ptr = product.get();
	products.insert(std::make_pair(path, std::move(product)));
	fullyBuilt.insert(ptr);

	Path parent(path.parent_path());
	std::error_code error;
	bool exists = fs::exists(parent, error);
	if (!exists || error) {
		Product * parentDir = GetProduct(parent, Product::Type::DIR);
		AddDependency(ptr, parentDir);
	}
	return ptr;
}

void
ProductManager::AddDependency(Product * product, Product * input)
{
	std::error_code error;

	product->AddDependency(input);
	fprintf(stderr, "%s depends on %s\n", product->GetPath().c_str(), input->GetPath().c_str());

	if (NeedsBuild(input)) {
		fprintf(stderr, "'%s' needs build because '%s' needs build\n", product->GetPath().c_str(), input->GetPath().c_str());
		SetNeedsBuild(product);
		return;
	}

	try {
		if (!ProductExists(product)) {
			SetNeedsBuild(product);
			fprintf(stderr, "'%s' needs build because it doesn't exist\n", product->GetPath().c_str());
			return;
		}

		if (!ProductExists(input)) {
			SetNeedsBuild(product);
			fprintf(stderr, "'%s' needs build because '%s' doesn't exist\n", product->GetPath().c_str(), input->GetPath().c_str());
			return;
		}

		if (input->IsDir()) {
			/*
			 * Directories are updated when any file in them is
			 * written to; do not rebuild if object is older than
			 * a directory it depends on as that's likely a false
			 * dependency.
			 */
			return;
		}

		auto inputLast = fs::last_write_time(input->GetPath());
		auto productLast = fs::last_write_time(product->GetPath());

		if (productLast < inputLast) {
			SetNeedsBuild(product);
			fprintf(stderr, "'%s' needs build because it is older than '%s'\n", product->GetPath().c_str(), input->GetPath().c_str());
		}
	} catch (fs::filesystem_error &e) {
		SetNeedsBuild(product);
		fprintf(stderr, "'%s' needs build because we got an error accessing it: %s\n", product->GetPath().c_str(), e.what());
	}
}

bool
ProductManager::ProductExists(const Product * product) const
{
	std::error_code error;

	return fs::exists(product->GetPath(), error) && !error;
}

void
ProductManager::SetInputs(Product * product, const std::vector<Product*> & inputs)
{
	int count = 0;
	for (Product * input : inputs) {
		if (product == input) {
			continue;
		}
		AddDependency(product, input);
		count++;
	}

	if (count == 0) {
		if (!ProductExists(product)) {
			SetNeedsBuild(product);
			fprintf(stderr, "'%s' needs build because it doesn't exist\n", product->GetPath().c_str());
			readyProducts.push_back(product);
		}
		return;
	}
}

void
ProductManager::SetNeedsBuild(const Product *p)
{
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

	for (Product * p : readyProducts)  {
		ProductReady(p);
	}
}

void
ProductManager::ProductReady(Product *p)
{
	fprintf(stderr, "%s is ready to build\n", p->GetPath().c_str());
	if (NeedsBuild(p))
		jobQueue.Submit(p->GetPendingJob());
}
