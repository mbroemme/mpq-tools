/*
 *  mpq-extract.c -- functions for extract files from a given mpq archive.
 *
 *  Copyright (c) 2003-2008 Maik Broemme <mbroemme@plusserver.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id: mpq-extract.c,v 1.18 2004/02/12 00:39:17 mbroemme Exp $
 */

/* mpq-tools configuration includes. */
#include "config.h"

/* generic includes. */
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* libmpq includes. */
#include <libmpq/mpq.h>

/* define new print functions for error. */
#define ERROR(...) fprintf(stderr, __VA_ARGS__);

/* define new print functions for notification. */
#define NOTICE(...) printf(__VA_ARGS__);

/* File numbers are zero-based internally. Omission selects every file. */
#define ALL_FILES UINT32_MAX

/* A generated filename holds at most ten decimal digits plus its affixes and NUL. */
#define GENERATED_FILENAME_SIZE sizeof("file4294967295.xxx")

/* Local failures use positive statuses, distinct from libmpq errors. */
enum
{
    MEMBER_OUT_OF_RANGE = 1,
    OUTPUT_OPEN_FAILED,
    OUTPUT_IO_FAILED
};

/* this function show the usage. */
int
mpq_extract__usage(char *program_name)
{

    /* show the help. */
    NOTICE("Usage: %s [OPTION] [ARCHIVE]...\n", program_name);
    NOTICE("Extracts files from a mpq-archive. (Example: %s d2speech.mpq)\n", program_name);
    NOTICE("\n");
    NOTICE("  -h, --help		shows this help screen\n");
    NOTICE("  -v, --version		shows the version information\n");
    NOTICE("  -e, --extract		extract files from the given mpq archive\n");
    NOTICE("  -l, --list		list the contents of the mpq archive\n");
    NOTICE("\n");
    NOTICE("Please report bugs to the appropriate authors, which can be found in the\n");
    NOTICE("version information. All other things can be send to <%s>\n", PACKAGE_BUGREPORT);

    /* if no error was found, return zero. */
    return 0;
}

/* this function shows the version information. */
int
mpq_extract__version(char *program_name)
{

    /* show the version. */
    NOTICE("%s (mopaq) %s (libmpq %s)\n", program_name, VERSION, libmpq__version());
    NOTICE("Written by Maik Broemme <mbroemme@libmpq.org>\n");
    NOTICE("\n");
    NOTICE("This is free software; see the source for copying conditions.  There is NO\n");
    NOTICE("warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");

    /* if no error was found, return zero. */
    return 0;
}

/* Generate a name until listfile support is available. */
static int32_t
mpq_extract__file_name(uint32_t file_number, char *filename, size_t filename_size)
{
    int result = snprintf(filename, filename_size, "file%06" PRIu32 ".xxx", file_number);
    if (result < 0) {
        return LIBMPQ_ERROR_FORMAT;
    }
    if ((size_t)result >= filename_size) {
        return LIBMPQ_ERROR_SIZE;
    }
    return 0;
}

