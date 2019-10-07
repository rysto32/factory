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

	if (!FileExists(path)) {
		ptr->SetNeedsBuild();
	}

	return ptr;
}

Product *
ProductManager::GetProduct(const Path & path, bool makeParent)
{
	bool madeProduct = false;

	Product * product = FindProduct(path);
	if (product == nullptr) {
		product = MakeProduct(path);
		madeProduct = true;
	}

	if (makeParent) {
		Product * parent = GetProduct(path.parent_path(), false);
		if (parent->SetDirectory()) {
			directories.push_back(parent);
		}

		if (madeProduct) {
			dirContentsMap[parent].push_back(product);
		}
	}

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
// 		fprintf(stderr, "'%s' needs build because '%s' needs build\n", product->GetPath().c_str(), input->GetPath().c_str());
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
ProductManager::SetInputs(Product * product, std::vector<Product*> inputs)
{
	for (Product *input : inputs) {
		dependeeMap[input].push_back(product);
	}

	std::vector<Product*> & mapInputs = inputMap[product];
	if (mapInputs.empty()) {
		mapInputs = std::move(inputs);
	} else {
		for (Product * p : inputs) {
			mapInputs.push_back(p);
		}
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
ProductManager::AddDirProducts(Product *dir, std::unordered_set<Product*> & dirContents)
{

	for (Product *p : dirContentsMap[dir]) {
		if (p->IsDirectory()) {
			AddDirProducts(p, dirContents);
		} else {
			dirContents.insert(p);
		}
	}
}

void
ProductManager::CalcDeps()
{
	for (Product *dir : directories) {
		if (dependeeMap[dir].empty()) {
			continue;
		}

// 		fprintf(stderr, "Enumerate '%s' for '%s'\n", dir->GetPath().c_str(),
// 		     dependeeMap[dir].front()->GetPath().c_str());
		std::unordered_set<Product*> dirContents;

		using opt = fs::directory_options;
		std::error_code error;
		auto dirIt = fs::recursive_directory_iterator(dir->GetPath(),
		    opt::skip_permission_denied | opt::follow_directory_symlink,
		    error);

		if (!error) {
			for (auto & entry : dirIt) {
				dirContents.insert(GetProduct(entry.path(), false));
			}
		}

		AddDirProducts(dir, dirContents);

		for (Product *dependee : dependeeMap[dir]) {
			for (Product *input : dirContents) {
				AddDependency(dependee, input);
// 				fprintf(stderr, "'%s' depends on dir contents '%s'\n",
// 				    dependee->GetPath().c_str(),
// 				    input->GetPath().c_str());
			}
		}
	}

	for (auto & [product, inputs] : inputMap) {
		for (Product *input : inputs) {
			if (!input->IsDirectory()) {
				AddDependency(product, input);
// 				fprintf(stderr, "'%s' depends on dir contents '%s'\n",
// 				    product->GetPath().c_str(),
// 				    input->GetPath().c_str());
			}
		}
	}
}

void
ProductManager::SubmitLeafJobs()
{
	CalcDeps();

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
			SubmitProductJob(product);
		}
	}
}

void
ProductManager::ProductReady(Product *p)
{
	if (p->NeedsBuild())
		SubmitProductJob(p);
}

void
ProductManager::SubmitProductJob(Product *product)
{
	Command * c = product->GetCommand();

	if (!c) {
		Product * dependee = product->GetDependees().front();
		errx(1, "No rule to make product '%s', needed by '%s'",
		    product->GetPath().c_str(), dependee->GetPath().c_str());
	}

	jobQueue.Submit(c);
}

void
ProductManager::CheckBlockedCommands()
{
	for (auto & [path, product] : products) {
		if (IsBlocked(product.get()))
			ReportCycle(product.get());
	}
}

bool
ProductManager::IsBlocked(Product *product)
{

	if (!product->NeedsBuild())
		return false;

	if (!product->IsBuildable()) {
		/*
		 * Could happen if we have a dependency cycle and are
		 * also lacking a build rule.
		 */
		Product * dependee = product->GetDependees().front();
		errx(1, "No rule to make product '%s', needed by '%s'",
			product->GetPath().c_str(), dependee->GetPath().c_str());
	}

	// If the command was queued then it wasn't blocked
	Command *command = product->GetCommand();
	if (command->WasQueued())
		return false;

	return true;
}

void
ProductManager::ReportCycle(Product *product)
{
	std::unordered_set<Product*> seenNodes;

	fprintf(stderr, "Dependency cycle blocks '%s'\n", product->GetPath().c_str());
	while (seenNodes.count(product) == 0) {
		Product * blocked = nullptr;
		for (Product * input : product->GetInputs()) {
			if (IsBlocked(input)) {
				blocked = input;
				break;
			}
		}

		/* Should be impossible. */
		if (blocked == nullptr)
			errx(1, "Couldn't find cycle");

		fprintf(stderr, "\tDepends on %s\n", blocked->GetPath().c_str());
		seenNodes.insert(product);
		product = blocked;
	}

	errx(1, "Build terminated due to cycle");
}
