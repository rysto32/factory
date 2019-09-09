
#include <sys/param.h>
#include <sys/ebpf.h>
#include <sys/ebpf_uapi.h>


#define	EBPF_ACTION_CONTINUE	0
#define EBPF_ACTION_DUP		1
#define EBPF_ACTION_OPENAT	2
#define	EBPF_ACTION_FSTATAT	3
#define	EBPF_ACTION_FSTAT	4

struct open_args
{
	int *fd;
	char * path;
	int mode;
	int *action;
};

struct stat_probe_args
{
	int *fd;
	char * path;
	int *action;
};

EBPF_DEFINE_MAP(fd_map,  "hashtable", MAXPATHLEN, sizeof(int), 256, 0);

void open_syscall_probe(struct open_args *args)
{
	void *path = args->path;
	int *fd = ebpf_map_lookup_path(&fd_map, &path);
	if (fd) {
		if (*args->path == '\0') {
			/* Exact match; dup existing descriptor. */
			*args->fd = *fd;
			*args->action = EBPF_ACTION_DUP;
		} else {
			*args->fd = *fd;
			args->path = path + 1;
			*args->action = EBPF_ACTION_OPENAT;
		}
		return;
	}
}


void stat_syscall_probe(struct stat_probe_args *args)
{
	void *path = args->path;
	int *fd = ebpf_map_lookup_path(&fd_map, &path);
	if (fd) {
		if (*args->path == '\0') {
			/* Exact match; fstat existing descriptor. */
			*args->fd = *fd;
			*args->action = EBPF_ACTION_FSTAT;
		} else {
			*args->fd = *fd;
			args->path = path + 1;
			*args->action = EBPF_ACTION_FSTATAT;
		}
		return;
	}
}


