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

/* Garbage to make things compile */
typedef int mcontext_t;

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/resource.h>
#include <sys/syslimits.h>
#include <sys/sysproto.h>
#include <sys/ebpf.h>
#include <sys/ebpf_probe_syscalls.h>

#define	EXEC_INTERP_NONE	1
#define EXEC_INTERP_STANDARD	2

struct stat {
	dev_t     st_dev;		/* inode's device */
	ino_t	  st_ino;		/* inode's number */
	nlink_t	  st_nlink;		/* number of hard links */
	mode_t	  st_mode;		/* inode protection mode */
	__int16_t st_padding0;
	uid_t	  st_uid;		/* user ID of the file's owner */
	gid_t	  st_gid;		/* group ID of the file's group */
	__int32_t st_padding1;
	dev_t     st_rdev;		/* device type */
#ifdef	__STAT_TIME_T_EXT
	__int32_t st_atim_ext;
#endif
	struct	timespec st_atim;	/* time of last access */
#ifdef	__STAT_TIME_T_EXT
	__int32_t st_mtim_ext;
#endif
	struct	timespec st_mtim;	/* time of last data modification */
#ifdef	__STAT_TIME_T_EXT
	__int32_t st_ctim_ext;
#endif
	struct	timespec st_ctim;	/* time of last file status change */
#ifdef	__STAT_TIME_T_EXT
	__int32_t st_btim_ext;
#endif
	struct	timespec st_birthtim;	/* time of file creation */
	off_t	  st_size;		/* file size, in bytes */
	blkcnt_t st_blocks;		/* blocks allocated for file */
	blksize_t st_blksize;		/* optimal blocksize for I/O */
	fflags_t  st_flags;		/* user defined flags for file */
	__uint64_t st_gen;		/* file generation number */
	__uint64_t st_spare[10];
};

#define	EBPF_ACTION_CONTINUE	0
#define EBPF_ACTION_RETURN	1

/* open-only flags */
#define O_RDONLY        0x0000          /* open for reading only */
#define O_WRONLY        0x0001          /* open for writing only */
#define O_RDWR          0x0002          /* open for reading and writing */
#define O_ACCMODE       0x0003          /* mask for above modes */

#define O_EXEC          0x00040000      /* Open for execute only */

#define O_CLOEXEC       0x00100000

EBPF_DEFINE_MAP(file_lookup_map,  "hashtable", MAXPATHLEN, sizeof(int), 256, 0);
EBPF_DEFINE_MAP(fd_filename_map,  "array", sizeof(int), NAME_MAX, 256, 0);
EBPF_DEFINE_MAP(fd_map,  "array", sizeof(int), sizeof(int), 256, 0);
EBPF_DEFINE_MAP(scratch, "percpu_array", sizeof(int), MAXPATHLEN, 8, 0);
EBPF_DEFINE_MAP(pid_map, "hashtable", sizeof(pid_t), sizeof(int), 10, 0);
EBPF_DEFINE_MAP(defer_map, "progarray", sizeof(int), sizeof(int), 4, 0);

#define unlikely(x) (__builtin_expect(!!(x), 0))
#define __force_inline __attribute((always_inline))

static inline int do_open(const char * path, int flags, int mode, int*) __force_inline;
static inline int * lookup_fd(void *pathBuf, char **path) __force_inline;
static inline int * lookup_fd_user(const char * userPath, char **path) __force_inline;

static inline int * lookup_fd_user(const char * userPath, char **path)
{
	int index = 0;
	void *pathBuf = ebpf_map_lookup_elem(&scratch, &index);
	if (!pathBuf) {
		set_errno(ENOMEM);
		return 0;
	}

	memset(pathBuf, 0, MAXPATHLEN);

	size_t len;
	int error = copyinstr(userPath, pathBuf, MAXPATHLEN, &len);
	if (error != 0) {
		return 0;
	}

	return lookup_fd(pathBuf, path);
}

static inline int * lookup_fd(void *pathBuf, char **path)
{
	void *result = ebpf_map_lookup_path(&file_lookup_map, &pathBuf);
	if (!result) {
		set_errno(EPERM);
		return (0);
	}

	char *lookupName = reinterpret_cast<char*>(pathBuf);
	int index = *reinterpret_cast<int*>(result);

	result = ebpf_map_lookup_elem(&fd_filename_map, &index);

	char *filename = reinterpret_cast<char*>(result);
	if (*filename != '\0' && *lookupName != '\0') {
		set_errno(EISDIR);
		return 0;
	}

	result = ebpf_map_lookup_elem(&fd_map, &index);
	if (!result) {
		set_errno(EDOOFUS);
	}

	if (*lookupName != '\0') {
		*path = lookupName;
	} else {
		*path = filename;
	}
	return reinterpret_cast<int*>(result);
}

template <typename F>
static inline int
fd_op(const char *userPath, const F & func) __force_inline;

template <typename F>
static inline int
fd_op(const char *userPath, const F & func)
{
	int ret;
	char *path;

	int *dir_fd = lookup_fd_user(userPath, &path);
	if (dir_fd) {
		ret = func(*dir_fd, path);
	} else {
		ret = -1;
	}

	return ret;

}

static inline int do_open(const char * userPath, int flags, int mode, int *fd_out)
{
	int error;

	error = fd_op(userPath,
		[flags, mode, fd_out](int dir_fd, const char *path)
		{
			int fd = openat(dir_fd, path, flags, mode);
			if (fd < 0) {
				return -1;
			}

			if (fd_out)
				*fd_out = fd;
			return (0);
		});

	return EBPF_ACTION_RETURN;
}

