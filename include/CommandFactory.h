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

#ifndef COMMAND_FACTORY_H
#define COMMAND_FACTORY_H

#include "Path.h"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class Command;
class Product;
class PermissionList;
class ProductManager;

struct CommandOptions
{
	std::vector<std::string> tmpdirs;
	std::optional<Path> workdir;
	std::optional<Path> stdin;
	std::optional<Path> stdout;
};

class CommandFactory
{
	ProductManager &productManager;
	Path factoryWorkDir;
	std::vector<std::unique_ptr<Command>> commandList;
	std::vector<Path> shellPath;

	static std::vector<Path> GetShellPath();

	Path GetExecutablePath(Path path);

public:
	CommandFactory(ProductManager &);
	void AddCommand(const std::vector<std::string> & products,
	    const std::vector<std::string> & inputs,
	    std::vector<std::string> && argList,
	    CommandOptions && options);
};

#endif

