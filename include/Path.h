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

#ifndef PATH_H
#define PATH_H

#if defined(__clang__) && __clang_major__ == 6

#include <experimental/filesystem>
namespace std
{
	using namespace std::experimental;
}
#else
#include <filesystem>
#endif

#include <string_view>

class Path
{
private:
	std::filesystem::path path;

	static std::string_view StripTrailingSlashes(std::string_view p)
	{
		auto lastChar = p.find_last_not_of("/");
		if (lastChar == std::string::npos) {
			return p;
		} else {
			return p.substr(0, lastChar + 1);
		}
	}

	friend Path operator/(const Path &, const Path &);

public:
	Path() = default;

	Path(std::string_view p)
	  : path(StripTrailingSlashes(p))
	{
	}

	Path(const char *p)
	  : path(StripTrailingSlashes(p))
	{
	}

	Path(const std::string & p)
	  : path(StripTrailingSlashes(p))
	{
	}

	Path(const std::filesystem::path & p)
	  : path(StripTrailingSlashes(p.c_str()))
	{
	}

	operator const std::filesystem::path &() const
	{
		return path;
	}

	Path parent_path() const
	{
		return path.parent_path();
	}

	Path root_path() const
	{
		return path.root_path();
	}

	Path lexically_normal() const
	{
		return path.lexically_normal();
	}

	Path absolute(std::error_code & code) const
	{
		return std::filesystem::absolute(path, code);
	}

	Path weakly_canonical(std::error_code & code) const
	{
		return std::filesystem::weakly_canonical(path, code);
	}

	Path canonical(std::error_code & code) const
	{
		return std::filesystem::canonical(path, code);
	}

	Path filename() const
	{
		return path.filename();
	}

	bool is_relative() const
	{
		return path.is_relative();
	}

	bool operator ==(const Path & rhs) const
	{
		return path == rhs.path;
	}

	bool operator !=(const Path & rhs) const
	{
		return !(*this == rhs);
	}

	bool empty() const
	{
		return path.empty();
	}

	const char * c_str() const
	{
		return path.c_str();
	}

	std::string string() const
	{
		return path.string();
	}
};

inline Path operator/(const Path & lhs, const Path & rhs)
{
	return lhs.path / rhs.path;
}

namespace std
{
	template<>
	struct hash<Path>
	{
		size_t operator()(const Path & p) const
		{
			return std::filesystem::hash_value(p);
		}
	};
};

#endif

