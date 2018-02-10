/*-
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

#include "mock/GlobalMock.h"

#include "interpose.h"
#include "SharedMem.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

using namespace testing;

class MockExec : public GlobalMockBase<MockExec>
{
public:
	MOCK_METHOD3(exec, int (std::string , char * const argv[], char *const envp[]));
	MOCK_METHOD3(fexec, int (int, char * const argv[], char *const envp[]));
};

class ArgMatcher
{
private:
	std::vector<const char *> expected;

public:
	ArgMatcher(const std::vector<const char *>  &a)
	  : expected(a)
	{
	}

	typedef bool result_type;
	result_type operator()(char * const argv[])
	{
		size_t i;
		for (i = 0; i < expected.size(); ++i) {
			if (strcmp(argv[i], expected[i]) != 0) {
				return false;
			}
		}

		return true;
	}
};

class EnvVar
{
private:
	std::string name;
	std::string value;

	std::string env;

public:
	EnvVar(const std::string & name, const std::string & value)
	  : name(name), value(value)
	{
		env = name + "=" + value;
	}

	const std::string & GetEnv() const
	{
		return env;
	}
};

class ExecEnvironment
{
private:
	std::vector<EnvVar> variables;
	std::vector<std::unique_ptr<char []>> environMemory;

public:
	typedef std::vector<EnvVar>::const_iterator const_iterator;

	const_iterator begin() const
	{
		return variables.begin();
	}

	const_iterator end() const
	{
		return variables.end();
	}

	EnvVar & at(size_t i)
	{
		return variables.at(i);
	}

	const EnvVar & at(size_t i) const
	{
		return variables.at(i);
	}

	size_t size() const
	{
		return variables.size();
	}

	void emplace_back(const std::string & name, const std::string & value)
	{
		variables.emplace_back(name, value);
	}

	std::vector<char *>
	GetEnviron()
	{
		std::vector<char *> environ;

		for (const auto & var : variables) {
			const std::string & envPair = var.GetEnv();
			auto ptr = std::make_unique<char[]>(envPair.size() + 1);
			strcpy(ptr.get(), envPair.c_str());

			environ.push_back(ptr.get());
			environMemory.emplace_back(std::move(ptr));
		}

		environ.push_back(NULL);
		return environ;
	}
};

class EnvMatcher
{
private:
	const ExecEnvironment & expected;

public:
	EnvMatcher(const ExecEnvironment & expected)
	  : expected(expected)
	{
	}

	typedef bool result_type;
	result_type operator()(char * const argv[])
	{
		size_t i;
		for (i = 0; i < expected.size(); ++i) {
			if (argv[i] != expected.at(i).GetEnv()) {
				std::cerr << argv[i] << " != " << expected.at(i).GetEnv() << "\n";
				return false;
			}
		}

		return true;
	}
};

class GlobalMockExec : private GlobalMock<MockExec>
{
public:
	void ExpectExec(const std::string & path,
	    const std::vector<const char *> & args, const ExecEnvironment & env,
	    int error)
	{
		EXPECT_CALL(**this, exec(path, ResultOf(ArgMatcher(args), true), ResultOf(EnvMatcher(env), true)))
		    .Times(1)
		    .WillOnce(Return(error));
	}
};

static bool fail_calloc = false;

extern "C" {
static int mock_execve(const char * path, char * const argv[], char *const envp[])
{
	return MockExec::MockObj().exec(path, argv, envp);
}

static int mock_fexecve(int fd, char * const argv[], char *const envp[])
{
	return MockExec::MockObj().fexec(fd, argv, envp);
}

void * mock_calloc(size_t size, size_t nobjs)
{
	if (fail_calloc)
		return NULL;

	return calloc(size, nobjs);
}

int wrapped_execve(const char * path, char *const argv[], char *const envp[]);
int wrapped_fexecve(int fd, char *const argv[], char *const envp[]);

execve_t * real_execve = mock_execve;
fexecve_t * real_fexecve = mock_fexecve;

static struct FactoryShm mock_shm = {
	.header = {
		.size = sizeof(struct FactoryShm),
		.api_num = SHARED_MEM_API_NUM,
	},
};

struct FactoryShm *shm = &mock_shm;
}

static int
call_wrapped_execve(const char * path, const char *const argv[], char *const envp[])
{
	// This const_casts are ugly but are necessary to work around POSIX
	// definine execve() with really inconvenient parameter types
	return wrapped_execve(path, const_cast<char * const *>(argv),
	    const_cast<char * const *>(envp));
}

#if 0
static int
call_wrapped_fexecve(int fd, const char *const argv[], const char *const envp[])
{
	return wrapped_fexecve(fd, const_cast<char * const *>(argv),
	    const_cast<char * const *>(envp));
}
#endif

class ExecTestSuite : public ::testing::Test
{
public:
	void SetUp()
	{
		fail_calloc = false;
	}
};

TEST_F(ExecTestSuite, ExecNullEnv)
{
	GlobalMockExec mockExec;
	std::string path("/bin/false");
	std::vector<const char *> args;
	ExecEnvironment env;
	std::string wrapper_path("/tmp/libfactory_wrapper.so.1");
	strlcpy(shm->sandbox_lib, wrapper_path.c_str(), sizeof(shm->sandbox_lib));

	args.push_back(path.c_str());

	env.emplace_back("LD_PRELOAD", wrapper_path);

	mockExec.ExpectExec(path, args, env, 0);

	call_wrapped_execve(path.c_str(), &args[0], NULL);
}

TEST_F(ExecTestSuite, ExecEmptyEnv)
{
	GlobalMockExec mockExec;
	std::string path("/bin/false");
	std::vector<const char *> args;
	ExecEnvironment env;
	std::string wrapper_path("/tmp/libfactory_wrapper.so.1");
	strlcpy(shm->sandbox_lib, wrapper_path.c_str(), sizeof(shm->sandbox_lib));

	args.push_back(path.c_str());

	std::vector<char *> environ = env.GetEnviron();
	env.emplace_back("LD_PRELOAD", wrapper_path);

	mockExec.ExpectExec(path, args, env, 0);

	call_wrapped_execve(path.c_str(), &args[0], &environ[0]);
}

TEST_F(ExecTestSuite, ExecNoPreloadEnv)
{
	GlobalMockExec mockExec;
	std::string path("/bin/false");
	std::vector<const char *> args;
	ExecEnvironment env;
	std::string wrapper_path("/tmp/libfactory_wrapper.so.1");
	strlcpy(shm->sandbox_lib, wrapper_path.c_str(), sizeof(shm->sandbox_lib));

	args.push_back(path.c_str());

	env.emplace_back("PATH", "/bin:/sbin:/usr/bin:/usr/sbin");
	env.emplace_back("USER", "root");
	env.emplace_back("HOME", "/root");

	std::vector<char *> environ = env.GetEnviron();
	env.emplace_back("LD_PRELOAD", wrapper_path);

	mockExec.ExpectExec(path, args, env, 0);

	call_wrapped_execve(path.c_str(), &args[0], &environ[0]);
}

TEST_F(ExecTestSuite, ExecPreloadSingle)
{
	GlobalMockExec mockExec;
	std::string path("/bin/false");
	std::vector<const char *> args;
	ExecEnvironment env;
	std::string wrapper_path("/tmp/libfactory_wrapper.so.1");
	strlcpy(shm->sandbox_lib, wrapper_path.c_str(), sizeof(shm->sandbox_lib));

	args.push_back(path.c_str());

	env.emplace_back("PATH", "/bin:/sbin:/usr/bin:/usr/sbin");
	env.emplace_back("USER", "root");

	size_t preload_index = env.size();
	env.emplace_back("LD_PRELOAD", "/lib/libthr.so.3");

	env.emplace_back("HOME", "/root");

	std::vector<char *> environ = env.GetEnviron();
	env.at(preload_index) = EnvVar("LD_PRELOAD", "/lib/libthr.so.3:" + wrapper_path);

	mockExec.ExpectExec(path, args, env, 0);

	call_wrapped_execve(path.c_str(), &args[0], &environ[0]);
}

TEST_F(ExecTestSuite, ExecPreloadMulti)
{
	GlobalMockExec mockExec;
	std::string path("/bin/false");
	std::vector<const char *> args;
	ExecEnvironment env;
	std::string wrapper_path("/tmp/libfactory_wrapper.so.1");
	strlcpy(shm->sandbox_lib, wrapper_path.c_str(), sizeof(shm->sandbox_lib));

	args.push_back(path.c_str());

	env.emplace_back("PATH", "/bin:/sbin:/usr/bin:/usr/sbin");
	env.emplace_back("USER", "root");

	size_t preload_index = env.size();


	// Pass a path that is the same length as the wrapper path into LD_PRELOAD
	// and ensure that execve() probably detects that the wrapper hasn't
	// already been passed
	std::string almost_wrapper(wrapper_path);
	almost_wrapper[10] = 'd';

	env.emplace_back("LD_PRELOAD", "/lib/libthr.so.3:" + almost_wrapper + ":/usr/lib/libdtrace.so.1");

	env.emplace_back("HOME", "/root");

	std::vector<char *> environ = env.GetEnviron();
	env.at(preload_index) = EnvVar("LD_PRELOAD", "/lib/libthr.so.3:" + almost_wrapper + ":/usr/lib/libdtrace.so.1:" + wrapper_path);

	mockExec.ExpectExec(path, args, env, 0);

	call_wrapped_execve(path.c_str(), &args[0], &environ[0]);
}

TEST_F(ExecTestSuite, ExecEmptyPreloadEnv)
{
	GlobalMockExec mockExec;
	std::string path("/bin/false");
	std::vector<const char *> args;
	ExecEnvironment env;
	std::string wrapper_path("/tmp/libfactory_wrapper.so.1");
	strlcpy(shm->sandbox_lib, wrapper_path.c_str(), sizeof(shm->sandbox_lib));

	args.push_back(path.c_str());

	env.emplace_back("PATH", "/bin:/sbin:/usr/bin:/usr/sbin");
	env.emplace_back("USER", "root");

	size_t preload_index = env.size();
	env.emplace_back("LD_PRELOAD", "");

	env.emplace_back("HOME", "/root");

	std::vector<char *> environ = env.GetEnviron();
	env.at(preload_index) = EnvVar("LD_PRELOAD", wrapper_path);

	mockExec.ExpectExec(path, args, env, 0);

	call_wrapped_execve(path.c_str(), &args[0], &environ[0]);
}

TEST_F(ExecTestSuite, ExecWrapperPreloadedOnly)
{
	GlobalMockExec mockExec;
	std::string path("/bin/false");
	std::vector<const char *> args;
	ExecEnvironment env;
	std::string wrapper_path("/tmp/libfactory_wrapper.so.1");
	strlcpy(shm->sandbox_lib, wrapper_path.c_str(), sizeof(shm->sandbox_lib));

	args.push_back(path.c_str());

	env.emplace_back("PATH", "/bin:/sbin:/usr/bin:/usr/sbin");
	env.emplace_back("USER", "root");
	env.emplace_back("LD_PRELOAD", wrapper_path);
	env.emplace_back("HOME", "/root");

	std::vector<char *> environ = env.GetEnviron();

	mockExec.ExpectExec(path, args, env, 0);

	call_wrapped_execve(path.c_str(), &args[0], &environ[0]);
}

TEST_F(ExecTestSuite, ExecWrapperPreloadedFirst)
{
	GlobalMockExec mockExec;
	std::string path("/bin/false");
	std::vector<const char *> args;
	ExecEnvironment env;
	std::string wrapper_path("/tmp/libfactory_wrapper.so.1");
	strlcpy(shm->sandbox_lib, wrapper_path.c_str(), sizeof(shm->sandbox_lib));

	args.push_back(path.c_str());

	env.emplace_back("PATH", "/bin:/sbin:/usr/bin:/usr/sbin");
	env.emplace_back("USER", "root");
	env.emplace_back("LD_PRELOAD", wrapper_path + ":/usr/lib/libcam.so.2");
	env.emplace_back("HOME", "/root");

	std::vector<char *> environ = env.GetEnviron();

	mockExec.ExpectExec(path, args, env, 0);

	call_wrapped_execve(path.c_str(), &args[0], &environ[0]);
}

TEST_F(ExecTestSuite, ExecWrapperPreloadedMiddle)
{
	GlobalMockExec mockExec;
	std::string path("/bin/false");
	std::vector<const char *> args;
	ExecEnvironment env;
	std::string wrapper_path("/tmp/libfactory_wrapper.so.1");
	strlcpy(shm->sandbox_lib, wrapper_path.c_str(), sizeof(shm->sandbox_lib));

	args.push_back(path.c_str());

	env.emplace_back("PATH", "/bin:/sbin:/usr/bin:/usr/sbin");
	env.emplace_back("USER", "root");
	env.emplace_back("LD_PRELOAD", "/lib/libc.so.7 " + wrapper_path + " /usr/lib/libcam.so.2");
	env.emplace_back("HOME", "/root");

	std::vector<char *> environ = env.GetEnviron();

	mockExec.ExpectExec(path, args, env, 0);

	call_wrapped_execve(path.c_str(), &args[0], &environ[0]);
}

TEST_F(ExecTestSuite, ExecWrapperPreloadedEnd)
{
	GlobalMockExec mockExec;
	std::string path("/bin/false");
	std::vector<const char *> args;
	ExecEnvironment env;
	std::string wrapper_path("/tmp/libfactory_wrapper.so.1");
	strlcpy(shm->sandbox_lib, wrapper_path.c_str(), sizeof(shm->sandbox_lib));

	args.push_back(path.c_str());

	env.emplace_back("PATH", "/bin:/sbin:/usr/bin:/usr/sbin");
	env.emplace_back("USER", "root");

	// The double colon here is intentional.  We're testing that we pass
	// allong to execve() faithfully.
	env.emplace_back("LD_PRELOAD", "/usr/lib/libzma.so.7::/usr/lib/libpmc.so.3:/usr/lib/libhdb.so.11:" + wrapper_path);
	env.emplace_back("HOME", "/root");

	std::vector<char *> environ = env.GetEnviron();

	mockExec.ExpectExec(path, args, env, 0);

	call_wrapped_execve(path.c_str(), &args[0], &environ[0]);
}

TEST_F(ExecTestSuite, ExecWrapperPreloadLastCharMismatch)
{
	GlobalMockExec mockExec;
	std::string path("/bin/false");
	std::vector<const char *> args;
	ExecEnvironment env;
	std::string wrapper_path("/tmp/libfactory_wrapper.so.1");
	strlcpy(shm->sandbox_lib, wrapper_path.c_str(), sizeof(shm->sandbox_lib));

	args.push_back(path.c_str());

	env.emplace_back("PATH", "/bin:/sbin:/usr/bin:/usr/sbin");
	env.emplace_back("USER", "root");

	size_t preload_index = env.size();


	// Pass a path that is the same length as the wrapper path into LD_PRELOAD
	// and ensure that execve() probably detects that the wrapper hasn't
	// already been passed
	std::string almost_wrapper(wrapper_path);
	almost_wrapper.back() = '9';

	env.emplace_back("LD_PRELOAD", "/lib/libthr.so.3:" + almost_wrapper + ":/usr/lib/libdtrace.so.1");

	env.emplace_back("HOME", "/root");

	std::vector<char *> environ = env.GetEnviron();
	env.at(preload_index) = EnvVar("LD_PRELOAD", "/lib/libthr.so.3:" + almost_wrapper + ":/usr/lib/libdtrace.so.1:" + wrapper_path);

	mockExec.ExpectExec(path, args, env, 0);

	call_wrapped_execve(path.c_str(), &args[0], &environ[0]);
}

TEST_F(ExecTestSuite, ExecWrapperPreloadLibTooShort)
{
	GlobalMockExec mockExec;
	std::string path("/bin/false");
	std::vector<const char *> args;
	ExecEnvironment env;
	std::string wrapper_path("/tmp/libfactory_wrapper.so.22");
	strlcpy(shm->sandbox_lib, wrapper_path.c_str(), sizeof(shm->sandbox_lib));

	args.push_back(path.c_str());

	env.emplace_back("PATH", "/bin:/sbin:/usr/bin:/usr/sbin");
	env.emplace_back("USER", "root");

	size_t preload_index = env.size();


	// Pass a path that is the same length as the wrapper path into LD_PRELOAD
	// and ensure that execve() probably detects that the wrapper hasn't
	// already been passed
	std::string almost_wrapper(wrapper_path);
	almost_wrapper.pop_back();

	env.emplace_back("LD_PRELOAD", "/lib/libthr.so.3:" + almost_wrapper);

	env.emplace_back("HOME", "/root");

	std::vector<char *> environ = env.GetEnviron();
	env.at(preload_index) = EnvVar("LD_PRELOAD", "/lib/libthr.so.3:" + almost_wrapper + ":" + wrapper_path);

	mockExec.ExpectExec(path, args, env, 0);

	call_wrapped_execve(path.c_str(), &args[0], &environ[0]);
}

TEST_F(ExecTestSuite, ExecWrapperPreloadLibTooLong)
{
	GlobalMockExec mockExec;
	std::string path("/bin/false");
	std::vector<const char *> args;
	ExecEnvironment env;
	std::string wrapper_path("/tmp/libfactory_wrapper.so.1");
	strlcpy(shm->sandbox_lib, wrapper_path.c_str(), sizeof(shm->sandbox_lib));

	args.push_back(path.c_str());

	env.emplace_back("PATH", "/bin:/sbin:/usr/bin:/usr/sbin");
	env.emplace_back("USER", "root");

	size_t preload_index = env.size();


	// Pass a path that is the same length as the wrapper path into LD_PRELOAD
	// and ensure that execve() probably detects that the wrapper hasn't
	// already been passed
	std::string almost_wrapper(wrapper_path);
	almost_wrapper.push_back('5');

	env.emplace_back("LD_PRELOAD", "/lib/libthr.so.3:" + almost_wrapper);

	env.emplace_back("HOME", "/root");

	std::vector<char *> environ = env.GetEnviron();
	env.at(preload_index) = EnvVar("LD_PRELOAD", "/lib/libthr.so.3:" + almost_wrapper + ":" + wrapper_path);

	mockExec.ExpectExec(path, args, env, 0);

	call_wrapped_execve(path.c_str(), &args[0], &environ[0]);
}
