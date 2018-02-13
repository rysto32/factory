/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Ryan Stone
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

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "interpose.h"
#include "SharedMem.h"

#define LD_PRELOAD_NAME "LD_PRELOAD"

static char *
fix_ld_preload(const char * orig_var, char **var_val)
{
	const char *start;
	char *dest;
	int i, orig_len, len, preload_set, lib_len, copied_libname;
	size_t sandbox_len;

	if (orig_var != NULL)
		orig_len = strlen(orig_var);
	else
		orig_len = 0;

	sandbox_len = strlen(shm->sandbox_lib) + 1;
	len = orig_len + sizeof(':') + sandbox_len;
	dest = malloc(len);

	/*
	 * XXX an abusive process that sets LD_PRELOAD twice will leak memory.
	 * It probably serves them right.
	 */
	*var_val = dest;

	// -1 to skip the null terminator
	memcpy(dest, LD_PRELOAD_NAME, sizeof(LD_PRELOAD_NAME) - 1);
	dest += sizeof(LD_PRELOAD_NAME) - 1;

	dest[0] = '=';
	dest++;

	preload_set = 0;
	copied_libname = 0;
	if (orig_var != NULL) {
		// We know that orig_var contains an '=' because our caller
		// checked for it
		i = 0;
		while (orig_var[i] != '=') {
			i++;
		}

		// + 1 skips over the '=' at the end
		i++;

		start = orig_var + i;
		while (orig_var[i] != '\0') {
			while (orig_var[i] != ' ' && orig_var[i] != ':' && orig_var[i] != '\0') {
				i++;
			}

			lib_len = (orig_var + i) - start;
			if (lib_len == sandbox_len - 1) {
				// Don't compare the NULL terminator
				if (memcmp(start, shm->sandbox_lib, sandbox_len - 1) == 0)
					preload_set = 1;
			}

			memcpy(dest, start, lib_len);
			dest += lib_len;

			while (orig_var[i] == ' ' || orig_var[i] == ':') {
				dest[0] = orig_var[i];
				i++;
				dest++;
			}

			start = orig_var + i;
			copied_libname = 1;
		}
	}

	if (preload_set) {
		dest[0] = '\0';
	} else {
		if (copied_libname) {
			dest[0] = ':';
			dest++;
		}
		memcpy(dest, shm->sandbox_lib, sandbox_len);
	}

	return (*var_val);
}

static char **
fix_envp(char * const orig_envp[], char **var_val)
{
	char ** new_envp;
	int i, count, ld_preload_set;

	*var_val = NULL;

	count = 0;
	if (orig_envp != 0) {
		for (; orig_envp[count] != NULL; ++count) {
		}
	}

	// count + 2 accounts for the NULL terminator in the array, plus the additional
	// LD_PRELOAD entry
	new_envp = calloc(sizeof(*new_envp), count + 2);
	if (new_envp == NULL)
		return (NULL);

	ld_preload_set = 0;
	for (i = 0; i < count; ++i) {
		if (strncmp(orig_envp[i], LD_PRELOAD_NAME "=", sizeof(LD_PRELOAD_NAME)) == 0) {
			new_envp[i] = fix_ld_preload(orig_envp[i], var_val);
			ld_preload_set = 1;
		} else {
			new_envp[i] = orig_envp[i];
		}
	}

	if (!ld_preload_set) {
		new_envp[i] = fix_ld_preload(NULL, var_val);
		++i;
	}

	new_envp[i] = NULL;
	return (new_envp);
}

int
_execve(const char *path, char * const argv[], char * const envp[])
{
	int fd;

	fd = open(path, O_EXEC);
	if (fd < 0)
		return (-1);

	return (fexecve(fd, argv, envp));
}

int
execve(const char *path, char * const argv[], char * const orig_envp[])
{
	return _execve(path, argv, orig_envp);
}

int
_fexecve(int fd, char * const argv[], char * const orig_envp[])
{
	char * var_val;
	char ** envp;
	int error;

	envp = fix_envp(orig_envp, &var_val);

	if (envp == NULL) {
		errno = ENOMEM;
		return (-1);
	}

	error = real_fexecve(fd, argv, envp);

	free(var_val);
	free(envp);
	return (error);
}

int
fexecve(int fd, char * const argv[], char * const orig_envp[])
{
	return _fexecve(fd, argv, orig_envp);
}
