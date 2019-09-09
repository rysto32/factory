
#include <sys/param.h>
#include <sys/ebpf.h>
#include <sys/ebpf_uapi.h>


#define	EBPF_ACTION_CONTINUE	0
#define EBPF_ACTION_DUP		1
#define EBPF_ACTION_OPENAT	2

struct open_args
{
	int *fd;
	char * path;
	int mode;
	int *action;
};

EBPF_DEFINE_MAP(fd_map,  "hashtable", MAXPATHLEN, sizeof(int), 256, 0);

//void *bpf_map_lookup_elem(void *map, void *key);


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