static inline int do_readlink(const char *path, char * buf, size_t len) __force_inline;
static inline int do_readlink(const char *userPath, char * buf, size_t len)
{

	fd_op(userPath,
		[buf, len](int dir_fd, const char *path)
		{
			int index = 1;
			void * result = ebpf_map_lookup_elem(&scratch, &index);
			char * scratchBuf = reinterpret_cast<char*>(result);
			memset(scratchBuf, 0, MAXPATHLEN);

			size_t arglen = len < MAXPATHLEN ? len : MAXPATHLEN;

			int error = readlinkat(dir_fd, path, scratchBuf, arglen);
			if (error == 0) {
				copyout(scratchBuf, buf, arglen);
			}
			return 0;
		});

	return EBPF_ACTION_RETURN;
}

static inline int do_fork(void) __force_inline;

static inline int do_fork(void)
{
	int fd;
	pid_t pid;

	pid = pdfork(&fd, 0);
	if (pid > 0) {
		ebpf_map_update_elem(&pid_map, &pid, &fd, 0);
		set_syscall_retval(pid, 0);
	}
	return EBPF_ACTION_RETURN;
}

extern "C" {
int open_syscall_probe(struct open_args *args)
{
	return do_open(args->path, args->flags, args->mode, NULL);
}

int openat_syscall_probe(struct openat_args *args)
{
	if (args->fd >= 0) {
		return EBPF_ACTION_CONTINUE;
	}

	return do_open(args->path, args->flag, args->mode, NULL);
}

int fstatat_syscall_probe(struct fstatat_args *args)
{
	if (args->fd >= 0) {
		return EBPF_ACTION_CONTINUE;
	}

	fd_op(args->path,
		[args](int dir_fd, const char *path)
		{
			int index = 1;
			void *statBuf = ebpf_map_lookup_elem(&scratch, &index);
			if (!statBuf) {
				set_errno(ENOMEM);
				return (-1);
			}

			int error;
			struct stat *sb = reinterpret_cast<struct stat*>(statBuf);
			if (path[0] == '\0') {
				error = fstat(dir_fd, sb);
			} else {
				error = fstatat(dir_fd, path, sb, args->flag);
			}

			if (error) {
				return -1;
			}

			copyout(statBuf, args->buf, sizeof(*args->buf));
			return (0);

		});

	return EBPF_ACTION_RETURN;
}

int access_syscall_probe(struct access_args *args)
{

	fd_op(args->path,
		[args](int dir_fd, const char *path)
		{
			return faccessat(dir_fd, path, args->amode, 0);
		});

	return EBPF_ACTION_RETURN;
}

int vfork_syscall_probe(struct vfork_args *args)
{
	return do_fork();
}

int fork_syscall_probe(struct fork_args *args)
{
	return do_fork();
}

int wait4_syscall_probe(struct wait4_args *args)
{
	if (args->pid < 0)
		return EBPF_ACTION_CONTINUE;

	pid_t pid = args->pid;
	void *buf = ebpf_map_lookup_elem(&pid_map, &pid);
	if (!buf) {
		set_errno(ENOENT);
		return EBPF_ACTION_RETURN;
	}
	int *fd = reinterpret_cast<int*>(buf);

	int next_index = 0;
	void * next = ebpf_map_lookup_elem(&defer_map, &next_index);

	pdwait4_defer(*fd, args->options, args, next);

	/* If we got here we failed to jump to the next program */
	return EBPF_ACTION_RETURN;
}

int defer_wait4(struct wait4_args *args, int error, int status, struct rusage *ru)
{
	if (unlikely(error != 0)) {
		set_errno(error);
		return EBPF_ACTION_RETURN;
	}

	if (args->status) {
		error = copyout(&status, args->status, sizeof(status));
		if (unlikely(error != 0))
			return EBPF_ACTION_RETURN;
	}
	if (args->rusage) {
		error = copyout(ru, args->rusage, sizeof(*ru));
	}
	return EBPF_ACTION_RETURN;
}

int execve_syscall_probe(struct execve_args *uap)
{
	char *interp;
	int type, error, index;
	int fd;
	void *buf;

	do_open(uap->fname, O_RDONLY | O_EXEC | O_CLOEXEC, 0, &fd);
	if (unlikely(fd < 0)) {
		goto done;
	}

	index = 1;
	buf = ebpf_map_lookup_elem(&scratch, &index);
	if (!buf) {
		set_errno(ENOMEM);
		return EBPF_ACTION_RETURN;
	}
	memset(buf, 0, MAXPATHLEN);

	interp = reinterpret_cast<char*>(buf);

	error = exec_get_interp(fd, interp, MAXPATHLEN, &type);
	if (error != 0) {
		return EBPF_ACTION_RETURN;
	}

	if (type == EXEC_INTERP_NONE) {
		fexecve(fd, uap->argv, uap->envv, 0);
	} else if (type == EXEC_INTERP_STANDARD) {
		int *dir_fd = lookup_fd(interp, &interp);
		if (!dir_fd) {
			return EBPF_ACTION_CONTINUE;
		}

		int interp_fd = openat(*dir_fd, interp, O_RDONLY | O_EXEC, 0);
		if (interp_fd < 0) {
			return EBPF_ACTION_RETURN;
		}

		char rtld[] = "rtld";
		char dashdash[] = "--";

		char * argv_prepend[] = {
			rtld,
			dashdash,
			NULL
		};

		fexecve(interp_fd, uap->argv, uap->envv, argv_prepend);
	}

done:
	return EBPF_ACTION_RETURN;
}

int readlink_syscall_probe(struct readlink_args *args)
{
	return do_readlink(args->path, args->buf, args->count);
}

int readlinkat_syscall_probe(struct readlinkat_args *args)
{
	if (args->fd >= 0)
		return EBPF_ACTION_CONTINUE;

	return do_readlink(args->path, args->buf, args->bufsize);
}
}
