
typedef int mcontext_t;

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/sysproto.h>
#include <sys/ebpf.h>
#include <sys/ebpf_probe_syscalls.h>

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

EBPF_DEFINE_MAP(fd_map,  "hashtable", MAXPATHLEN, sizeof(int), 256, 0);
EBPF_DEFINE_MAP(scratch, "percpu_array", sizeof(int), MAXPATHLEN, 8, 0);

static inline int do_open(const char * path, int flags, int mode) __attribute((always_inline));

static inline int do_open(const char * userPath, int flags, int mode)
{
	int index = 0;
	void *pathBuf = ebpf_map_lookup_elem(&scratch, &index);
	if (!pathBuf) {
		set_errno(ENOMEM);
		return EBPF_ACTION_RETURN;
	}

	size_t len;
	int error = copyinstr(userPath, pathBuf, MAXPATHLEN, &len);
	if (error != 0) {
		return EBPF_ACTION_CONTINUE;
	}

	int *fd = ebpf_map_lookup_path(&fd_map, &pathBuf);
	if (fd) {
		char *path = pathBuf;
		if (path[0] == '\0') {
			dup(*fd);
			return EBPF_ACTION_RETURN;
		} else {
			openat(*fd, path, flags, mode);
			return EBPF_ACTION_RETURN;
		}
	}

	return EBPF_ACTION_CONTINUE;
}

int open_syscall_probe(struct open_args *args)
{
	return do_open(args->path, args->flags, args->mode);
}

int openat_syscall_probe(struct openat_args *args)
{
	if (args->fd >= 0) {
		return EBPF_ACTION_CONTINUE;
	}

	return do_open(args->path, args->flag, args->mode);
}


int fstatat_syscall_probe(struct fstatat_args *args)
{
	if (args->fd >= 0) {
		return EBPF_ACTION_CONTINUE;
	}

	int index = 0;
	void *pathBuf = ebpf_map_lookup_elem(&scratch, &index);
	if (pathBuf == NULL) {
		set_errno(ENOMEM);
		return EBPF_ACTION_RETURN;
	}

	size_t len;
	int error = copyinstr(args->path, pathBuf, MAXPATHLEN, &len);
	if (error != 0) {
		return EBPF_ACTION_CONTINUE;
	}

	int *fd = ebpf_map_lookup_path(&fd_map, &pathBuf);
	if (fd) {
		index = 1;
		void *statBuf = ebpf_map_lookup_elem(&scratch, &index);
		if (!statBuf) {
			set_errno(ENOMEM);
			return EBPF_ACTION_RETURN;
		}

		char *path = pathBuf;
		if (path[0] == '\0') {
			error = fstat(*fd, statBuf);
		} else {
			error = fstatat(*fd, path, statBuf, args->flag);
		}

		if (error) {
			return EBPF_ACTION_RETURN;
		}

		copyout(statBuf, args->buf, sizeof(*args->buf));
		return EBPF_ACTION_RETURN;
	}

	return EBPF_ACTION_CONTINUE;
}