/* this function will list the archive content. */
static int32_t
mpq_extract__list(
    const char *mpq_filename, uint32_t file_number, unsigned int number, unsigned int files
)
{

    /* some common variables. */
    int32_t result;
    int32_t close_result;
    libmpq__off_t size_packed;
    libmpq__off_t size_unpacked;
    uint32_t total_files;
    uint32_t flags;
    uint32_t i;
    char filename[GENERATED_FILENAME_SIZE];
    mpq_archive_s *mpq_archive;

    /* open the mpq-archive. */
    if ((result = libmpq__archive_open(&mpq_archive, mpq_filename, -1)) != 0) {

        /* something on open file failed. */
        return result;
    }

    /* fetch number of files. */
    result = libmpq__archive_files(mpq_archive, &total_files);
    if (result != 0) {
        goto close_archive;
    }

    /* check if we should process all files. */
    if (file_number != ALL_FILES) {

        /* check if processing multiple files. */
        if (number > 0 && files > 1 && number < files) {

            /* show empty line. */
            NOTICE("\n");
        }

        if (file_number >= total_files) {
            result = MEMBER_OUT_OF_RANGE;
            goto close_archive;
        }

        /* fetch information. */
        if ((result = libmpq__file_size_packed(mpq_archive, file_number, &size_packed)) != 0 ||
            (result = libmpq__file_size_unpacked(mpq_archive, file_number, &size_unpacked)) != 0 ||
            (result = libmpq__file_flags(mpq_archive, file_number, &flags)) != 0 ||
            (result = mpq_extract__file_name(file_number, filename, sizeof(filename))) != 0) {
            goto close_archive;
        }

        /* show the file information. */
        NOTICE(
            "file number:			%" PRIu32 "/%" PRIu32 "\n", file_number, total_files
        );
        NOTICE("file packed size:		%" PRId64 "\n", size_packed);
        NOTICE("file unpacked size:		%" PRId64 "\n", size_unpacked);
        NOTICE(
            "file compression ratio:		%.2f%%\n",
            (100 - fabs(((float)size_packed / (float)size_unpacked * 100)))
        );
        NOTICE(
            "file compressed:		%s\n", (flags & LIBMPQ_FILE_FLAG_COMPRESS) ? "yes" : "no"
        );
        NOTICE(
            "file imploded:			%s\n",
            (flags & LIBMPQ_FILE_FLAG_IMPLODE) ? "yes" : "no"
        );
        NOTICE(
            "file encrypted:			%s\n",
            (flags & LIBMPQ_FILE_FLAG_ENCRYPTED) ? "yes" : "no"
        );
        NOTICE("file name:			%s\n", filename);
    } else {

        /* show header. */
        NOTICE("number   ucmp. size   cmp. size   ratio   cmp   imp   enc   filename\n");
        NOTICE("------   ----------   ---------   -----   ---   ---   ---   --------\n");

        /* loop through all files. */
        for (i = 0; i < total_files; i++) {

            /* fetch sizes. */
            if ((result = libmpq__file_size_packed(mpq_archive, i, &size_packed)) != 0 ||
                (result = libmpq__file_size_unpacked(mpq_archive, i, &size_unpacked)) != 0 ||
                (result = libmpq__file_flags(mpq_archive, i, &flags)) != 0 ||
                (result = mpq_extract__file_name(i, filename, sizeof(filename))) != 0) {
                goto close_archive;
            }

            /* show file information. */
            NOTICE(
                "  %4" PRIu32 "   %10" PRId64 "   %9" PRId64 " %6.0f%%   %3s   %3s   %3s   %s\n", i,
                size_packed, size_unpacked,
                (100 - fabs(((float)size_packed / (float)size_unpacked * 100))),
                (flags & LIBMPQ_FILE_FLAG_COMPRESS) ? "yes" : "no",
                (flags & LIBMPQ_FILE_FLAG_IMPLODE) ? "yes" : "no",
                (flags & LIBMPQ_FILE_FLAG_ENCRYPTED) ? "yes" : "no", filename
            );
        }

        /* The archive size queries add to these totals. */
        size_packed = 0;
        size_unpacked = 0;

        /* fetch sizes. */
        if ((result = libmpq__archive_size_packed(mpq_archive, &size_packed)) != 0 ||
            (result = libmpq__archive_size_unpacked(mpq_archive, &size_unpacked)) != 0) {
            goto close_archive;
        }

        /* show footer. */
        NOTICE("------   ----------   ---------   -----   ---   ---   ---   --------\n");
        NOTICE(
            "  %4" PRIu32 "   %10" PRId64 "   %9" PRId64 " %6.0f%%   %s\n", total_files,
            size_packed, size_unpacked,
            (100 - fabs(((float)size_packed / (float)size_unpacked * 100))), mpq_filename
        );
    }

close_archive:
    close_result = libmpq__archive_close(mpq_archive);
    return result != 0 ? result : close_result;
}

static void
mpq_extract__output_error(const char *filename, const char *operation, int error)
{
    ERROR(
        "'%s': cannot %s output: %s\n", filename, operation,
        error != 0 ? strerror(error) : "I/O error"
    );
}

/* Extract one member to its generated filename. */
static int32_t
mpq_extract__extract_file(mpq_archive_s *mpq_archive, uint32_t file_number)
{
    char filename[GENERATED_FILENAME_SIZE];
    int32_t result = mpq_extract__file_name(file_number, filename, sizeof(filename));
    if (result != 0) {
        return result;
    }

    libmpq__off_t out_size;
    result = libmpq__file_size_unpacked(mpq_archive, file_number, &out_size);
    if (result != 0) {
        return result;
    }
    if (out_size < 0 || (uintmax_t)out_size > SIZE_MAX) {
        return LIBMPQ_ERROR_SIZE;
    }

    size_t capacity = (size_t)out_size;
    uint8_t *buffer = NULL;
    if (capacity != 0) {
        buffer = malloc(capacity);
        if (buffer == NULL) {
            return LIBMPQ_ERROR_MALLOC;
        }
    }

    /* Supply a valid buffer even when the logical member is empty. */
    uint8_t empty = 0;
    uint8_t *out_buf = buffer != NULL ? buffer : &empty;
    libmpq__off_t transferred = -1;
    result = libmpq__file_read(mpq_archive, file_number, out_buf, out_size, &transferred);
    if (result != 0) {
        goto free_buffer;
    }
    if (transferred != out_size) {
        result = LIBMPQ_ERROR_SIZE;
        goto free_buffer;
    }

    errno = 0;
    FILE *fp = fopen(filename, "wb");
    if (fp == NULL) {
        mpq_extract__output_error(filename, "open", errno);
        result = OUTPUT_OPEN_FAILED;
        goto free_buffer;
    }

    NOTICE("extracting %s\n", filename);

    size_t bytes = (size_t)transferred;
    errno = 0;
    if (fwrite(out_buf, 1, bytes, fp) != bytes) {
        mpq_extract__output_error(filename, "write", errno);
        result = OUTPUT_IO_FAILED;
    }

    errno = 0;
    int close_result = fclose(fp);
    if (close_result != 0 && result == 0) {
        mpq_extract__output_error(filename, "close", errno);
        result = OUTPUT_IO_FAILED;
    }

free_buffer:
    free(buffer);
    return result;
}

