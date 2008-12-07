# the autoconf initilization.
AC_INIT(mpq-tools, 0.4.2, [mbroemme@plusserver.de], [mpq-tools])

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
AC_FUNC_FSEEKO

# checking for pkg-config.
AC_CHECK_PROG([have_pkg_config], [pkg-config], [yes])

# checking if we have pkg-config or not.
if test "$have_pkg_config" = "yes"; then

	# checking for libmpq.
	PKG_CHECK_MODULES([LIBMPQ], [libmpq >= 0.4.2], [HAVE_LIBMPQ="yes"], [AC_MSG_ERROR([*** $LIBMPQ_PKG_ERRORS])])
fi
if test -z "$have_pkg_config"; then

	# checking for mpq library.
	AC_CHECK_HEADER([libmpq/mpq.h], [], [AC_MSG_ERROR([*** mpq.h is required, install libmpq header files])])
	AC_PATH_PROG([libmpq_config], [libmpq-config], [AC_MSG_ERROR([*** libmpq-config is required, install libmpq development files])])

	# checking for libmpq environment.
	if test -n "$libmpq_config"; then
		LIBMPQ_CFLAGS="$(libmpq-config --cflags)"
		LIBMPQ_LIBS="$(libmpq-config --libs)"
	fi
fi

# export library flags.
AC_SUBST(LIBMPQ_CFLAGS)
AC_SUBST(LIBMPQ_LIBS)

# creating files.
AC_OUTPUT([
Makefile
src/Makefile
doc/Makefile
doc/man1/Makefile
])
