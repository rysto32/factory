
.DEFAULT_GOAL:=all

all: programs libraries test

TOPDIR:=.
CURDIR:=.
STACK=x

OUTDIR:=$(TOPDIR)/obj
OBJDIR:=$(OUTDIR)/objects
LIBDIR:=$(OUTDIR)/lib
INSTALLDIR:=$(OUTDIR)/staging/
DEPENDDIR:=$(OUTDIR)/depend/
SDEPENDDIR:=$(OUTDIR)/sdepend/
TESTOBJDIR:=$(OUTDIR)/test_obj
TESTDIR:=$(OUTDIR)/test

include Rules.mk
include make/Subdirs.mk


.PHONY: test

test: $(TEST_PROGS) | programs
	@for test in $(TEST_PROGS); do \
		limits -c 0 ./$${test} || break; \
	done

.PHONY: nothing
nothing:
	@true

programs::

libraries::
