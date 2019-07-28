
LIB := build_sample

SRCS := \
	main.cpp \

PROG := bin/build

PROG_LIBS := \
	build_sample \
	job \
	perm \
	product \
	msgsocket \
	eventloop \
	config \
	temp_files \

PROG_STDLIBS := \
	event_core \
	nv \
	ucl \


