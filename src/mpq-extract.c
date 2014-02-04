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
#include <fcntl.h>
#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* libmpq includes. */
#include <mpq.h>

/* define new print functions for error. */
#define ERROR(...) fprintf(stderr, __VA_ARGS__);

/* define new print functions for notification. */
#define NOTICE(...) printf(__VA_ARGS__);

/* define size of off_t */
#if _FILE_OFFSET_BITS == 64
#define OFFTSTR "lli"
#else
#define OFFTSTR "li"
#endif

/* this function show the usage. */
int mpq_extract__usage(char *program_name) {

	/* show the help. */
	NOTICE("Usage: %s [OPTION] [ARCHIVE]...\n", program_name);
	NOTICE("Extracts files from a mpq-archive. (Example: %s d2speech.mpq)\n", program_name);
	NOTICE("\n");
	NOTICE("  -h, --help		shows this help screen\n");
	NOTICE("  -v, --version		shows the version information\n");
	NOTICE("  -e, --extract		extract files from the given mpq archive\n");
	NOTICE("  -l, --list		list the contents of the mpq archive\n");
	NOTICE("  -n, --name		extract one or more files by name\n");
	NOTICE("\n");
	NOTICE("Please report bugs to the appropriate authors, which can be found in the\n");
	NOTICE("version information. All other things can be send to <%s>\n", PACKAGE_BUGREPORT);

        /* if no error was found, return zero. */
        return 0;
}

/* this function shows the version information. */
int mpq_extract__version(char *program_name) {

	/* show the version. */
	NOTICE("%s (mopaq) %s (libmpq %s)\n", program_name, VERSION, libmpq__version());
	NOTICE("Written by %s\n", AUTHOR);
	NOTICE("\n");
	NOTICE("This is free software; see the source for copying conditions.  There is NO\n");
	NOTICE("warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");

	/* if no error was found, return zero. */
	return 0;
}

/* XXX: this is a hack to make mpq-tools compile until proper
 * listfile support is added. */
int32_t libmpq__file_name(mpq_archive_s *mpq_archive, uint32_t file_number, char *filename, size_t filename_size) {

	int32_t result = 0;

	if ((result = snprintf(filename, filename_size, "file%06i.xxx", file_number)) < 0) {
		return LIBMPQ_ERROR_FORMAT;
	}
	return result;
}

