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
#include "Product.h"
#include "ProductManager.h"

#include <err.h>
#include <sys/stat.h>

CommandFactory::CommandFactory(ProductManager &p)
  : productManager(p)
{
}

void
CommandFactory::AddCommand(const std::vector<std::string> & productList,
    const std::vector<std::string> & inputPaths,
    std::vector<std::string> && argList,
    const std::vector<std::string> & tmpdirs)
{
	PermissionList permList;
	std::vector<Product*> inputs, products;

	Product * exe = productManager.GetProduct(argList.front());
	inputs.push_back(exe);

	permList.AddPermission(exe->GetPath(), Permission::READ | Permission::EXEC);

	for (auto & path : inputPaths) {
		Product * input = productManager.GetProduct(path);
		permList.AddPermission(input->GetPath(), Permission::READ);
		inputs.push_back(input);
	}

	for (auto & path : tmpdirs) {
		permList.AddPermission(path, Permission::READ | Permission::WRITE);
	}

	for (auto & path : productList) {
		Product * product = productManager.GetProduct(path);
		permList.AddPermission(product->GetPath(), Permission::READ | Permission::WRITE);
		products.push_back(product);
		productManager.SetInputs(product, inputs);
	}

	commandList.emplace_back(std::make_unique<Command>(std::move(products), std::move(argList), std::move(permList)));
}
