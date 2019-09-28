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

#define	KQ_ERR_EMPTY		0x0001	/* Error on kevent on empty kqueue */

#define EVFILT_READ		(-1)
#define EVFILT_WRITE		(-2)
#define EVFILT_AIO		(-3)	/* attached to aio requests */
#define EVFILT_VNODE		(-4)	/* attached to vnodes */
#define EVFILT_PROC		(-5)	/* attached to struct proc */
#define EVFILT_SIGNAL		(-6)	/* attached to struct proc */
#define EVFILT_TIMER		(-7)	/* timers */
#define EVFILT_PROCDESC		(-8)	/* attached to process descriptors */
#define EVFILT_FS		(-9)	/* filesystem events */
#define EVFILT_LIO		(-10)	/* attached to lio requests */
#define EVFILT_USER		(-11)	/* User events */
#define EVFILT_SENDFILE		(-12)	/* attached to sendfile requests */
#define EVFILT_EMPTY		(-13)	/* empty send socket buf */
#define EVFILT_SYSCOUNT		13

/* actions */
#define EV_ADD		0x0001		/* add event to kq (implies enable) */
#define EV_DELETE	0x0002		/* delete event from kq */
#define EV_ENABLE	0x0004		/* enable event */
#define EV_DISABLE	0x0008		/* disable event (not reported) */
#define EV_FORCEONESHOT	0x0100		/* enable _ONESHOT and force trigger */

/* flags */
#define EV_ONESHOT	0x0010		/* only report one occurrence */
#define EV_CLEAR	0x0020		/* clear event state after reporting */
#define EV_RECEIPT	0x0040		/* force EV_ERROR on success, data=0 */
#define EV_DISPATCH	0x0080		/* disable event after reporting */

#define EV_SYSFLAGS	0xF000		/* reserved by system */
#define	EV_DROP		0x1000		/* note should be dropped */
#define EV_FLAG1	0x2000		/* filter-specific flag */
#define EV_FLAG2	0x4000		/* filter-specific flag */

/* returned values */
#define EV_EOF		0x8000		/* EOF detected */
#define EV_ERROR	0x4000		/* error, data contains errno */

/*
 * data/hint flags for EVFILT_PROC and EVFILT_PROCDESC, shared with userspace
 */
#define	NOTE_EXIT	0x80000000		/* process exited */
#define	NOTE_FORK	0x40000000		/* process forked */
#define	NOTE_EXEC	0x20000000		/* process exec'd */
#define	NOTE_PCTRLMASK	0xf0000000		/* mask for hint bits */
#define	NOTE_PDATAMASK	0x000fffff		/* mask for pid */

