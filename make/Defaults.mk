
CC:=cc
CXX:=c++
LD:=c++
LD_C:=/usr/local/bin/ld
AR:=ar

#CWARNFLAGS.gcc += -Wno-expansion-to-defined -Wno-extra -Wno-unused-but-set-variable

C_OPTIM:=-O3 -fno-omit-frame-pointer

C_WARNFLAGS := -Wall -Werror

CXX_STD:=-std=c++17
CXX_WARNFLAGS:=-Wno-user-defined-literals

CFLAGS:=-I/usr/local/include -I$(TOPDIR)/include $(C_OPTIM) -g $(C_WARNFLAGS)
CXXFLAGS:=$(CXX_STD) $(CXX_WARNFLAGS)
