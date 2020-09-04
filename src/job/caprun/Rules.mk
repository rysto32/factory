
PROG :=	bin/caprun

PROG_LIBS := \
	caprun \
	job \
	perm \
	product \
	capsicum_sb \
	ebpf \
	preload_sb \
	msgsocket \
	eventloop \
	temp_files \

PROG_STDLIBS := \
	event_core \
	elf \
	gbpf \

LIB :=	caprun

SRCS := \
	Caprun.cpp \
