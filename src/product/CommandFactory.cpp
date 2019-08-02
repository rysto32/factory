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

#include <err.h>
#include <sys/stat.h>

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

void
CommandFactory::AddCommand(const std::vector<std::string> & productList,
    const std::unordered_map<std::string, std::vector<std::string>> & permMap,
    std::vector<std::string> && argList)
{
	PermissionList permList;
	std::vector<Product*> inputs, products;

	for (auto & [permType, paths] : permMap) {
		Permission perm;
		if (permType == "ro") {
			perm = Permission::READ;
		} else {
			errx(1, "Unrecognized permission type '%s'", permType.c_str());
		}

		for (std::string p : paths) {
			if (IsDirectory(p)) {
				permList.AddDirPermission(p, perm);
			} else {
				permList.AddFilePermission(p, perm);
			}

			inputs.push_back(productManager.GetProduct(p));
		}
	}

	for (const std::string & path : productList) {
		Product * product = productManager.GetProduct(path);
		products.push_back(product);
		productManager.SetInputs(product, inputs);

		permList.AddFilePermission(path, Permission::READ | Permission::WRITE);
	}

	permList.Finalize();

	commandList.emplace_back(std::make_unique<Command>(std::move(products), std::move(argList), std::move(permList)));
}
