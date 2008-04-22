/*
 *  mpq-info.c -- functions for information about the given mpq archive.
 *
 *  Copyright (c) 2003-2007 Maik Broemme <mbroemme@plusserver.de>
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
 */

/* generic includes. */
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* libmpq includes. */
#include <mpq.h>

/* mpq-tools configuration includes. */
#include "config.h"

/* define new print functions for error. */
#define ERROR(...) fprintf(stderr, __VA_ARGS__);

/* define new print functions for notification. */
#define NOTICE(...) printf(__VA_ARGS__);

/* this function show the usage. */
int mpq_info__usage(char *program_name) {

	/* show the help. */
	NOTICE("Usage: %s [OPTION] [ARCHIVE]...\n", program_name);
	NOTICE("Displays information of a mpq-archive. (Example: %s d2speech.mpq)\n", program_name);
	NOTICE("\n");
	NOTICE("  -h, --help		shows this help screen\n");
	NOTICE("  -v, --version		shows the version information\n");
	NOTICE("\n");
	NOTICE("Please report bugs to the appropriate authors, which can be found in the\n");
	NOTICE("version information. All other things can be send to <%s>\n", PACKAGE_BUGREPORT);

	/* if no error was found, return zero. */
	return 0;
}

/* this function shows the version information. */
int mpq_info__version(char *program_name) {

	/* show the version. */
	NOTICE("%s (mopaq) %s (libmpq %s)\n", program_name, VERSION, libmpq__version());
	NOTICE("Written by %s\n", AUTHOR);
	NOTICE("\n");
	NOTICE("This is free software; see the source for copying conditions.  There is NO\n");
	NOTICE("warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");

	/* if no error was found, return zero. */
	return 0;
}

/* this function shows some archive information. */
int mpq_info__archive_info(char *program_name, char *mpq_filename, unsigned int number, unsigned int count) {

	/* some common variables. */
	int result;
	off_t packed_size    = 0;
	off_t unpacked_size  = 0;
	off_t offset         = 0;
	unsigned int version = 0;
	unsigned int files   = 0;
	mpq_archive_s *mpq_archive;

	/* open the mpq-archive. */
	if ((result = libmpq__archive_open(&mpq_archive, mpq_filename, -1)) < 0) {

		/* open archive failed. */
		NOTICE("archive number:			%i/%i\n", number, count);
		NOTICE("archive name:			%s\n", mpq_filename);
		NOTICE("archive type:			no mpq archive\n");

	} else {

		/* fetch some required information. */
		libmpq__archive_version(mpq_archive, &version);
		libmpq__archive_offset(mpq_archive, &offset);
		libmpq__archive_files(mpq_archive, &files);
		libmpq__archive_packed_size(mpq_archive, &packed_size);
		libmpq__archive_unpacked_size(mpq_archive, &unpacked_size);

		/* open archive was successful, show information. */
		NOTICE("archive number:			%i/%i\n", number, count);
		NOTICE("archive name:			%s\n", mpq_filename);
		NOTICE("archive version:		%i\n", version);
		NOTICE("archive offset:			%li\n", offset);
		NOTICE("archive files:			%i\n", files);
		NOTICE("archive packed size:		%li\n", packed_size);
		NOTICE("archive unpacked size:		%li\n", unpacked_size);
		NOTICE("archive compression ratio:	%.2f\n", (100 - ((float)packed_size / (float)unpacked_size * 100)));

		libmpq__archive_close(mpq_archive);
	}

	/* if multiple archives were given, continue with next one. */
	if (number < count) {
		NOTICE("\n-- next archive --\n\n");
	}

	/* if no error was found, return zero. */
	return 0;
}

/* the main function starts here. */
int main(int argc, char **argv) {

	/* common variables for the command line. */
	int opt;
	int option_index = 0;
	static char const short_options[] = "hv";
	static struct option const long_options[] = {
		{"help",	no_argument,	0,	'h'},
		{"version",	no_argument,	0,	'v'},
		{0,		0,		0,	0}
	};
	optind = 0;
	opterr = 0;

	/* some common variables. */
	char *program_name;
	char mpq_filename[PATH_MAX];
	unsigned int count;
	unsigned int number;

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

	/* parse command line. */
	while ((opt = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {

		/* check if all command line options are parsed. */
		if (opt == -1) {
			break;
		}

		/* parse option. */
		switch (opt) {
			case 'h':
				mpq_info__usage(program_name);
				exit(0);
			case 'v':
				mpq_info__version(program_name);
				exit(0);
			default:

				/* show some info on how to get help. :) */
				ERROR("%s: unrecognized option `%s'\n", program_name, argv[optind - 1]);
				ERROR("Try `%s --help' for more information.\n", program_name);

				/* exit with error. */
				exit(1);
		}
	}

	/* fill option structure with long option arguments */
	count = argc - optind;
	number = 1;

	libmpq__init ();

	/* create the file count from the command line arguments. */
	do {
		strncpy(mpq_filename, argv[optind], PATH_MAX);
		mpq_info__archive_info(program_name, mpq_filename, number, count);
		number++;
	} while (++optind < argc);

	libmpq__shutdown ();

	/* execution was successful. */
	exit(0);
}
