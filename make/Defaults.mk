
CC:=cc
CXX:=c++
LD:=$(CXX)
LD_C:=ld
AR:=ar

#CWARNFLAGS.gcc += -Wno-expansion-to-defined -Wno-extra -Wno-unused-but-set-variable

C_OPTIM:=-O3 -fno-omit-frame-pointer

C_WARNFLAGS := -Wall -Werror

CXX_STD:=-std=c++17 -ftemplate-backtrace-limit=0
CXX_WARNFLAGS:=-Wno-user-defined-literals

CFLAGS:=-I/usr/local/include -I/usr/local/include/lua53 -I$(TOPDIR)/include $(C_OPTIM) -g $(C_WARNFLAGS)
CXXFLAGS:=$(CXX_STD) $(CXX_WARNFLAGS)

# I don't know why ld needs this -L/usr/lib...
LDFLAGS := -L/usr/lib -L/usr/local/lib
