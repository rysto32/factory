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

#define	AT_SYMLINK_NOFOLLOW     0x0200  /* Do not follow symbolic links */

/*
 * Magic value that specify the use of the current working directory
 * to determine the target of relative file paths in the openat() and
 * similar syscalls.
 */
#define	AT_FDCWD		-100

#define MAX_PIDS	10
#define MAX_PREOPEN_FDS	256

EBPF_DEFINE_MAP(file_lookup_map,  "hashtable", MAXPATHLEN, sizeof(int), MAX_PREOPEN_FDS, 0);
EBPF_DEFINE_MAP(fd_filename_map,  "array", sizeof(int), NAME_MAX, MAX_PREOPEN_FDS, 0);
EBPF_DEFINE_MAP(fd_map,  "array", sizeof(int), sizeof(int), MAX_PREOPEN_FDS, 0);
EBPF_DEFINE_MAP(scratch, "percpu_array", sizeof(int), MAXPATHLEN, 8, 0);
EBPF_DEFINE_MAP(pid_map, "hashtable", sizeof(pid_t), sizeof(int), MAX_PIDS, 0);
EBPF_DEFINE_MAP(defer_map, "progarray", sizeof(int), sizeof(int), 4, 0);
EBPF_DEFINE_MAP(cwd_map, "hashtable", sizeof(pid_t), sizeof(int), MAX_PIDS, 0);

#define unlikely(x) (__builtin_expect(!!(x), 0))
#define __force_inline __attribute((always_inline))

class ScratchMgr
{
	int next;

public:
	ScratchMgr()
	  : next(0)
	{
	}

	ScratchMgr(const ScratchMgr &) = delete;
	ScratchMgr(ScratchMgr &&) = delete;

	ScratchMgr & operator=(const ScratchMgr&) = delete;
	ScratchMgr & operator=(ScratchMgr &&) = delete;

	template <typename T>
	T * GetScratch() __force_inline;
};

template <typename T>
T*
ScratchMgr::GetScratch()
{
	void *buf = ebpf_map_lookup_elem(&scratch, &next);
	if (!buf) {
		set_errno(ENOMEM);
		return nullptr;
	}

	memset(buf, 0, MAXPATHLEN);

	next++;
	return reinterpret_cast<T*>(buf);
}

#define LOOKUP_SYMLINK 0x01

static inline int do_open(ScratchMgr &alloc, const char * path, int flags, int mode, int*) __force_inline;
template <int ITERS=3>
static inline int * lookup_fd(ScratchMgr &alloc, void *pathBuf, char **path, int flags = 0) __force_inline;
template <int ITERS=3>
static inline int * do_lookup_fd(void *pathBuf, void *scratchBuf, char **path, int flags) __force_inline;
template <int ITERS=3>
static inline int * do_symlink_lookup(int *fd, void *origPath, void *scratchBuf, char **path, int flags) __force_inline;
static inline void * do_single_lookup(void *pathBuf, void *scratchBuf, char **path, int flags) __force_inline;
static inline int * lookup_fd_user(ScratchMgr &alloc, const char * userPath, char **path, int flags = 0) __force_inline;
static inline int do_mkdir(const char *path, mode_t mode) __force_inline;
static inline int do_fchdir(int fd) __force_inline;

static int resolve_symlink(void *pathBuf, void *scratchBuf, int fd, char *fileName) __force_inline;

static inline int * lookup_fd_user(ScratchMgr &alloc, const char * userPath, char **path, int flags)
{
	void *result;
	char *tmp;
	int *fd;

	char * inBuf = alloc.GetScratch<char>();
	if (!inBuf) {
		return nullptr;
	}

	size_t len;
	int error = copyinstr(userPath, inBuf, MAXPATHLEN, &len);
	if (error != 0) {
		return nullptr;
	}

	char *pathBuf = alloc.GetScratch<char>();
	if (!pathBuf) {
		return nullptr;
	}

	if (inBuf[0] != '/') {
		pid_t pid = getpid();
		result = ebpf_map_lookup_elem(&cwd_map, &pid);
		if (!result) {
			set_errno(ENXIO);
			return nullptr;
		}

		tmp = inBuf;
		inBuf = pathBuf;
		pathBuf = tmp;

		*path = pathBuf;
	} else {
		error = canonical_path(pathBuf, inBuf, MAXPATHLEN);
		if (error != 0) {
			set_errno(error);
			return nullptr;
		}

		result = do_single_lookup(pathBuf, inBuf, path, flags);

		if (!result) {
			return nullptr;
		}
	}

	fd = reinterpret_cast<int*>(result);
	return do_symlink_lookup(fd, pathBuf, inBuf, path, flags);
}

