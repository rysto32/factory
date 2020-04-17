#!/bin/sh

c++ -c -std=c++17 -target bpf -Wall -o open.o -O2 syscall_rewriter.cpp

