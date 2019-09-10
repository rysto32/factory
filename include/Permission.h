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

#ifndef PERMISSION_H
#define PERMISSION_H

#include <type_traits>

enum Permission
{
	NONE = 0x00,
	READ = 0x01,
	WRITE = 0x02,
	EXEC = 0x04,
	STAT = 0x08,
};

template <typename T>
auto
EnumToInt(T t)
{
	return static_cast<typename std::underlying_type<T>::type>(t);
}

inline Permission operator|(Permission a, Permission b)
{
	return static_cast<Permission>(
	    EnumToInt(a) | EnumToInt(b));
}

inline Permission operator&(Permission a, Permission b)
{
	return static_cast<Permission>(
	    EnumToInt(a) & EnumToInt(b));
}

inline Permission operator~(Permission a)
{
	return static_cast<Permission>(~EnumToInt(a));
}

inline Permission operator|=(Permission & a, Permission b)
{
	a = a | b;
	return a;
}

#endif
