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

#ifndef PRODUCT_MANAGER_H
#define PRODUCT_MANAGER_H

#include "Path.h"
#include "NamedTarget.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class JobQueue;
class Product;

class ProductManager
{
private:
	typedef std::unordered_map<Product*, std::vector<Product*>> DepMap;
	typedef std::unordered_map<Path, std::unique_ptr<Product>> ProductMap;
	typedef std::unordered_map<std::string, NamedTarget> TargetMap;

	ProductMap products;
	JobQueue & jobQueue;

	DepMap inputMap;
	DepMap dependeeMap;
	DepMap dirContentsMap;
	TargetMap targetMap;
	std::vector<Product*> directories;

	static bool FileExists(const Path & path);

	void AddDependency(Product * product, Product * input);

	bool CheckNeedsBuild(Product * product, const Product * input);
	void CheckNeedsBuild(Product * product);

	Product * FindProduct(const Path &);
	Product * MakeProduct(const Path &);

	void SubmitProductJob(Product *product);

	bool IsBlocked(Product *product);
	void ReportCycle(Product * product);

	void CalcDeps();
	void CollectParentInputs(Product *product);
	void CollectInputTree(std::unordered_set<Product*> & set, Product *p);
	void AddDirProducts(Product *dir, std::unordered_set<Product*> & dirContents);

public:
	ProductManager(JobQueue &);

	ProductManager(const ProductManager &) = delete;
	ProductManager(ProductManager &&) = delete;
	ProductManager &operator=(const ProductManager &) = delete;
	ProductManager &operator=(ProductManager &&) = delete;

	Product * GetProduct(const Path &, bool makeParent = true);
	void SetInputs(Product * product, std::vector<Product*> inputs);

	void AddToTarget(std::string_view name, Product *p);

	void CheckBlockedCommands();
	void SubmitLeafJobs(const std::unordered_set<std::string_view> &targets);

	void ProductReady(Product *);
};

#endif

