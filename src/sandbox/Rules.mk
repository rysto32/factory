
LIB :=	factory_sandbox
SHLIB_NEEDED := 1

SHLIB_STDLIBS := \
	nv \

SRCS := \
	close.c \
	exec.c \
	interpose.c \
	open.c \

TESTS := \
	exec \

TEST_EXEC_SRCS := \
	exec.c

TEST_EXEC_STDLIBS := \
	gmock \

TEST_EXEC_WRAPFUNCS := \
	_execve=wrapped_execve \
	fexecve=wrapped_fexecve \
	calloc=mock_calloc \
	open=mock_open \

