
PROG :=	bin/runner

PROG_LIBS := \
	runner \
	job \
	perm \
	product \
	msgsocket \
	eventloop \
	temp_files \

PROG_STDLIBS := \
	event_core \
	nv \

LIB :=	runner

SRCS := \
	Runner.cpp \