/* this function will list the archive content. */
int mpq_extract__list(char *mpq_filename, unsigned int file_number, unsigned int number, unsigned int files) {

	/* some common variables. */
	int result               = 0;
	off_t size_packed        = 0;
	off_t size_unpacked      = 0;
	unsigned int total_files = 0;
	unsigned int encrypted   = 0;
	unsigned int compressed  = 0;
	unsigned int imploded    = 0;
	unsigned int i;
	static char filename[PATH_MAX];
	mpq_archive_s *mpq_archive;

	/* open the mpq-archive. */
	if ((result = libmpq__archive_open(&mpq_archive, mpq_filename, -1)) < 0) {

		/* something on open file failed. */
		return result;
	}

	/* fetch number of files. */
	libmpq__archive_files(mpq_archive, &total_files);

	/* check if we should process all files. */
	if (file_number != -1) {

		/* check if processing multiple files. */
		if (number > 0 && files > 1 && number < files) {

			/* show empty line. */
			NOTICE("\n");
		}

		if (file_number > total_files - 1) {
			return LIBMPQ_ERROR_EXIST;
		}

		/* fetch information. */
		libmpq__file_size_packed(mpq_archive, file_number, &size_packed);
		libmpq__file_size_unpacked(mpq_archive, file_number, &size_unpacked);
		libmpq__file_encrypted(mpq_archive, file_number, &encrypted);
		libmpq__file_compressed(mpq_archive, file_number, &compressed);
		libmpq__file_imploded(mpq_archive, file_number, &imploded);
		libmpq__file_name(mpq_archive, file_number, filename, PATH_MAX);

		/* show the file information. */
		NOTICE("file number:			%i/%i\n", file_number, total_files);
		NOTICE("file packed size:		%" OFFTSTR "\n", size_packed);
		NOTICE("file unpacked size:		%" OFFTSTR "\n", size_unpacked);
		NOTICE("file compression ratio:		%.2f%%\n", (100 - fabs(((float)size_packed / (float)size_unpacked * 100))));
		NOTICE("file compressed:		%s\n", compressed ? "yes" : "no");
		NOTICE("file imploded:			%s\n", imploded ? "yes" : "no");
		NOTICE("file encrypted:			%s\n", encrypted ? "yes" : "no");
		NOTICE("file name:			%s\n", filename);
	} else {

		/* show header. */
		NOTICE("number   ucmp. size   cmp. size   ratio   cmp   imp   enc   filename\n");
		NOTICE("------   ----------   ---------   -----   ---   ---   ---   --------\n");

		/* loop through all files. */
		for (i = 0; i < total_files; i++) {

			/* cleanup variables. */
			size_packed   = 0;
			size_unpacked = 0;
			encrypted     = 0;
			compressed    = 0;
			imploded      = 0;

			/* fetch sizes. */
			libmpq__file_size_packed(mpq_archive, i, &size_packed);
			libmpq__file_size_unpacked(mpq_archive, i, &size_unpacked);
			libmpq__file_encrypted(mpq_archive, i, &encrypted);
			libmpq__file_compressed(mpq_archive, i, &compressed);
			libmpq__file_imploded(mpq_archive, i, &imploded);
			libmpq__file_name(mpq_archive, i, filename, PATH_MAX);

			/* show file information. */
			NOTICE("  %4i   %10" OFFTSTR "   %9" OFFTSTR " %6.0f%%   %3s   %3s   %3s   %s\n",
				i,
				size_packed,
				size_unpacked,
				(100 - fabs(((float)size_packed / (float)size_unpacked * 100))),
				compressed ? "yes" : "no",
				imploded ? "yes" : "no",
				encrypted ? "yes" : "no",
				filename
			);
		}

		/* cleanup variables. */
		size_packed   = 0;
		size_unpacked = 0;

		/* fetch sizes. */
		libmpq__archive_size_packed(mpq_archive, &size_packed);
		libmpq__archive_size_unpacked(mpq_archive, &size_unpacked);

		/* show footer. */
		NOTICE("------   ----------   ---------   -----   ---   ---   ---   --------\n");
		NOTICE("  %4i   %10" OFFTSTR "   %9" OFFTSTR " %6.0f%%   %s\n",
			total_files,
			size_packed,
			size_unpacked,
			(100 - fabs(((float)size_packed / (float)size_unpacked * 100))),
			mpq_filename);
	}

	/* always close file descriptor, file could be opened also if it is no valid mpq archive. */
	libmpq__archive_close(mpq_archive);

	/* if no error was found, return zero. */
	return 0;
}

/* this function extract a single file from archive. */
int mpq_extract__extract_file(mpq_archive_s *mpq_archive, unsigned int file_number, FILE *fp) {

	/* some common variables. */
	static char filename[PATH_MAX];
	unsigned char *out_buf;
	off_t transferred = 0;
	off_t out_size    = 0;
	int result        = 0;

	libmpq__file_name(mpq_archive, file_number, filename, PATH_MAX);

	/* get/show filename to extract. */
	if (filename == NULL) {

		/* filename was not found. */
		return LIBMPQ_ERROR_EXIST;
	}

	NOTICE("extracting %s\n", filename);

	libmpq__file_size_unpacked(mpq_archive, file_number, &out_size);

	if ((out_buf = malloc(out_size)) == NULL)
		return LIBMPQ_ERROR_MALLOC;

	if ((result = libmpq__file_read(mpq_archive, file_number, out_buf, out_size, &transferred)) < 0)
		return result;

	fwrite(out_buf, 1, out_size, fp);

	/* free output buffer. */
	free(out_buf);

	/* if no error was found, return zero. */
	return 0;
}

