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
#include "mpq-cli.h"

#if defined(_WIN32) && !defined(_CRT_RAND_S)
#define _CRT_RAND_S
#endif

/* generic includes. */
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#ifndef NOGDI
#define NOGDI
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

/* libmpq includes. */
#include <libmpq/mpq.h>

#define EXTRACT_BUFFER_SIZE (64u * 1024u)

#ifdef _WIN32
#define TEMP_FILENAME_TEMPLATE ".mpq-extract-00000000000000000000000000000000"
#else
#define TEMP_FILENAME_TEMPLATE ".mpq-extract-XXXXXX"
#endif

/* Generate a name until listfile support is available. */
bool
mpq_extract__file_name(uint32_t file_number, char *filename, size_t filename_size)
{
    int result = snprintf(filename, filename_size, "file%06" PRIu32 ".xxx", file_number);
    return result >= 0 && (size_t)result < filename_size;
}

static void
mpq_extract__output_error(
    const mpq_cli_context_s *context, const char *filename, const char *operation, int error
)
{
    mpq_cli__error_prefix(context);
    fprintf(
        stderr, "cannot %s '%s': %s\n", operation, filename,
        error != 0 ? strerror(error) : "I/O error"
    );
}

static void
mpq_extract__discard_temp(const mpq_cli_context_s *context, const char *temp_name)
{
    errno = 0;
#ifdef _WIN32
    int result = _unlink(temp_name);
#else
    int result = unlink(temp_name);
#endif
    if (result != 0) {
        mpq_extract__output_error(context, temp_name, "remove temporary output", errno);
    }
}

#ifdef _WIN32
static int
mpq_extract__create_temp(char *temp_name)
{
    for (unsigned int attempt = 0; attempt < 16; attempt++) {
        unsigned int random[4];
        for (size_t i = 0; i < 4; i++) {
            int result = rand_s(&random[i]);
            if (result != 0) {
                errno = result;
                return -1;
            }
        }

        snprintf(
            temp_name, sizeof(TEMP_FILENAME_TEMPLATE), ".mpq-extract-%08x%08x%08x%08x", random[0],
            random[1], random[2], random[3]
        );
        int fd = _open(temp_name, _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY, _S_IREAD | _S_IWRITE);
        if (fd >= 0 || errno != EEXIST) {
            return fd;
        }
    }
    return -1;
}
#endif

static FILE *
mpq_extract__open_temp(const mpq_cli_context_s *context, char *temp_name, const char *filename)
{
    errno = 0;
#ifdef _WIN32
    int fd = mpq_extract__create_temp(temp_name);
#else
    int fd = mkstemp(temp_name);
#endif
    if (fd < 0) {
        mpq_extract__output_error(context, filename, "create temporary output for", errno);
        return NULL;
    }

    errno = 0;
#ifdef _WIN32
    FILE *fp = _fdopen(fd, "wb");
#else
    FILE *fp = fdopen(fd, "wb");
#endif
    if (fp == NULL) {
        mpq_extract__output_error(context, temp_name, "attach temporary output stream to", errno);
        errno = 0;
#ifdef _WIN32
        int close_result = _close(fd);
#else
        int close_result = close(fd);
#endif
        if (close_result != 0) {
            mpq_extract__output_error(
                context, temp_name, "close temporary output descriptor", errno
            );
        }
        mpq_extract__discard_temp(context, temp_name);
    }
    return fp;
}

static bool
mpq_extract__publish(const mpq_cli_context_s *context, const char *temp_name, const char *filename)
{
#ifdef _WIN32
    if (!MoveFileExA(temp_name, filename, MOVEFILE_REPLACE_EXISTING)) {
        DWORD error = GetLastError();
        mpq_cli__error_prefix(context);
        fprintf(
            stderr, "cannot publish output '%s': Windows error %lu\n", filename,
            (unsigned long)error
        );
        return false;
    }
#else
    errno = 0;
    if (rename(temp_name, filename) != 0) {
        mpq_extract__output_error(context, filename, "publish output", errno);
        return false;
    }
#endif
    return true;
}

static bool
mpq_extract__copy_stream(
    const mpq_cli_context_s *context, mpq_stream_s *stream, FILE *fp, const char *temp_name,
    uint8_t *buffer, libmpq__off_t size
)
{
    const libmpq__off_t request = EXTRACT_BUFFER_SIZE;
    libmpq__off_t total = 0;
    for (;;) {
        libmpq__off_t transferred = -1;
        if (!mpq_cli__library_ok(
                context, "read member stream",
                libmpq__stream_read(stream, buffer, request, &transferred)
            )) {
            return false;
        }
        if (transferred < 0 || transferred > request || transferred > size - total) {
            mpq_cli__error(context, "invalid member stream transfer count");
            return false;
        }
        if (transferred == 0) {
            if (total != size) {
                mpq_cli__error(context, "premature end of member stream");
                return false;
            }
            return true;
        }

        size_t bytes = (size_t)transferred;
        errno = 0;
        if (fwrite(buffer, 1, bytes, fp) != bytes) {
            mpq_extract__output_error(context, temp_name, "write temporary output", errno);
            return false;
        }
        total += transferred;
    }
}

