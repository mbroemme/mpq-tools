# Copyright (c) 2008, 2026 Maik Broemme <mbroemme@libmpq.org>
#
# This file is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This file is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this file; if not, see <https://www.gnu.org/licenses/>.

# Build the unified command-line utility.
bin_PROGRAMS = mpq

# Compiler and linker flags for libmpq.
AM_CPPFLAGS = $(LIBMPQ_CFLAGS)
LDADD = $(LIBMPQ_LIBS)

mpq_SOURCES = \
	mpq-main.c \
	mpq-cli.c \
	mpq-cli.h \
	mpq-info.c \
	mpq-extract.c