struct kevent {
	__uintptr_t	ident;		/* identifier for this event */
	short		filter;		/* filter for event */
	unsigned short	flags;		/* action flags for kqueue */
	unsigned int	fflags;		/* filter flag value */
	__int64_t	data;		/* filter data value */
	void		*udata;		/* opaque user data identifier */
	__uint64_t	ext[4];		/* extensions */
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

#define F_SETFD         2               /* set file descriptor flags */

#define FD_CLOEXEC      1               /* close-on-exec flag */

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
EBPF_DEFINE_MAP(cwd_map, "hashtable", sizeof(pid_t), sizeof(int), MAX_PIDS, 0);
EBPF_DEFINE_MAP(cwd_name_map, "hashtable", sizeof(pid_t), MAXPATHLEN, MAX_PIDS, 0);

EBPF_DEFINE_MAP(exit_kq_map, "hashtable", sizeof(pid_t), sizeof(int), MAX_PIDS, 0);

EBPF_DEFINE_MAP(pdwait_prog, "progarray", sizeof(int), sizeof(int), 1, 0);
EBPF_DEFINE_MAP(kevent_prog, "progarray", sizeof(int), sizeof(int), 1, 0);

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

static inline int do_open(ScratchMgr &alloc, const char * path, int flags, int mode, int*) __force_inline;
template <int ITERS=3>
static inline int * lookup_fd(ScratchMgr &alloc, char *pathBuf, char **path) __force_inline;
template <int ITERS=3, bool FOUND>
static inline int * do_lookup_fd(struct ebpf_symlink_res_bufs *bufs, char **path, int flags) __force_inline;
template <int ITERS=3, bool FOUND = false>
static inline int * do_symlink_lookup(int *fd, struct ebpf_symlink_res_bufs *bufs, char **path, int flags) __force_inline;
static inline void * do_single_lookup(void *pathBuf, char **path) __force_inline;
static inline int * lookup_fd_user(const char * userPath, char *pathBuf, char *inBuf, char **path) __force_inline;
static inline int do_mkdir(const char *path, mode_t mode) __force_inline;
static inline int do_fchdir(int fd) __force_inline;
static inline int get_exit_kq(pid_t pid) __force_inline;
static inline int do_symlink(const char *target, const char *source) __force_inline;
static inline int do_unlink(const char *path, int flag) __force_inline;

static inline int * lookup_fd_user(const char * userPath, char *pathBuf, char *inBuf, char **path)
{
	void *result;
	pid_t pid;
	int *fd;

	size_t len;
	int error = copyinstr(userPath, inBuf, MAXPATHLEN, &len);
	if (error != 0) {
		return nullptr;
	}

	ktrnamei(inBuf);

	if (inBuf[0] != '/') {
		pid = getpid();
		result = ebpf_map_lookup_elem(&cwd_name_map, &pid);
		if (result) {
			strlcpy(pathBuf, (char*)result, MAXPATHLEN);
			goto path_lookup;
		}

		result = ebpf_map_lookup_elem(&cwd_map, &pid);
		if (!result) {
			set_errno(EPERM);
			return nullptr;
		}

		if (inBuf[0] == '.' && inBuf[1] == '\0') {
			*path = pathBuf; /* Empty String */
			return (reinterpret_cast<int*>(result));
		}

		strlcpy(pathBuf, inBuf, MAXPATHLEN);

		*path = pathBuf;
	} else {
path_lookup:
		error = canonical_path(pathBuf, inBuf, MAXPATHLEN);
		if (error != 0) {
			set_errno(error);
			return nullptr;
		}

		result = do_single_lookup(pathBuf, path);

		if (!result) {
			return nullptr;
		}
	}

	fd = reinterpret_cast<int*>(result);
	return (fd);
}

template <int ITERS>
static inline int * lookup_fd(ScratchMgr &alloc, char *pathBuf, char **path)
{
	struct ebpf_symlink_res_bufs bufs;

	bufs.pathBuf = pathBuf;

	bufs.scratch1 = alloc.GetScratch<char>();
	if (!bufs.scratch1) {
		return (nullptr);
	}

	bufs.scratch2 = alloc.GetScratch<char>();
	if (!bufs.scratch2) {
		return (nullptr);
	}

	return do_lookup_fd<ITERS, false>(&bufs, path, 0);
}

static inline void * do_single_lookup(void *pathBuf, char **path)
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

template <int ITERS, bool FOUND>
static inline int * do_lookup_fd(struct ebpf_symlink_res_bufs *bufs, char **path, int flags)
{
	void * result = do_single_lookup(bufs->pathBuf, path);
	int *fd = reinterpret_cast<int*>(result);
	if (!fd) {
		return nullptr;
	}

	return do_symlink_lookup<ITERS, true>(fd, bufs, path, flags);
}

template <int ITERS, bool FOUND>
static inline int * do_symlink_lookup(int * fd, struct ebpf_symlink_res_bufs *bufs, char **path, int flags)
{

	if (**path == '\0') {
		return fd;
	}

	if constexpr (ITERS <= 1) {
		return fd;
	} else {

		int error = resolve_one_symlink(bufs, *fd, *path, flags);
		if (error == ENODEV || error == ENOENT) {
			// Not a symlink.
			if constexpr (FOUND) {
				set_errno(0);
				set_syscall_retval(0, 0);

				return fd;
			} else {
				return nullptr;
			}
		} else if (error != 0) {
			return nullptr;
		}

		/* Redo the lookup using the symlink target. */
		return do_lookup_fd<ITERS - 1, true>(bufs, path, flags);
	}
}

template <typename F>
static inline int
fd_op(const char *userPath, int flags, const F & func) __force_inline;

template <typename F>
static inline int
fd_op(ScratchMgr &alloc, const char *userPath, int flags, const F & func)
{
	int ret;
	struct ebpf_symlink_res_bufs bufs;
	char *path;

	bufs.pathBuf = alloc.GetScratch<char>();
	if (!bufs.pathBuf) {
		return (-1);
	}

	bufs.scratch1 = alloc.GetScratch<char>();
	if (!bufs.scratch1) {
		return (-1);
	}

	bufs.scratch2 = alloc.GetScratch<char>();
	if (!bufs.scratch2) {
		return (-1);
	}

	int *dir_fd = lookup_fd_user(userPath, bufs.pathBuf, bufs.scratch1, &path);
	if (dir_fd) {
		ret = func(*dir_fd, path);
		if (ret == ENOTCAPABLE) {
			/*
			 * This error might be due to a symlink that points
			 * outside of *dir_fd.  Try to manually resolve symlinks
			 * to allow the to operation to succeed.
			 */
			dir_fd = do_symlink_lookup(dir_fd, &bufs, &path, flags);

			if (dir_fd) {
				set_errno(0);
				ret = func(*dir_fd, path);
			}
		}
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
			int fd, error, allowed;

			if (path[0] == '\0') {
				/* We only get here with open(".") */
				allowed = O_RDONLY | O_CLOEXEC;
				if ((flags & ~allowed) != 0) {
					set_errno(EPERM);
					return (EPERM);
				}

				fd = dup(dir_fd);
				if (fd < 0) {
					return (get_errno());
				}

				if (flags & O_CLOEXEC) {
					error = fcntl(fd, F_SETFD, FD_CLOEXEC);
					if (error != 0) {
						close(fd);
						return (error);
					}
				}
			} else {
				fd = openat(dir_fd, path, flags, mode);

				if (fd < 0) {
					return (get_errno());
				}
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

	fd_op(alloc, userPath, AT_SYMLINK_NOFOLLOW,
		[&alloc, buf, len](int dir_fd, const char *path)
		{
			char * scratchBuf = alloc.GetScratch<char>();

			size_t arglen = len < MAXPATHLEN ? len : MAXPATHLEN;

			int error = readlinkat(dir_fd, path, scratchBuf, arglen);
			if (error == 0) {
				copyout(scratchBuf, buf, arglen);
			}
			return (error);
		});

	return EBPF_ACTION_RETURN;
}

static inline int do_fork(void) __force_inline;

static inline int
get_exit_kq(pid_t pid)
{
	int kq;
	void *result;

	result = ebpf_map_lookup_elem(&exit_kq_map, &pid);
	if (result) {
		return *reinterpret_cast<int*>(result);
	}

	kq = kqueue(KQ_ERR_EMPTY);
	ebpf_map_update_elem(&exit_kq_map, &pid, &kq, 0);
	return (kq);
}

static inline int do_fork(void)
{
	int fd, kq;
	pid_t pid, ppid;
	void *cwd;

	pid = pdfork(&fd, 0);
	if (pid > 0) {
		ppid = getpid();
		ebpf_map_update_elem(&pid_map, &pid, &fd, 0);

		cwd = ebpf_map_lookup_elem(&cwd_name_map, &ppid);
		if (cwd) {
			ebpf_map_update_elem(&cwd_name_map, &pid, cwd, 0);
		} else {
			cwd = ebpf_map_lookup_elem(&cwd_map, &ppid);
			if (cwd) {
				ebpf_map_update_elem(&cwd_map, &pid, cwd, 0);
			}
		}

		kq = get_exit_kq(ppid);
		struct kevent ev = {
			.ident = uintptr_t(fd),
			.filter = EVFILT_PROCDESC,
			.flags = EV_ADD,
			.fflags = NOTE_EXIT,
		};

		kevent_install(kq, &ev, 1);

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

	ScratchMgr alloc;
	fd_op(alloc, args->path, args->flag,
		[&alloc, args](int dir_fd, const char *path)
		{
			struct stat *sb = alloc.GetScratch<struct stat>();
			if (!sb) {
				set_errno(ENOMEM);
				return (ENOMEM);
			}

			int error;
			if (path[0] == '\0') {
				error = fstat(dir_fd, sb);
			} else {
				error = fstatat(dir_fd, path, sb, args->flag);
			}

			if (error) {
				return (error);
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
	void *next;
	void *buf;
	int fd, error, kq;
	pid_t pid;

	if (args->pid < 0) {
		const int SUPPORTED_FLAGS = WNOHANG | WEXITED | WNOWAIT;

		/* This option is implicit for wait4. */
		args->options |= WEXITED;

		if ((args->options & ~SUPPORTED_FLAGS) != 0) {
			set_errno(EPERM);
			return (EBPF_ACTION_RETURN);
		}

		kq = get_exit_kq(getpid());

		if (args->options & (WNOHANG | WNOWAIT)) {
			struct kevent ev;
			error = kevent_poll(kq, &ev, 1);
			if (error != 0) {
				return EBPF_ACTION_RETURN;
			}

			fd = ev.ident;
			goto dowait4;
		}

		int next_index = 0;
		next = ebpf_map_lookup_elem(&kevent_prog, &next_index);

		kevent_block(kq, NULL, next);

		/* If we got here we failed to jump to the next program */
		return EBPF_ACTION_RETURN;
	} else {
		pid = args->pid;
		buf = ebpf_map_lookup_elem(&pid_map, &pid);
		if (!buf) {
			set_errno(ENOENT);
			return EBPF_ACTION_RETURN;
		}
		fd = *reinterpret_cast<int*>(buf);

dowait4:
		int next_index = 0;
		next = ebpf_map_lookup_elem(&pdwait_prog, &next_index);

		pdwait4_defer(fd, args->options, args, next);

		/* If we got here we failed to jump to the next program */
		return EBPF_ACTION_RETURN;
	}
}

int defer_kevent(struct wait4_args *args, int error, struct kevent *ev)
{
	int prog_index;
	void *next;

	if (error != 0) {
		set_errno(error);
		return (EBPF_ACTION_RETURN);
	}

	prog_index = 0;
	next = ebpf_map_lookup_elem(&pdwait_prog, &prog_index);

	pdwait4_defer(ev->ident, args->options, args, next);

	/* If we got here we failed to jump to the next program */
	return EBPF_ACTION_RETURN;
}

int defer_wait4(struct wait4_args *args, int error, int status, struct rusage *ru, int fd)
{
	pid_t pid;

	if (unlikely(error != 0)) {
		set_errno(error);
		set_syscall_retval(-1, 0);
		return EBPF_ACTION_RETURN;
	}

	pid = get_syscall_retval();

	if (args->status) {
		error = copyout(&status, args->status, sizeof(status));
		if (unlikely(error != 0)) {
			set_syscall_retval(-1, 0);
			return EBPF_ACTION_RETURN;
		}
	}
	if (args->rusage) {
		error = copyout(ru, args->rusage, sizeof(*ru));
	}

	if (!(args->options & WNOWAIT)) {
		close(fd);
		ebpf_map_delete_elem(&pid_map, &pid);
	}

	if (error == 0) {
		set_syscall_retval(pid, 0);
	} else {
		set_syscall_retval(-1, 0);
		set_errno(error);
	}
	return EBPF_ACTION_RETURN;
}

int execve_syscall_probe(struct execve_args *uap)
{
	char *interp, *exec;
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

		exec = alloc.GetScratch<char>();
		error = copyinstr(uap->fname, exec, MAXPATHLEN, NULL);
		if (error != 0) {
			return (EBPF_ACTION_RETURN);
		}

		char rtld[] = "rtld";
		char dashdash[] = "--";

		char * argv_prepend[] = {
			rtld,
			dashdash,
			exec,
			NULL
		};

		fexecve(interp_fd, uap->argv + 1, uap->envv, argv_prepend);
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
	char *path, *canonical;
	int fd, error;
	pid_t pid;

	do_open(alloc, args->path, O_RDONLY, 0, &fd);
	if (fd < 0) {
		return (EBPF_ACTION_RETURN);
	}


	error = do_fchdir(fd);
	if (error == 0) {
		path = alloc.GetScratch<char>();
		if (!path) {
			set_errno(0);
			return (EBPF_ACTION_RETURN);
		}

		canonical = alloc.GetScratch<char>();
		if (!canonical) {
			set_errno(0);
			return (EBPF_ACTION_RETURN);
		}

		error = copyin(args->path, path, MAXPATHLEN);
		if (error != 0) {
			set_errno(0);
			return (EBPF_ACTION_RETURN);
		}

		error = canonical_path(canonical, path, MAXPATHLEN);

		pid = getpid();
		ebpf_map_update_elem(&cwd_name_map, &pid, canonical, 0);
	}
	return (EBPF_ACTION_RETURN);
}

int
fchdir_syscall_probe(struct fchdir_args *args)
{

	do_fchdir(args->fd);
	return (EBPF_ACTION_RETURN);
}

int
exit_syscall_probe(struct exit_args *args)
{
	pid_t pid;

	pid = getpid();

	/*
	 * XXX this must also happen if the process dies in another way, like
	 * due to a signal.
	 */
	ebpf_map_delete_elem(&cwd_map, &pid);
	ebpf_map_delete_elem(&cwd_name_map, &pid);
	ebpf_map_delete_elem(&exit_kq_map, &pid);
	return (EBPF_ACTION_CONTINUE);
}

static __inline int
do_symlink(const char *target_user, const char *source)
{
	ScratchMgr alloc;
	char *target;
	int error;

	target = alloc.GetScratch<char>();
	if (!target) {
		return (EBPF_ACTION_RETURN);
	}

	error = copyinstr(target_user, target, MAXPATHLEN, nullptr);
	if (error != 0) {
		return (EBPF_ACTION_RETURN);
	}

	fd_op(alloc, source, 0,
		[target](int fd, const char *path)
		{
			return (symlinkat(target, fd, path));
		});

	return (EBPF_ACTION_RETURN);
}

int
symlink_syscall_probe(struct symlink_args *args)
{

	return do_symlink(args->path, args->link);
}

int
symlinkat_syscall_probe(struct symlinkat_args *args)
{
	if (args->fd != AT_FDCWD) {
		return EBPF_ACTION_CONTINUE;
	}

	return do_symlink(args->path1, args->path2);
}

int
utimensat_syscall_probe(struct utimensat_args *args)
{
	ScratchMgr alloc;
	struct timespec *times;
	int error;

	if (args->fd != AT_FDCWD) {
		return EBPF_ACTION_CONTINUE;
	}

	times = alloc.GetScratch<struct timespec>();
	error = copyin(args->times, times, 2 * sizeof(times));
	if (error != 0) {
		return (EBPF_ACTION_RETURN);
	}

	fd_op(alloc, args->path, args->flag,
		[args,times](int fd, const char *path)
		{
			return (utimensat(fd, path, times, args->flag));
		});

	return (EBPF_ACTION_RETURN);
}

static inline int
do_unlink(const char *path, int flags)
{
	ScratchMgr alloc;

	fd_op(alloc, path, AT_SYMLINK_NOFOLLOW,
		[flags](int fd, const char *file)
		{
			return (unlinkat(fd, file, flags));
		});

	return (EBPF_ACTION_RETURN);
}

int
unlink_syscall_probe(struct unlink_args *args)
{

	return (do_unlink(args->path, 0));
}

int
unlinkat_syscall_probe(struct unlinkat_args *args)
{

	if (args->fd != AT_FDCWD) {
		return (EBPF_ACTION_CONTINUE);
	}

	return (do_unlink(args->path, args->flag));
}

}