/* Extract one member or all members in archive order. */
static int32_t
mpq_extract__extract(const char *mpq_filename, uint32_t file_number)
{
    mpq_archive_s *mpq_archive;
    int32_t result = libmpq__archive_open(&mpq_archive, mpq_filename, -1);
    if (result != 0) {
        return result;
    }

    uint32_t total_files;
    result = libmpq__archive_files(mpq_archive, &total_files);
    if (result == 0) {
        if (file_number == ALL_FILES) {
            for (uint32_t i = 0; i < total_files; i++) {
                result = mpq_extract__extract_file(mpq_archive, i);
                if (result != 0) {
                    break;
                }
            }
        } else if (file_number < total_files) {
            result = mpq_extract__extract_file(mpq_archive, file_number);
        } else {
            result = MEMBER_OUT_OF_RANGE;
        }
    }

    int32_t close_result = libmpq__archive_close(mpq_archive);
    return result != 0 ? result : close_result;
}

/* the main function starts here. */
int
main(int argc, char **argv)
{

    /* common variables for the command line. */
    int32_t result;
    int opt;
    int option_index = 0;
    static char const short_options[] = "hvelf:";
    static struct option const long_options[] = { { "help", no_argument, 0, 'h' },
                                                  { "version", no_argument, 0, 'v' },
                                                  { "extract", no_argument, 0, 'e' },
                                                  { "list", no_argument, 0, 'l' },
                                                  { 0, 0, 0, 0 } };
    optind = 0;
    opterr = 0;

    /* some common variables. */
    char *program_name;
    unsigned int action = 0;
    unsigned int count;

    /* get program name. */
    program_name = argv[0];
    char *slash = program_name ? (strrchr)(program_name, '/') : NULL;
    if (slash) {
        program_name = slash + 1;
    }

    /* if no command line option was given, show some info. */
    if (argc <= 1) {

        /* show some info on how to get help. :) */
        ERROR("%s: no action was given\n", program_name);
        ERROR("Try `%s --help' for more information.\n", program_name);

        /* exit with error. */
        exit(1);
    }

    /* if no command line option was given, show some info. */
    if (argc <= 1) {

        /* show some info on how to get help. :) */
        ERROR("%s: no action was given\n", program_name);
        ERROR("Try `%s --help' for more information.\n", program_name);

        /* exit with error. */
        exit(1);
    }

    /* parse command line. */
    while ((opt = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {

        /* check if all command line options are parsed. */
        if (opt == -1) {
            break;
        }

        /* parse option. */
        switch (opt) {
        case 'h':
            mpq_extract__usage(program_name);
            exit(0);
        case 'v':
            mpq_extract__version(program_name);
            exit(0);
        case 'l':
            action = 1;
            continue;
        case 'e':
            action = 2;
            continue;
        default:

            /* show some info on how to get help. :) */
            ERROR("%s: unrecognized option `%s'\n", program_name, argv[optind - 1]);
            ERROR("Try `%s --help' for more information.\n", program_name);

            /* exit with error. */
            exit(1);
        }
    }

    if (!action) {
        ERROR("%s: no action given.\n", program_name);

        ERROR("Try `%s --help' for more information.\n", program_name);

        /* exit with error. */
        exit(1);
    }

    if (optind >= argc) {
        ERROR("%s: no archive given.\n", program_name);

        ERROR("Try `%s --help' for more information.\n", program_name);

        /* exit with error. */
        exit(1);
    }

    /* we assume first parameter which is left as archive. */
    const char *mpq_filename = argv[optind++];

    /* count number of files to process in archive. */
    count = argc - optind;

    /* process file names. */
    do {
        uint32_t file_number = ALL_FILES;

        if (argv[optind]) {
            file_number = strtol(argv[optind], NULL, 10);

            /* check whether we were given a (valid) file number. */
            if (!file_number) {
                ERROR("%s: invalid file number '%s'\n", program_name, argv[optind]);
                exit(1);
            }
            file_number--;
        }

        /* List or extract the selected members. */
        if (action == 1) {
            result = mpq_extract__list(mpq_filename, file_number, argc - optind, count);
        } else {
            result = mpq_extract__extract(mpq_filename, file_number);
        }

        if (result == MEMBER_OUT_OF_RANGE) {
            ERROR(
                "%s: '%s' no such file or directory in archive '%s'\n", program_name, argv[optind],
                mpq_filename
            );

        } else if (result < 0) {
            const char *message = libmpq__strerror(result);
            ERROR(
                "%s: '%s': %s (%" PRId32 ")\n", program_name, mpq_filename,
                message != NULL ? message : "unknown libmpq error", result
            );
        }

        if (result == LIBMPQ_ERROR_OPEN || result == OUTPUT_OPEN_FAILED) {
            exit(1);
        }
    } while (++optind < argc);

    /* execution was successful. */
    exit(0);
}
