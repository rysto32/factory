
LIB := tar_wrapper

SRCS := \
	tar.cpp \

PROG := bin/captar

PROG_LIBS := \
	tar_wrapper \
	capsicum_sb \
	ebpf \
	perm \

PROG_STDLIBS := \
	elf \
	gbpf \
