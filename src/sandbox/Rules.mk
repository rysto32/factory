
LIB :=	factory_sandbox
SHLIB_NEEDED := 1

SRCS := \
	exec.c \
	interpose.c \
	msgsock.c \
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

