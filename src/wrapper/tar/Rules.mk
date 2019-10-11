
LIB := tar_wrapper

SRCS := \
	tar.cpp \

PROG := bin/tar_wrapper

PROG_LIBS := \
	tar_wrapper \
	capsicum_sb \
	ebpf \
	perm \

PROG_STDLIBS := \
	elf \
	gbpf \
