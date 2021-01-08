dnl Copyright (C) 2021  Trevor Woerner <twoerner@gmail.com>
dnl SPDX-License-Identifier: OSL-3.0

dnl AC_PREREQ(2.57)
AC_INIT([ROMsearch], 1.0.0, twoerner@gmail.com, ROMsearch)
AC_CONFIG_SRCDIR(src/ROMsearch.c)
AC_CONFIG_AUX_DIR(cfg)
AM_INIT_AUTOMAKE([foreign no-dist-gzip dist-bzip2 1.9])
AM_CONFIG_HEADER(cfg/config.h)

SUBDIRS="src"

dnl **********************************
dnl checks for programs
dnl **********************************
AC_PROG_CC
AC_PROG_CPP
AC_PROG_MAKE_SET
AC_PROG_INSTALL
AC_PROG_LN_S

dnl m4_ifdef([AM_PATH_CHECK],[
dnl AM_PATH_CHECK(0.8.2, [HAVE_CHECK=true], [HAVE_CHECK=false])
dnl ])
dnl AM_CONDITIONAL(COND_HAVE_CHECK, test x$HAVE_CHECK = xtrue)
dnl AC_SUBST(HAVE_CHECK)

dnl **********************************
dnl checks for header files
dnl **********************************
AC_HEADER_STDC
AC_CHECK_HEADERS(stdio.h stdint.h stdlib.h stdbool.h inttypes.h string.h)
AC_CHECK_HEADERS(unistd.h fcntl.h errno.h poll.h time.h getopt.h signal.h setjmp.h)
AC_CHECK_HEADERS(sys/types.h sys/stat.h)

dnl **********************************
dnl checks for typedefs, structs, and
dnl compiler characteristics
dnl **********************************
dnl AC_TYPE_SIZE_T

dnl **********************************
dnl checks for library functions
dnl **********************************
dnl AC_CHECK_FUNCS(select)

dnl **********************************
dnl other stuff
dnl **********************************
#if test x$HAVE_CHECK = xtrue; then
#	SUBDIRS="$SUBDIRS tests"
#fi
AC_SUBST(SUBDIRS)

dnl **********************************
dnl output
dnl **********************************
AC_OUTPUT(Makefile
cfg/Makefile
src/Makefile)
