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
#include <paths.h>
#include <unistd.h>

CommandFactory::CommandFactory(ProductManager &p)
  : productManager(p),
    factoryWorkDir(std::filesystem::current_path()),
    shellPath(GetShellPath())
{
}

std::vector<Path>
CommandFactory::GetShellPath()
{
	std::vector<Path> list;
	const char * pathStorage = getenv("PATH");
	if (!pathStorage || pathStorage[0] == '\0')
		pathStorage = _PATH_DEFPATH;

	std::string_view path(pathStorage);

	std::string_view::size_type dirStart;
	std::string_view::size_type dirEnd = 0;
	while (dirEnd != std::string_view::npos) {
		dirStart = path.find_first_not_of(":", dirEnd);
		dirEnd = path.find_first_of(":", dirStart);

		list.push_back(path.substr(dirStart, dirEnd - dirStart));
	}

	return list;
}

Path
CommandFactory::GetExecutablePath(Path path)
{
	if (!path.parent_path().empty())
		return path;

	for (const Path & dir : shellPath) {
		Path candidate = dir / path;

		int error = eaccess(candidate.c_str(), R_OK | X_OK);
		if (error == 0)
			return candidate;

		error = errno;
	}

	errx(1, "No executable '%s' in PATH", path.c_str());
}

void
CommandFactory::AddCommand(const std::vector<std::string> & productList,
    const std::vector<std::string> & inputPaths,
    std::vector<std::string> && argList,
    CommandOptions && options)
{
	PermissionList permList;
	std::vector<Product*> inputs, products;
	Path workdir;

	if (options.workdir)
		workdir = std::move(*options.workdir);
	else
		workdir = factoryWorkDir;

	Path exePath = GetExecutablePath(argList.front());
	Product * exe = productManager.GetProduct(exePath);
	inputs.push_back(exe);

	argList.front() = exePath.string();

	permList.AddPermission(exe->GetPath(), Permission::READ | Permission::EXEC);

	for (Path path : inputPaths) {
		if (path.is_relative()) {
			path = workdir / path;
		}
		Product * input = productManager.GetProduct(path);
		permList.AddPermission(input->GetPath(), Permission::READ);
		inputs.push_back(input);
	}

	for (auto & path : options.tmpdirs) {
		permList.AddPermission(path, Permission::READ | Permission::WRITE);
	}

	for (Path path : productList) {
		if (path.is_relative()) {
			path = workdir / path;
		}

		Product * product = productManager.GetProduct(path);
		permList.AddPermission(product->GetPath(), Permission::READ | Permission::WRITE);
		products.push_back(product);
		productManager.SetInputs(product, inputs);
	}

	commandList.emplace_back(std::make_unique<Command>(std::move(products), std::move(argList),
	    std::move(permList), std::move(workdir)));
}