template <int ITERS>
static inline int * lookup_fd(ScratchMgr &alloc, void *pathBuf, char **path, int flags)
{
	return do_lookup_fd<ITERS>(pathBuf, alloc.GetScratch<void>(), path, flags);
}

static inline void * do_single_lookup(void *pathBuf, void *scratchBuf, char **path, int flags)
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
		/*
		 * User looked up /a/b/c, but we have an entry for a
		 * file (not dir) called /a/b.
		 */
		set_errno(EISDIR);
		return 0;
	}

	result = ebpf_map_lookup_elem(&fd_map, &index);
	if (!result) {
		set_errno(EDOOFUS);
		return nullptr;
	}

	if (*lookupName != '\0') {
		*path = lookupName;
	} else {
		*path = filename;
	}

	return result;
}

template <int ITERS>
static inline int * do_lookup_fd(void *pathBuf, void *scratchBuf, char **path, int flags)
{
	void * result = do_single_lookup(pathBuf, scratchBuf, path, flags);
	int *fd = reinterpret_cast<int*>(result);

	return do_symlink_lookup<ITERS>(fd, pathBuf, scratchBuf, path, flags);
}

template <int ITERS>
static inline int * do_symlink_lookup(int * fd, void *origPath, void *scratchBuf, char **path, int flags)
{

	if (flags & LOOKUP_SYMLINK) {
		return fd;
	}

	if constexpr (ITERS <= 1) {
		return fd;
	} else {
		/*
		 * If the result of the lookup is a symlink, we need to resolve
		 * it in case the symlink points outside of the directory tree
		 * covered by fd.
		 */
		int error = resolve_symlink(origPath, scratchBuf, *fd, *path);
		if (error == ENODEV) {
			// Not a symlink.
			set_errno(0);
			set_syscall_retval(0, 0);
			return fd;
		} else if (error != 0) {
			return nullptr;
		}

		/* Redo the lookup using the symlink target. */
		return do_lookup_fd<ITERS - 1>(origPath, scratchBuf, path, flags);
	}
}

static int resolve_symlink(void *pathBuf, void *scratchBuf, int fd, char *fileName)
{

	if (*fileName == '\0') {
		// We opened the directory directly; we can't handle
		// symlinks here.
		return ENODEV;
	}

	char * target = reinterpret_cast<char*>(scratchBuf);
	memset(target, 0, MAXPATHLEN);

	int error = readlinkat(fd, fileName, target, MAXPATHLEN);
	if (error != 0) {
		// Probably not a symlink.  Return what we already have.
		return ENODEV;
	}

	char * basePath = reinterpret_cast<char*>(pathBuf);
	return canonical_path(basePath, target, MAXPATHLEN);

}

template <typename F>
static inline int
fd_op(const char *userPath, int flags, const F & func) __force_inline;

template <typename F>
static inline int
fd_op(ScratchMgr &alloc, const char *userPath, int flags, const F & func)
{
	int ret;
	char *path;

	int *dir_fd = lookup_fd_user(alloc, userPath, &path, flags);
	if (dir_fd) {
		ret = func(*dir_fd, path);
	} else {
		ret = -1;
	}

	return ret;

}

