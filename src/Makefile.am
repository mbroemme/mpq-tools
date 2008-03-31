# minimum required automake 1.6
AUTOMAKE_OPTIONS		= 1.6

# the main programs.
bin_PROGRAMS			= mpq-extract mpq-info

# sources for mpq-extract program.
mpq_extract_SOURCES		= mpq-extract.c
mpq_extract_CFLAGS		= @LIBMPQ_CFLAGS@
mpq_extract_LDADD		= @LIBMPQ_LIBS@

# sources for mpq-info program.
mpq_info_SOURCES		= mpq-info.c
mpq_info_CFLAGS			= @LIBMPQ_CFLAGS@
mpq_info_LDADD			= @LIBMPQ_LIBS@
