
LIB := main
SRCS := \
	main.cpp \

PROG := bin/factory

PROG_LIBS := \
	main \
	interpreter \
	lua \
	ingest \
	job \
	perm \
	product \
	preload_sb \
	msgsocket \
	eventloop \
	config \
	temp_files \
	util \

PROG_STDLIBS := \
	event_core \
	nv \
	ucl \
	lua-5.3 \
