# Build the main programs.
bin_PROGRAMS = \
	mpq-extract \
	mpq-info

# Compiler and linker flags for libmpq.
AM_CPPFLAGS = $(LIBMPQ_CFLAGS)
LDADD = $(LIBMPQ_LIBS)

# Sources for mpq-extract program.
mpq_extract_SOURCES = mpq-extract.c

# Sources for mpq-info program.
mpq_info_SOURCES = mpq-info.c
