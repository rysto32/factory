
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
	capsicum_sb \
	ebpf \
	msgsocket \
	eventloop \
	config \
	temp_files \
	util \

PROG_STDLIBS := \
	event_core \
	elf \
	gbpf \
	ucl \
	lua-5.3 \
