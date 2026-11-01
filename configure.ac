# Initialize the Autoconf package metadata.
AC_INIT([mpq-tools], [0.4.2], [mbroemme@libmpq.org], [mpq-tools])

# Detect the canonical build and host environments.
AC_CANONICAL_HOST

# initialize autoconf and automake system.
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_HEADERS([config.h])

# Check for the required build tools.
AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC

# Enable large-file support.
AC_SYS_LARGEFILE

# Check for required libraries.
PKG_CHECK_MODULES([LIBMPQ], [libmpq >= 0.5.0 libmpq < 0.7.0])

# Generate the remaining package build files.
AC_CONFIG_FILES([
	Makefile
	src/Makefile
	docs/man1/Makefile
])

# Complete the configuration process.
AC_OUTPUT