/* this function will extract the archive content. */
int mpq_extract__extract(char *mpq_filename, unsigned int file_number) {

	/* some common variables. */
	mpq_archive_s *mpq_archive;
	static char filename[PATH_MAX];
	unsigned int i;
	unsigned int total_files = 0;
	int result               = 0;
	FILE *fp;

	/* open the mpq-archive. */
	if ((result = libmpq__archive_open(&mpq_archive, mpq_filename, -1)) < 0) {

		/* something on open archive failed. */
		return result;
	}

	/* check if we should process all files. */
	if (file_number != -1) {

		/* get filename. */
		libmpq__file_name(mpq_archive, file_number, filename, PATH_MAX);

		if (filename == NULL) {

			/* filename was not found. */
			return LIBMPQ_ERROR_EXIST;
		}

		/* open file for writing. */
		if ((fp = fopen(filename, "wb")) == NULL) {

			/* open file failed. */
			return LIBMPQ_ERROR_OPEN;
		}

		/* extract file. */
		if ((result = mpq_extract__extract_file(mpq_archive, file_number, fp)) < 0) {

			/* close file. */
			if ((fclose(fp)) < 0) {

				/* close file failed. */
				return LIBMPQ_ERROR_CLOSE;
			}

			/* always close file descriptor, file could be opened also if it is no valid mpq archive. */
			libmpq__archive_close(mpq_archive);

			/* something on extracting file failed. */
			return result;
		}

		/* close file. */
		if ((fclose(fp)) < 0) {

			/* close file failed. */
			return LIBMPQ_ERROR_CLOSE;
		}
	} else {

		/* fetch number of files. */
		libmpq__archive_files(mpq_archive, &total_files);

		/* loop through all files. */
		for (i = 0; i < total_files; i++) {

			/* get filename. */
			libmpq__file_name(mpq_archive, i, filename, PATH_MAX);

			/* check if file exist. */
			if (filename == NULL) {

				/* filename was not found. */
				return LIBMPQ_ERROR_EXIST;
			}

			/* open file for writing. */
			if ((fp = fopen(filename, "wb")) == NULL) {

				/* open file failed. */
				return LIBMPQ_ERROR_OPEN;
			}

			/* extract file. */
			if ((result = mpq_extract__extract_file(mpq_archive, i, fp)) < 0) {

				/* close file. */
				if ((fclose(fp)) < 0) {

					/* close file failed. */
					return LIBMPQ_ERROR_CLOSE;
				}

				/* always close file descriptor, file could be opened also if it is no valid mpq archive. */
				libmpq__archive_close(mpq_archive);

				/* something on extracting file failed. */
				return result;
			}

			/* close file. */
			if ((fclose(fp)) < 0) {

				/* close file failed. */
				return LIBMPQ_ERROR_CLOSE;
			}
		}
	}

	/* always close file descriptor, file could be opened also if it is no valid mpq archive. */
	libmpq__archive_close(mpq_archive);

	/* if no error was found, return zero. */
	return 0;
}