static inline int do_open(ScratchMgr &alloc, const char * userPath, int flags, int mode, int *fd_out)
{
	int error;

	error = fd_op(alloc, userPath, 0,
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

	if (error != 0 && fd_out) {
		*fd_out = -1;
	}

	return EBPF_ACTION_RETURN;
}

static inline int do_readlink(ScratchMgr &alloc, const char *path, char * buf, size_t len) __force_inline;
static inline int do_readlink(ScratchMgr &alloc, const char *userPath, char * buf, size_t len)
{

	fd_op(alloc, userPath, 0,
		[&alloc, buf, len](int dir_fd, const char *path)
		{
			char * scratchBuf = alloc.GetScratch<char>();

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
	pid_t pid, ppid;

	pid = pdfork(&fd, 0);
	if (pid > 0) {
		ppid = getpid();
		ebpf_map_update_elem(&pid_map, &pid, &fd, 0);
		void *cwd = ebpf_map_lookup_elem(&cwd_map, &ppid);
		if (cwd) {
			ebpf_map_update_elem(&cwd_map, &pid, cwd, 0);
		}
		set_syscall_retval(pid, 0);
	}
	return EBPF_ACTION_RETURN;
}

extern "C" {
int open_syscall_probe(struct open_args *args)
{
	ScratchMgr alloc;
	return do_open(alloc, args->path, args->flags, args->mode, NULL);
}

int openat_syscall_probe(struct openat_args *args)
{
	if (args->fd != AT_FDCWD) {
		return EBPF_ACTION_CONTINUE;
	}

	ScratchMgr alloc;
	return do_open(alloc, args->path, args->flag, args->mode, NULL);
}

int fstatat_syscall_probe(struct fstatat_args *args)
{
	if (args->fd != AT_FDCWD) {
		return EBPF_ACTION_CONTINUE;
	}

	int flags = args->flag & AT_SYMLINK_NOFOLLOW ? LOOKUP_SYMLINK : 0;

	ScratchMgr alloc;
	fd_op(alloc, args->path, flags,
		[&alloc, args](int dir_fd, const char *path)
		{
			struct stat *sb = alloc.GetScratch<struct stat>();
			if (!sb) {
				set_errno(ENOMEM);
				return (-1);
			}

			int error;
			if (path[0] == '\0') {
				error = fstat(dir_fd, sb);
			} else {
				error = fstatat(dir_fd, path, sb, args->flag);
			}

			if (error) {
				return -1;
			}

			copyout(sb, args->buf, sizeof(*args->buf));
			return (0);

		});

	return EBPF_ACTION_RETURN;
}

int access_syscall_probe(struct access_args *args)
{

	ScratchMgr alloc;
	fd_op(alloc, args->path, 0,
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

	ScratchMgr alloc;
	do_open(alloc, uap->fname, O_RDONLY | O_EXEC | O_CLOEXEC, 0, &fd);
	if (unlikely(fd < 0)) {
		goto done;
	}

	index = 1;
	interp = alloc.GetScratch<char>();
	if (!interp) {
		return EBPF_ACTION_RETURN;
	}
	memset(interp, 0, MAXPATHLEN);

	error = exec_get_interp(fd, interp, MAXPATHLEN, &type);
	if (error != 0) {
		return EBPF_ACTION_RETURN;
	}

	if (type == EXEC_INTERP_NONE) {
		fexecve(fd, uap->argv, uap->envv, 0);
	} else if (type == EXEC_INTERP_STANDARD) {
		/*
		 * Have to reduce to reduce to 2 lookup iterations to keep
		 * the EBPF program size below the limit.
		 */
		int *dir_fd = lookup_fd<2>(alloc, interp, &interp);
		if (!dir_fd) {
			return EBPF_ACTION_RETURN;
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
	ScratchMgr alloc;
	return do_readlink(alloc, args->path, args->buf, args->count);
}

int readlinkat_syscall_probe(struct readlinkat_args *args)
{
	if (args->fd != AT_FDCWD)
		return EBPF_ACTION_CONTINUE;

	ScratchMgr alloc;
	return do_readlink(alloc, args->path, args->buf, args->bufsize);
}

int rename_syscall_probe(struct rename_args *args)
{
	ScratchMgr alloc;
	int tofd, error;
	const char *to;

	error = fd_op(alloc, args->to, 0,
	    [&tofd,&to](int fd, const char *path)
		{
			tofd = fd;
			to = path;
			return (0);
		});
	if (error != 0) {
		return (EBPF_ACTION_RETURN);
	}

	fd_op(alloc, args->from, 0,
		[tofd,to](int fromfd, const char *from)
		{
			return (renameat(fromfd, from, tofd, to));
		});

	return (EBPF_ACTION_RETURN);
}

static inline int
do_mkdir(const char *path, mode_t mode)
{
	ScratchMgr alloc;

	fd_op(alloc, path, 0,
		[mode](int fd, const char *path)
		{
			return (mkdirat(fd, path, mode));
		});

	return (EBPF_ACTION_RETURN);
}

int
mkdir_syscall_probe(struct mkdir_args *args)
{

	return (do_mkdir(args->path, args->mode));
}

int
mkdirat_syscall_probe(struct mkdirat_args *args)
{
	if (args->fd != AT_FDCWD) {
		return EBPF_ACTION_CONTINUE;
	}

	return (do_mkdir(args->path, args->mode));
}

static inline int
do_fchdir(int fd)
{
	pid_t pid;
	int error;

	pid = getpid();
	error = fchdir(fd);
	if (error != 0) {
		return (error);
	}

	ebpf_map_update_elem(&cwd_map, &pid, &fd, 0);

	/*
	 * We no longer know (or need to know) the path of cwd, so just delete
	 * the element from the map.
	 */
	ebpf_map_delete_elem(&cwd_name_map, &pid);
	return (0);
}

int
chdir_syscall_probe(struct chdir_args *args)
{
	ScratchMgr alloc;
	int fd;

	do_open(alloc, args->path, O_RDONLY, 0, &fd);
	if (fd < 0) {
		return (EBPF_ACTION_RETURN);
	}


	do_fchdir(fd);
	return (EBPF_ACTION_RETURN);
}

int
fchdir_syscall_probe(struct fchdir_args *args)
{

	do_fchdir(args->fd);
	return (EBPF_ACTION_RETURN);
}

}
