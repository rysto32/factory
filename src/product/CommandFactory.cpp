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

#include "CommandFactory.h"

#include "Command.h"
#include "PermissionList.h"
#include "ProductManager.h"

#include <filesystem>

#include <err.h>
#include <sys/stat.h>

namespace fs = std::filesystem;

CommandFactory::CommandFactory(ProductManager &p)
  : productManager(p)
{
}

bool
CommandFactory::IsDirectory(const std::string & path)
{
	struct stat sb;
	int error;

	error = stat(path.c_str(), &sb);
	if (error != 0) {
		// XXX non-existent paths may be created a directories
		return false;
	}

	return sb.st_mode & S_IFDIR;
}

Product*
CommandFactory::ParsePermissionConf(PermissionList & permList, const PermissionConf & permConf) const
{
	Permission perm;

	if (strcmp(permConf.access, "r") == 0) {
		perm = Permission::READ;
	} else if (strcmp(permConf.access, "rw") == 0) {
		perm = Permission::READ | Permission::WRITE;
	} else if (strcmp(permConf.access, "x") == 0) {
		perm = Permission::READ | Permission::EXEC;
	} else {
		errx(1, "Unrecognized permission type '%s'", permConf.access);
	}

	Product::Type type;
	if (strcmp(permConf.type, "dir") == 0) {
		type = Product::Type::DIR;
	} else if (strcmp(permConf.type, "file") == 0) {
		type = Product::Type::FILE;
	} else {
		errx(1, "Unrecognized product type '%s'", permConf.access);
	}

	Path path(permConf.path);
	if (type == Product::Type::DIR) {
		permList.AddDirPermission(path, perm);
	} else {
		permList.AddFilePermission(path, perm);
	}

	return productManager.GetProduct(path, type);
}

void
CommandFactory::AddCommand(const std::vector<PermissionConf> & productList,
    const std::vector<PermissionConf> & permMap,
    std::vector<std::string> && argList)
{
	PermissionList permList;
	std::vector<Product*> inputs, products;

	Product * exe = productManager.GetProduct(argList.front(), Product::Type::FILE);
	inputs.push_back(exe);

	permList.AddFilePermission(exe->GetPath(), Permission::READ | Permission::EXEC);

	for (auto & permConf : permMap) {
		inputs.push_back(ParsePermissionConf(permList, permConf));
	}

	for (auto & permConf : productList) {
		Product * product = ParsePermissionConf(permList, permConf);
		products.push_back(product);
		productManager.SetInputs(product, inputs);
	}

	commandList.emplace_back(std::make_unique<Command>(std::move(products), std::move(argList), std::move(permList)));
}
