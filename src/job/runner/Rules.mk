
PROG :=	bin/runner

PROG_LIBS := \
	runner \
	job \
	perm \
	product \
	capsicum_sb \
	preload_sb \
	msgsocket \
	eventloop \
	temp_files \

PROG_STDLIBS := \
	event_core \
	elf \
	gbpf \

LIB :=	runner

SRCS := \
	Runner.cpp \
