# the autoconf initilization.
AC_INIT(mpq-tools, 0.4.1, [mbroemme@plusserver.de], [mpq-tools])

# detect the canonical host and target build environment.
AC_CANONICAL_SYSTEM

# initialize autoconf and automake system.
AM_INIT_AUTOMAKE([no-dependencies])
AC_CONFIG_HEADERS([config.h:config.h.in])

# create define for the package author.
AC_DEFINE([AUTHOR], ["Maik Broemme <mbroemme@plusserver.de>"], [Define to the author of this package.])

# notices.
AC_PREREQ(2.53)
AC_REVISION($Revision: 1.6 $)

# checking for programs.
AC_PROG_CC
AC_SYS_LARGEFILE

# check for libmpq.
PKG_CHECK_MODULES([LIBMPQ], [libmpq >= 0.4.1], [HAVE_LIBMPQ="yes"], [AC_MSG_ERROR([*** $LIBMPQ_PKG_ERRORS])])
AC_SUBST(LIBMPQ_CFLAGS)
AC_SUBST(LIBMPQ_LIBS)

# creating files.
AC_OUTPUT([
Makefile
src/Makefile
doc/Makefile
doc/man1/Makefile
])
