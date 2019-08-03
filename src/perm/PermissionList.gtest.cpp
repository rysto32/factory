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

#include "PermissionList.h"

#include <errno.h>
#include <fcntl.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>


class PermissionListTestSuite : public ::testing::Test
{
};

TEST_F(PermissionListTestSuite, TestDirPerm)
{
	PermissionList list;

	list.AddPermission("/home", Permission::READ);

	EXPECT_EQ(list.IsPermitted("/home/rstone", O_RDONLY), 0);
	EXPECT_EQ(list.IsPermitted("/home/rstone", O_WRONLY), EPERM);
	EXPECT_EQ(list.IsPermitted("/home/rstone", O_RDWR), EPERM);
	EXPECT_EQ(list.IsPermitted("/home/rstone", O_RDONLY | O_EXEC), EPERM);

	EXPECT_EQ(list.IsPermitted("/hom", O_RDONLY), EPERM);
	EXPECT_EQ(list.IsPermitted("/homer", O_RDONLY), EPERM);
	EXPECT_EQ(list.IsPermitted("/home/", O_RDONLY), 0);
	EXPECT_EQ(list.IsPermitted("/tmp/test", O_RDONLY), EPERM);
	EXPECT_EQ(list.IsPermitted("/etc", O_RDONLY), EPERM);
	EXPECT_EQ(list.IsPermitted("/usr/home", O_RDONLY), EPERM);
}

TEST_F(PermissionListTestSuite, TestMultiDirPerm)
{
	PermissionList list;

	list.AddPermission("/home", Permission::READ);
	list.AddPermission("/tmp", Permission::READ);

	EXPECT_EQ(list.IsPermitted("/home/rstone", O_RDONLY), 0);
	EXPECT_EQ(list.IsPermitted("/home/rstone", O_WRONLY), EPERM);
	EXPECT_EQ(list.IsPermitted("/home/rstone", O_RDWR), EPERM);
	EXPECT_EQ(list.IsPermitted("/home/rstone", O_RDONLY | O_EXEC), EPERM);

	EXPECT_EQ(list.IsPermitted("/tmp/test", O_RDONLY), 0);

	EXPECT_EQ(list.IsPermitted("/hom", O_RDONLY), EPERM);
	EXPECT_EQ(list.IsPermitted("/homer", O_RDONLY), EPERM);
	EXPECT_EQ(list.IsPermitted("/home/", O_RDONLY), 0);
	EXPECT_EQ(list.IsPermitted("/etc", O_RDONLY), EPERM);
	EXPECT_EQ(list.IsPermitted("/usr/home", O_RDONLY), EPERM);
}

TEST_F(PermissionListTestSuite, TestSubDirPerm)
{
	PermissionList list;

	list.AddPermission("/home", Permission::READ);

	EXPECT_EQ(list.IsPermitted("/home/rstone/tmp", O_RDONLY), 0);
	EXPECT_EQ(list.IsPermitted("/home/rstone/tmp", O_RDONLY | O_EXEC), EPERM);
	EXPECT_EQ(list.IsPermitted("/home/rstone/git/factory/src", O_RDONLY), 0);
}

TEST_F(PermissionListTestSuite, TestTrailingSlash)
{
	PermissionList list;

	list.AddPermission("/home/", Permission::READ);
	list.AddPermission("/tmp//////", Permission::WRITE);

	EXPECT_EQ(list.IsPermitted("/home", O_RDONLY), 0);
	EXPECT_EQ(list.IsPermitted("/home/", O_RDONLY), 0);
	EXPECT_EQ(list.IsPermitted("/home/rstone/tmp", O_RDONLY), 0);
	EXPECT_EQ(list.IsPermitted("/home/rstone/tmp", O_RDONLY | O_EXEC), EPERM);

	EXPECT_EQ(list.IsPermitted("/tmp/test", O_WRONLY), 0);
	EXPECT_EQ(list.IsPermitted("/tmp/", O_WRONLY), 0);
	EXPECT_EQ(list.IsPermitted("/tmp/test/foo/bar/baz", O_WRONLY), 0);
	EXPECT_EQ(list.IsPermitted("/tmptest", O_WRONLY), EPERM);
	EXPECT_EQ(list.IsPermitted("/tmp/test", O_RDONLY), EPERM);
}

TEST_F(PermissionListTestSuite, TestRootDirPerm)
{
	PermissionList list;

	list.AddPermission("/", Permission::READ);

	EXPECT_EQ(list.IsPermitted("/etc", O_RDONLY), 0);
	EXPECT_EQ(list.IsPermitted("/tmp", O_RDONLY), 0);
	EXPECT_EQ(list.IsPermitted("/home/rstone/tmp", O_RDONLY), 0);
	EXPECT_EQ(list.IsPermitted("/usr/bin/cc", O_RDONLY | O_EXEC), EPERM);
}

