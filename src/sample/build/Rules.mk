
LIB := build_sample

SRCS := \
	main.cpp \

PROG := bin/build

PROG_LIBS := \
	build_sample \
	ingest \
	interpreter \
	lua \
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
	lua-5.3 \