int mpq_extract__file_by_name(char *mpq_filename, char **argv, int arg_first, int arg_last) {
	/* some common variables. */
	int result = 0;
	mpq_archive_s *mpq_archive;
	FILE *fp;
	unsigned char *out_buf;
	off_t transferred = 0;
	off_t out_size = 0;
	int32_t file_number;
	char *filename;
	
	/* open the mpq-archive. */
	if ((result = libmpq__archive_open(&mpq_archive, mpq_filename, -1)) < 0) {
		/* something on open file failed. */
		ERROR("Failed to open %s\n", filename);
		return result;
	}
	
	int i;
	for (i = arg_first; i < arg_last; i++) {
		file_number = -1;
		filename = argv[i];
		
		NOTICE("Extracting %s\n", filename);
		
		/* get file number by name */
		libmpq__file_number(mpq_archive, filename, &file_number);
	
		if (file_number == -1) {
			ERROR("No such file or directory\n");
			continue;
		}
		
		/* open file for writing. */
		if ((fp = fopen(filename, "wb")) == NULL) {
			/* open file failed. */
			ERROR("Failed to create file\n");
			continue;
		}
	
		libmpq__file_size_unpacked(mpq_archive, file_number, &out_size);

		if ((out_buf = malloc(out_size)) == NULL) {
			ERROR("Failed to allocate buffer\n");
			continue;
		}

		if ((result = libmpq__file_read(mpq_archive, file_number, out_buf, out_size, &transferred)) < 0) {
			/*/ fix a memory leak */
			free(out_buf);
			ERROR("Failed to extract file\n");
			continue;
		}
		/* write to file */
		fwrite(out_buf, 1, out_size, fp);
		/* free output buffer. */
		free(out_buf);

		/* extract file. */
		/* close file. */
		if ((fclose(fp)) < 0) {
			/* close file failed. */
			ERROR("Failed to close file\n");
			continue;
		}
		
		NOTICE("OK\n");
	}
	/* always close file descriptor, file could be opened also if it is no valid mpq archive. */
	libmpq__archive_close(mpq_archive);
	return 0;
}

/* the main function starts here. */
int main(int argc, char **argv) {

	/* common variables for the command line. */
	int result;
	int opt;
	int option_index = 0;
	static char const short_options[] = "hvelnf:";
	static struct option const long_options[] = {
		{"help",	no_argument,		0,	'h'},
		{"version",	no_argument,		0,	'v'},
		{"extract",	no_argument,		0,	'e'},
		{"list",	no_argument,		0,	'l'},
		{"name",	no_argument,		0,	'n'},
		{0,		0,			0,	0}
	};
	optind = 0;
	opterr = 0;

	/* some common variables. */
	char *program_name;
	char mpq_filename[PATH_MAX];
	unsigned int action = 0;
	unsigned int count;

	/* get program name. */
	program_name = argv[0];
	if (program_name && strrchr(program_name, '/')) {
		program_name = strrchr(program_name, '/') + 1;
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
			case 'n':
				strncpy(mpq_filename, argv[2], PATH_MAX);
				// get rest of names
				if (argc <= 3) {
					ERROR("Missing filenames to extract\n");
					exit(1);
				}
				mpq_extract__file_by_name(mpq_filename, argv, 3, argc);
				exit(0);
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
	strncpy(mpq_filename, argv[optind++], PATH_MAX);

	/* count number of files to process in archive. */
	count = argc - optind;

	/* process file names. */
	do {
		unsigned int file_number = 0;

		if (argv[optind]) {
			file_number = strtol (argv[optind], NULL, 10);

			/* check whether we were given a (valid) file number. */
			if (!file_number) {
				ERROR("%s: invalid file number '%s'\n", program_name, argv[optind]);
				exit(1);
			}
		}

		/* check if we should list archive only. */
		if (action == 1) {

			/* process archive. */
			result = mpq_extract__list(mpq_filename, file_number - 1, argc - optind, count);
		}

		/* check if we should extract archive content. */
		if (action == 2) {
			/* extract archive content. */
			result = mpq_extract__extract(mpq_filename, file_number - 1);
		}

		/* check if archive was correctly opened. */
		if (result == LIBMPQ_ERROR_OPEN) {

			/* open archive failed. */
			ERROR("%s: '%s' no such file or directory\n", program_name, mpq_filename);

			/* if archive did not exist, we can stop everything. :) */
			exit(1);
		}

		/* check if file in archive exist. */
		if (result == LIBMPQ_ERROR_EXIST) {

			/* file was not found in archive. */
			ERROR("%s: '%s' no such file or directory in archive '%s'\n", program_name, argv[optind], mpq_filename);

			/* if file did not exist, we continue to next file. */
			continue;
		}
	} while (++optind < argc);

	/* execution was successful. */
	exit(0);
}
