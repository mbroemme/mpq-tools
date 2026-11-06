/*
 *  mpq-info.c -- archive information and member listing.
 *
 *  Copyright (c) 2003-2007 Maik Broemme <mbroemme@plusserver.de>
 *  Member listing copyright (c) 2003-2008 Maik Broemme <mbroemme@plusserver.de>
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

#include "config.h"
#include "mpq-cli.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>

#include <libmpq/mpq.h>

#define NOTICE(...) printf(__VA_ARGS__);

static int32_t
mpq_info__archive_info(const char *mpq_filename, size_t number, size_t count)
{
    libmpq__off_t size_packed = 0;
    libmpq__off_t size_unpacked = 0;
    libmpq__off_t offset;
    uint32_t version;
    uint32_t files;
    mpq_archive_s *mpq_archive;

    NOTICE("archive number:			%zu/%zu\n", number, count);
    NOTICE("archive name:			%s\n", mpq_filename);
    int32_t result = libmpq__archive_open(&mpq_archive, mpq_filename, -1);
    if (result != 0) {
        NOTICE("archive type:			no mpq archive\n");
        return result;
    }

    if ((result = libmpq__archive_version(mpq_archive, &version)) == 0 &&
        (result = libmpq__archive_offset(mpq_archive, &offset)) == 0 &&
        (result = libmpq__archive_files(mpq_archive, &files)) == 0 &&
        (result = libmpq__archive_size_packed(mpq_archive, &size_packed)) == 0 &&
        (result = libmpq__archive_size_unpacked(mpq_archive, &size_unpacked)) == 0) {
        NOTICE("archive version:		%" PRIu32 "\n", version);
        NOTICE("archive offset:			%" PRId64 "\n", offset);
        NOTICE("archive files:			%" PRIu32 "\n", files);
        NOTICE("archive packed size:		%" PRId64 "\n", size_packed);
        NOTICE("archive unpacked size:		%" PRId64 "\n", size_unpacked);
        NOTICE(
            "archive compression ratio:	%.2f\n",
            (100 - ((float)size_packed / (float)size_unpacked * 100))
        );
    }

    int32_t close_result = libmpq__archive_close(mpq_archive);
    return result != 0 ? result : close_result;
}

static void
mpq_info__library_error(
    const char *program, const char *command, const char *archive, const uint32_t *index,
    int32_t status
)
{
    const char *message = libmpq__strerror(status);
    fprintf(stderr, "%s %s: '%s': ", program, command, archive);
    if (index != NULL) {
        fprintf(stderr, "member %" PRIu32 ": ", *index);
    }
    fprintf(
        stderr, "%s (%" PRId32 ")\n", message != NULL ? message : "unknown libmpq error", status
    );
}

enum mpq_cli_status
mpq_info__run(const char *program, const mpq_cli_request_s *request)
{
    enum mpq_cli_status result = MPQ_CLI_SUCCESS;
    for (size_t i = 0; i < request->archive_count; i++) {
        const char *archive = request->archives[i];
        if (request->verbose) {
            fprintf(stderr, "%s info: '%s': reading archive information\n", program, archive);
        }
        int32_t status = mpq_info__archive_info(archive, i + 1, request->archive_count);
        if (status != 0) {
            mpq_info__library_error(program, "info", archive, NULL, status);
            result = MPQ_CLI_FAILURE;
        }
        if (i + 1 < request->archive_count) {
            NOTICE("\n-- next archive --\n\n");
        }
    }
    return result;
}

static int32_t
mpq_info__list_member(
    mpq_archive_s *archive, uint32_t index, uint32_t count, bool full, bool separate
)
{
    libmpq__off_t size_packed;
    libmpq__off_t size_unpacked;
    uint32_t flags;
    char filename[MPQ_GENERATED_FILENAME_SIZE];
    int32_t result;

    if ((result = libmpq__file_size_packed(archive, index, &size_packed)) != 0 ||
        (result = libmpq__file_size_unpacked(archive, index, &size_unpacked)) != 0 ||
        (result = libmpq__file_flags(archive, index, &flags)) != 0 ||
        (result = mpq_extract__file_name(index, filename, sizeof(filename))) != 0) {
        return result;
    }

    double ratio = 100 - fabs((float)size_packed / (float)size_unpacked * 100);
    const char *compressed = (flags & LIBMPQ_FILE_FLAG_COMPRESS) ? "yes" : "no";
    const char *imploded = (flags & LIBMPQ_FILE_FLAG_IMPLODE) ? "yes" : "no";
    const char *encrypted = (flags & LIBMPQ_FILE_FLAG_ENCRYPTED) ? "yes" : "no";

    if (full) {
        NOTICE(
            "  %4" PRIu32 "   %10" PRId64 "   %9" PRId64 " %6.0f%%   %3s   %3s   %3s   %s\n", index,
            size_packed, size_unpacked, ratio, compressed, imploded, encrypted, filename
        );
    } else {
        if (separate) {
            NOTICE("\n");
        }
        NOTICE("file number:			%" PRIu32 "/%" PRIu32 "\n", index, count);
        NOTICE("file packed size:		%" PRId64 "\n", size_packed);
        NOTICE("file unpacked size:		%" PRId64 "\n", size_unpacked);
        NOTICE("file compression ratio:		%.2f%%\n", ratio);
        NOTICE("file compressed:		%s\n", compressed);
        NOTICE("file imploded:			%s\n", imploded);
        NOTICE("file encrypted:			%s\n", encrypted);
        NOTICE("file name:			%s\n", filename);
    }
    return 0;
}

static enum mpq_cli_status
mpq_info__list_members(
    const char *program, const mpq_cli_request_s *request, mpq_archive_s *archive, uint32_t count
)
{
    const char *path = request->archives[0];
    bool full = request->index_count == 0;
    if (full) {
        NOTICE("number   ucmp. size   cmp. size   ratio   cmp   imp   enc   filename\n");
        NOTICE("------   ----------   ---------   -----   ---   ---   ---   --------\n");
    }

    size_t selections = full ? count : request->index_count;
    for (size_t i = 0; i < selections; i++) {
        uint32_t index = full ? (uint32_t)i : request->indexes[i];
        if (index >= count) {
            fprintf(
                stderr, "%s list: '%s': member %" PRIu32 " out of range\n", program, path, index
            );
            return MPQ_CLI_FAILURE;
        }
        if (request->verbose && !full) {
            fprintf(stderr, "%s list: '%s': listing member %" PRIu32 "\n", program, path, index);
        }
        int32_t status = mpq_info__list_member(archive, index, count, full, i != 0);
        if (status != 0) {
            mpq_info__library_error(program, "list", path, &index, status);
            return MPQ_CLI_FAILURE;
        }
    }
    return MPQ_CLI_SUCCESS;
}

enum mpq_cli_status
mpq_list__run(const char *program, const mpq_cli_request_s *request)
{
    const char *path = request->archives[0];
    bool full = request->index_count == 0;
    if (request->verbose) {
        fprintf(
            stderr, "%s list: '%s': listing %s members\n", program, path, full ? "all" : "selected"
        );
    }
    mpq_archive_s *archive;
    int32_t status = libmpq__archive_open(&archive, path, -1);
    if (status != 0) {
        mpq_info__library_error(program, "list", path, NULL, status);
        return MPQ_CLI_FAILURE;
    }

    uint32_t count;
    status = libmpq__archive_files(archive, &count);
    enum mpq_cli_status result = MPQ_CLI_FAILURE;
    if (status == 0) {
        result = mpq_info__list_members(program, request, archive, count);
    }

    libmpq__off_t size_packed = 0;
    libmpq__off_t size_unpacked = 0;
    if (result == MPQ_CLI_SUCCESS && full &&
        ((status = libmpq__archive_size_packed(archive, &size_packed)) != 0 ||
         (status = libmpq__archive_size_unpacked(archive, &size_unpacked)) != 0)) {
        result = MPQ_CLI_FAILURE;
    }

    int32_t close_status = libmpq__archive_close(archive);
    if (result == MPQ_CLI_SUCCESS) {
        status = close_status;
    }
    if (status != 0) {
        mpq_info__library_error(program, "list", path, NULL, status);
        return MPQ_CLI_FAILURE;
    }

    if (result == MPQ_CLI_SUCCESS && full) {
        NOTICE("------   ----------   ---------   -----   ---   ---   ---   --------\n");
        NOTICE(
            "  %4" PRIu32 "   %10" PRId64 "   %9" PRId64 " %6.0f%%   %s\n", count, size_packed,
            size_unpacked, (100 - fabs(((float)size_packed / (float)size_unpacked * 100))), path
        );
    }
    return result;
}
