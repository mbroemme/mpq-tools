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

static bool
mpq_info__archive_info(const mpq_cli_context_s *context, size_t number, size_t count, bool separate)
{
    libmpq__off_t size_packed = 0;
    libmpq__off_t size_unpacked = 0;
    libmpq__off_t offset;
    uint32_t version;
    uint32_t files;
    mpq_archive_s *archive;

    if (!mpq_cli__library_ok(
            context, "open archive", libmpq__archive_open(&archive, context->archive, -1)
        )) {
        return false;
    }

    bool success =
        mpq_cli__library_ok(
            context, "obtain archive version", libmpq__archive_version(archive, &version)
        ) &&
        mpq_cli__library_ok(
            context, "obtain archive offset", libmpq__archive_offset(archive, &offset)
        ) &&
        mpq_cli__library_ok(
            context, "obtain archive member count", libmpq__archive_files(archive, &files)
        ) &&
        mpq_cli__library_ok(
            context, "obtain archive packed total",
            libmpq__archive_size_packed(archive, &size_packed)
        ) &&
        mpq_cli__library_ok(
            context, "obtain archive unpacked total",
            libmpq__archive_size_unpacked(archive, &size_unpacked)
        );
    int32_t close_status = libmpq__archive_close(archive);
    if (!success || !mpq_cli__library_ok(context, "close archive", close_status)) {
        return false;
    }

    if (separate) {
        NOTICE("\n-- next archive --\n\n");
    }
    NOTICE("archive number:\t\t\t%zu/%zu\n", number, count);
    NOTICE("archive name:\t\t\t%s\n", context->archive);
    NOTICE("archive version:\t\t%" PRIu32 "\n", version);
    NOTICE("archive offset:\t\t\t%" PRId64 "\n", offset);
    NOTICE("archive files:\t\t\t%" PRIu32 "\n", files);
    NOTICE("archive packed size:\t\t%" PRId64 "\n", size_packed);
    NOTICE("archive unpacked size:\t\t%" PRId64 "\n", size_unpacked);
    NOTICE(
        "archive compression ratio:\t%.2f\n",
        (100 - ((float)size_packed / (float)size_unpacked * 100))
    );
    return true;
}

enum mpq_cli_status
mpq_info__run(const char *program, const mpq_cli_request_s *request)
{
    enum mpq_cli_status result = MPQ_CLI_SUCCESS;
    bool printed = false;
    for (size_t i = 0; i < request->archive_count; i++) {
        const char *archive = request->archives[i];
        mpq_cli_context_s context = { program, request->command, archive, NULL };
        if (request->verbose) {
            fprintf(stderr, "%s info: '%s': reading archive information\n", program, archive);
        }
        if (mpq_info__archive_info(&context, i + 1, request->archive_count, printed)) {
            printed = true;
        } else {
            result = MPQ_CLI_FAILURE;
        }
    }
    return result;
}

static bool
mpq_info__list_member(
    const mpq_cli_context_s *context, mpq_archive_s *archive, uint32_t count, bool full,
    bool separate
)
{
    libmpq__off_t size_packed;
    libmpq__off_t size_unpacked;
    uint32_t flags;
    char filename[MPQ_GENERATED_FILENAME_SIZE];
    uint32_t index = *context->index;

    if (!mpq_cli__library_ok(
            context, "obtain member packed size",
            libmpq__file_size_packed(archive, index, &size_packed)
        ) ||
        !mpq_cli__library_ok(
            context, "obtain member unpacked size",
            libmpq__file_size_unpacked(archive, index, &size_unpacked)
        ) ||
        !mpq_cli__library_ok(
            context, "obtain member flags", libmpq__file_flags(archive, index, &flags)
        )) {
        return false;
    }
    if (!mpq_extract__file_name(index, filename, sizeof(filename))) {
        mpq_cli__error(context, "cannot generate member filename");
        return false;
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
    return true;
}

static bool
mpq_info__list_members(
    mpq_cli_context_s context, const mpq_cli_request_s *request, mpq_archive_s *archive,
    uint32_t count
)
{
    bool full = request->index_count == 0;
    if (full) {
        NOTICE("number   ucmp. size   cmp. size   ratio   cmp   imp   enc   filename\n");
        NOTICE("------   ----------   ---------   -----   ---   ---   ---   --------\n");
    }

    size_t selections = full ? count : request->index_count;
    for (size_t i = 0; i < selections; i++) {
        uint32_t index = full ? (uint32_t)i : request->indexes[i];
        context.index = &index;
        if (index >= count) {
            mpq_cli__error(&context, "out of range");
            return false;
        }
        if (request->verbose && !full) {
            fprintf(
                stderr, "%s list: '%s': listing member %" PRIu32 "\n", context.program,
                context.archive, index
            );
        }
        if (!mpq_info__list_member(&context, archive, count, full, i != 0)) {
            return false;
        }
    }
    return true;
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
    mpq_cli_context_s context = { program, request->command, path, NULL };
    mpq_archive_s *archive;
    if (!mpq_cli__library_ok(&context, "open archive", libmpq__archive_open(&archive, path, -1))) {
        return MPQ_CLI_FAILURE;
    }

    uint32_t count;
    libmpq__off_t size_packed = 0;
    libmpq__off_t size_unpacked = 0;
    bool success =
        mpq_cli__library_ok(
            &context, "obtain archive member count", libmpq__archive_files(archive, &count)
        ) &&
        mpq_info__list_members(context, request, archive, count);
    if (success && full) {
        success = mpq_cli__library_ok(
                      &context, "obtain archive packed total",
                      libmpq__archive_size_packed(archive, &size_packed)
                  ) &&
                  mpq_cli__library_ok(
                      &context, "obtain archive unpacked total",
                      libmpq__archive_size_unpacked(archive, &size_unpacked)
                  );
    }
    int32_t close_status = libmpq__archive_close(archive);
    if (!success || !mpq_cli__library_ok(&context, "close archive", close_status)) {
        return MPQ_CLI_FAILURE;
    }

    if (full) {
        NOTICE("------   ----------   ---------   -----   ---   ---   ---   --------\n");
        NOTICE(
            "  %4" PRIu32 "   %10" PRId64 "   %9" PRId64 " %6.0f%%   %s\n", count, size_packed,
            size_unpacked, (100 - fabs(((float)size_packed / (float)size_unpacked * 100))), path
        );
    }
    return MPQ_CLI_SUCCESS;
}
