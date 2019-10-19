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

#include "CapsicumSandbox.h"
#include "PermissionList.h"

#include <optional>
#include <string_view>

#include <err.h>
#include <libelf.h>
#include <string.h>
#include <unistd.h>

// Not defined by any header(!)
extern char ** environ;

static bool
accepts_param(const std::string_view flag)
{

	return (flag == "b" || flag == "-blocksize" ||
	    flag == "C" || flag == "-cd" || flag == "-directory" ||
	    flag == "-exclude" ||
	    flag == "f" || flag == "-file" ||
	    flag == "-format" ||
	    flag == "-gid" ||
	    flag == "-gname" ||
	    flag == "-include" ||
	    flag == "-newer" ||
	    flag == "-newer-mtime" ||
	    flag == "-newer-than" ||
	    flag == "-newer-mtime-than" ||
	    flag == "-older" ||
	    flag == "-older-mtime" ||
	    flag == "-older-than" ||
	    flag == "-older-mtime-than" ||
	    flag == "-options" ||
	    flag == "-passphrase" ||
	    flag == "s" ||
	    flag == "-strip-components" ||
	    flag == "-uid" ||
	    flag == "-uname");

}

static std::optional<std::string_view> NextArg(std::string_view rest, int i, int argc, char **argv)
{
	std::optional<std::string_view> next;

	if (!rest.empty()) {
		next = rest;
	} else if ((i + 1) < argc) {
		next = argv[i + 1];
	}

	return next;
}

int main(int argc, char **argv)
{
	PermissionList perms;
	std::optional<std::string_view> tarfile;
	bool writeTar = false;
	bool extract = false;
	bool preservePaths = false;
	const char *real_tar = "/usr/bin/tar";
	int i;

	if (elf_version(EV_CURRENT) == EV_NONE)
		errx(1, "ELF library initialization failed: %s",
		    elf_errmsg(-1));

	std::optional<Path> work_dir(std::filesystem::current_path());

	if (argc > 1) {
		if (argv[1][0] == '-') {
			/* Standard arguments */
			for (i = 1; i < argc; ) {
				if (argv[i][0] != '-') {
					break;
				}

				std::string_view flag;
				std::string_view rest;
				if (argv[i][1] == '-') {
					flag = argv[i] + 1;
				} else {
					flag = std::string_view(argv[i] + 1, 1);
					rest = std::string_view(argv[i] + 2);
				}

				while (1) {

					if (flag == "f" || flag == "-file") {
						tarfile = NextArg(rest, i, argc, argv);
					} else if (flag == "C" || flag == "-cd" || flag == "-directory") {
						work_dir = NextArg(rest, i, argc, argv);
					} else if (flag == "T" || flag == "-files-from" ||
					    flag == "X" || flag == "-exclude-from") {
						std::optional<std::string_view> file;

						file = NextArg(rest, i, argc, argv);

						if (file) {
							perms.AddPermission(*file, Permission::READ);
						}
					} else if (flag == "-newer-than" ||
					    flag == "-newer-mtime-than" ||
					    flag == "-older-than" ||
					    flag == "-older-mtime-than") {
						std::optional<std::string_view> file;

						file = NextArg(rest, i, argc, argv);

						if (file) {
							perms.AddPermission(*file, Permission::STAT);
						}

					} else if (flag == "P" || flag == "-absolute-paths") {
						preservePaths = true;
					} else if (flag == "x" || flag == "-extract") {
						extract = true;
					} else if (flag == "c" || flag == "-create" ||
					    flag == "r" || flag == "-append" ||
					    flag == "u" || flag == "-update") {
						    writeTar = true;
					}

					if (accepts_param(flag)) {
						if (rest.empty()) {
							// "-X arg"
							i += 2;
							break;
						} else {
							// "-Xarg"
							i += 1;
							break;
						}
					} else{
						if (!rest.empty()) {
							// "-abcd"
							flag = rest.substr(0, 1);
							rest = rest.substr(1);
						} else {
							i += 1;
							break;
						}
					}
				}
			}

			for (; i < argc; ++i) {
				if (argv[i][0] == '@') {
					perms.AddPermission(argv[i] + 1, Permission::READ);
				}
			}
		} else {
			/* Bundled arguments (legacy) */
			fprintf(stderr, "Bundled args not supported");
			exit(1);
		}
	}

	if (tarfile) {
		Permission p = Permission::READ;
		if (writeTar) {
			p |= Permission::WRITE;
		}

		perms.AddPermission(*tarfile, p);
	}

	if (work_dir) {
		Permission p = Permission::READ | Permission::EXEC;
		if (extract) {
			p |= Permission::WRITE;
		}
		perms.AddPermission(*work_dir, p);
	}

	perms.AddPermission(real_tar, Permission::READ | Permission::EXEC);
	perms.AddPermission("/lib", Permission::READ | Permission::EXEC);
	perms.AddPermission("/usr/lib", Permission::READ | Permission::EXEC);
	perms.AddPermission("/usr/share/nls", Permission::READ);
	perms.AddPermission("/usr/share/locale", Permission::READ);
	perms.AddPermission("/etc", Permission::READ);
	perms.AddPermission("/libexec", Permission::READ | Permission::EXEC);

	CapsicumSandbox sandbox(real_tar, perms, std::filesystem::current_path());

	sandbox.Enable();

	execve(real_tar, argv, environ);
	err(1, "execve %s failed", real_tar);
	_exit(1);
}