/* Extract one member to its generated filename. */
static bool
mpq_extract__extract_file(
    const mpq_cli_context_s *context, mpq_archive_s *archive, uint8_t *buffer, bool quiet
)
{
    char filename[MPQ_GENERATED_FILENAME_SIZE];
    if (!mpq_extract__file_name(*context->index, filename, sizeof(filename))) {
        mpq_cli__error(context, "cannot generate member filename");
        return false;
    }

    mpq_stream_s *stream;
    if (!mpq_cli__library_ok(
            context, "open member stream", libmpq__stream_open(archive, *context->index, &stream)
        )) {
        return false;
    }

    char temp_name[] = TEMP_FILENAME_TEMPLATE;
    FILE *fp = NULL;
    libmpq__off_t size = -1;
    bool success = mpq_cli__library_ok(
        context, "obtain member stream size", libmpq__stream_size(stream, &size)
    );
    if (success) {
        if (size < 0) {
            mpq_cli__error(context, "negative member stream size");
            success = false;
        } else {
            fp = mpq_extract__open_temp(context, temp_name, filename);
            success = fp != NULL &&
                      mpq_extract__copy_stream(context, stream, fp, temp_name, buffer, size);
        }
    }

    int32_t stream_close_status = libmpq__stream_close(stream);
    success = success && mpq_cli__library_ok(context, "close member stream", stream_close_status);
    if (fp == NULL) {
        return false;
    }

    errno = 0;
    int close_result = fclose(fp);
    if (close_result != 0 && success) {
        mpq_extract__output_error(context, temp_name, "close temporary output", errno);
        success = false;
    }

    success = success && mpq_extract__publish(context, temp_name, filename);
    if (!success) {
        mpq_extract__discard_temp(context, temp_name);
    } else if (!quiet) {
        printf("extracting %s\n", filename);
    }
    return success;
}

static bool
mpq_extract__members(
    mpq_cli_context_s context, const mpq_cli_request_s *request, mpq_archive_s *archive,
    uint32_t count
)
{
    bool all = request->index_count == 0;
    uint8_t *buffer = NULL;
    bool success = true;
    size_t selections = all ? count : request->index_count;
    for (size_t i = 0; i < selections; i++) {
        uint32_t index = all ? (uint32_t)i : request->indexes[i];
        context.index = &index;
        if (index >= count) {
            mpq_cli__error(&context, "out of range");
            success = false;
            break;
        }
        if (buffer == NULL) {
            const size_t capacity = EXTRACT_BUFFER_SIZE;
            buffer = malloc(capacity);
            if (buffer == NULL) {
                context.index = NULL;
                mpq_cli__error(&context, "cannot allocate extraction buffer");
                success = false;
                break;
            }
        }
        if (request->verbose && !all) {
            fprintf(
                stderr, "%s extract: '%s': extracting member %" PRIu32 "\n", context.program,
                context.archive, index
            );
        }
        if (!mpq_extract__extract_file(&context, archive, buffer, request->quiet)) {
            success = false;
            break;
        }
    }
    free(buffer);
    return success;
}

enum mpq_cli_status
mpq_extract__run(const char *program, const mpq_cli_request_s *request)
{
    const char *path = request->archives[0];
    if (request->verbose) {
        fprintf(
            stderr, "%s extract: '%s': extracting %s members\n", program, path,
            request->index_count == 0 ? "all" : "selected"
        );
    }
    mpq_cli_context_s context = { program, request->command, path, NULL };
    mpq_archive_s *archive;
    if (!mpq_cli__library_ok(&context, "open archive", libmpq__archive_open(&archive, path, -1))) {
        return MPQ_CLI_FAILURE;
    }

    uint32_t count;
    bool success =
        mpq_cli__library_ok(
            &context, "obtain archive member count", libmpq__archive_files(archive, &count)
        ) &&
        mpq_extract__members(context, request, archive, count);
    int32_t close_status = libmpq__archive_close(archive);
    if (!success || !mpq_cli__library_ok(&context, "close archive", close_status)) {
        return MPQ_CLI_FAILURE;
    }
    return MPQ_CLI_SUCCESS;
}
