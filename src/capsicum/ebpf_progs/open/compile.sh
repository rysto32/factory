#!/bin/sh

c++ -std=c++17 -target bpf -Wall -o open.o -O2 syscall_rewriter.cpp -I ~/repos/generic-ebpf/sys/ -I ~/repos/bsd-worktree/epbf-import/sys -c

