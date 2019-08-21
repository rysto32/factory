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
ProductManager::MakeProduct(const Path & path)
{
	auto product = std::make_unique<Product>(path, *this);
	Product * ptr = product.get();
	products.insert(std::make_pair(path, std::move(product)));

	return ptr;
}

Product *
ProductManager::GetProduct(const Path & path)
{
	Product * product = FindProduct(path);
	if (product != nullptr) {
		return product;
	}

	product = MakeProduct(path);

	return product;
}

Product *
ProductManager::FindProduct(const Path &path)
{
	auto it = products.find(path);
	if (it != products.end()) {
		return it->second.get();
	}

	return nullptr;
}

void
ProductManager::AddDependency(Product * product, Product * input)
{
	if (product == input)
		return;

	product->AddDependency(input);
// 	fprintf(stderr, "%s depends on %s\n", product->GetPath().c_str(), input->GetPath().c_str());
}

bool
ProductManager::CheckNeedsBuild(Product * product, const Product * input)
{

	if (input->NeedsBuild()) {
		fprintf(stderr, "'%s' needs build because '%s' needs build\n", product->GetPath().c_str(), input->GetPath().c_str());
		product->SetNeedsBuild();
		return true;
	}

	try {
		auto productStatus = fs::status(product->GetPath());
		if (!fs::exists(productStatus)) {
// 			fprintf(stderr, "'%s' needs build because it doesn't exist\n", product->GetPath().c_str());
			product->SetNeedsBuild();
			return true;
		}

		if (fs::is_directory(productStatus)) {
			/* A directory can not be rebuilt, so if it already exists, we are done. */
			return false;
		}

		auto inputStatus = fs::status(input->GetPath());
		if (!fs::exists(inputStatus)) {
// 			fprintf(stderr, "'%s' needs build because '%s' doesn't exist\n", product->GetPath().c_str(), input->GetPath().c_str());
			product->SetNeedsBuild();
			return true;
		}

		if (fs::is_directory(inputStatus)) {
			/*
			 * Directories are updated when any file in them is
			 * written to; do not rebuild if object is older than
			 * a directory it depends on as that's likely a false
			 * dependency.
			 */
			return false;
		}

		auto inputLast = fs::last_write_time(input->GetPath());
		auto productLast = fs::last_write_time(product->GetPath());

		if (productLast < inputLast) {
// 			fprintf(stderr, "'%s' needs build because it is older than '%s'\n", product->GetPath().c_str(), input->GetPath().c_str());
			product->SetNeedsBuild();
			return true;
		}
	} catch (fs::filesystem_error &e) {
// 		fprintf(stderr, "'%s' needs build because we got an error accessing it: %s\n", product->GetPath().c_str(), e.what());
		product->SetNeedsBuild();
		return true;
	}
	return false;
}

bool
ProductManager::FileExists(const Path & path)
{
	/*
	 * XXX The parent path of "foo" is "", which doesn't test as existing.
	 * Pretend that is does and hope for the best.
	 */
	if (path.empty()) {
		return true;
	}

	std::error_code error;

	return fs::exists(path, error) && !error;
}

void
ProductManager::SetInputs(Product * product, const std::vector<Product*> & inputs)
{
	for (Product * input : inputs) {
		if (product == input) {
			continue;
		}
		AddDependency(product, input);
	}
}

void
ProductManager::CheckNeedsBuild(Product * product)
{
	if (product->NeedsBuild())
		return;

	for (const Product * input : product->GetInputs()) {
		if(CheckNeedsBuild(product, input))
			return;
	}
}

void
ProductManager::SubmitLeafJobs()
{

	for (auto & [path, productPtr] : products) {
		Product * product = productPtr.get();

		Path parentPath(product->GetPath().parent_path());
		Product * parent = FindProduct(parentPath);

		if (parent) {
			AddDependency(product, parent);
		} else if (!FileExists(parentPath)) {
			errx(1, "No command to make product '%s', needed by '%s'",
			     parentPath.c_str(), product->GetPath().c_str());
		}

		CheckNeedsBuild(product);
	}

	for (auto & [path, productPtr] : products) {
		Product * product = productPtr.get();

		if (product->NeedsBuild() && product->IsReady()) {
// 			fprintf(stderr, "%s is ready\n", product->GetPath().c_str());
			jobQueue.Submit(product->GetPendingJob());
		}
	}
}

void
ProductManager::ProductReady(Product *p)
{
	if (p->NeedsBuild())
		jobQueue.Submit(p->GetPendingJob());
}
