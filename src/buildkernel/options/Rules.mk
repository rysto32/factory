
LIB := mkoptions

SRCS := \
	mkoptions.cpp \


PROG := bin/mkoptions

PROG_LIBS := \
	mkoptions \
	config \

PROG_STDLIBS := \
	ucl \
