## Copyright (C) 2021  Trevor Woerner <twoerner@gmail.com>
## SPDX-License-Identifier: OSL-3.0

########################
## src/Makefile.am
########################
SUBDIRS =
AM_CFLAGS = -Wall -Werror -Wextra -Wconversion -Wreturn-type -Wstrict-prototypes

bin_PROGRAMS = ROMsearch tester
ROMsearch_SOURCES = ROMsearch.c common.c common.h
tester_SOURCES = tester.c common.c common.h

clean-local::
	$(RM) toTesterFifoFd fmTesterFifoFd
