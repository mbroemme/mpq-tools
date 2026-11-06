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

/* define new print functions for error. */
#define ERROR(...) fprintf(stderr, __VA_ARGS__);

/* define new print functions for notification. */
#define NOTICE(...) printf(__VA_ARGS__);

#define EXTRACT_BUFFER_SIZE (64u * 1024u)

#ifdef _WIN32
#define TEMP_FILENAME_TEMPLATE ".mpq-extract-00000000000000000000000000000000"
#else
#define TEMP_FILENAME_TEMPLATE ".mpq-extract-XXXXXX"
#endif

/* Local failures use positive statuses, distinct from libmpq errors. */
enum
{
    OUTPUT_OPEN_FAILED = 1,
    OUTPUT_IO_FAILED
};

/* Generate a name until listfile support is available. */
int32_t
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

typedef struct mpq_extract_context
{
    const char *program;
    const char *archive;
    uint32_t index;
} mpq_extract_context_s;

static void
mpq_extract__error_prefix(const mpq_extract_context_s *context)
{
    fprintf(
        stderr, "%s extract: '%s': member %" PRIu32 ": ", context->program, context->archive,
        context->index
    );
}

static void
mpq_extract__output_error(
    const mpq_extract_context_s *context, const char *filename, const char *operation, int error
)
{
    mpq_extract__error_prefix(context);
    ERROR(
        "'%s': cannot %s output: %s\n", filename, operation,
        error != 0 ? strerror(error) : "I/O error"
    );
}

static void
mpq_extract__discard_temp(const mpq_extract_context_s *context, const char *temp_name)
{
#ifdef _WIN32
    int result = _unlink(temp_name);
#else
    int result = unlink(temp_name);
#endif
    if (result != 0) {
        mpq_extract__output_error(context, temp_name, "remove temporary", errno);
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
mpq_extract__open_temp(const mpq_extract_context_s *context, char *temp_name, const char *filename)
{
    errno = 0;
#ifdef _WIN32
    int fd = mpq_extract__create_temp(temp_name);
#else
    int fd = mkstemp(temp_name);
#endif
    if (fd < 0) {
        mpq_extract__output_error(context, filename, "create temporary", errno);
        return NULL;
    }

    errno = 0;
#ifdef _WIN32
    FILE *fp = _fdopen(fd, "wb");
#else
    FILE *fp = fdopen(fd, "wb");
#endif
    if (fp == NULL) {
        mpq_extract__output_error(context, filename, "open temporary", errno);
#ifdef _WIN32
        int close_result = _close(fd);
#else
        int close_result = close(fd);
#endif
        if (close_result != 0) {
            mpq_extract__output_error(context, temp_name, "close temporary", errno);
        }
        mpq_extract__discard_temp(context, temp_name);
    }
    return fp;
}

static int32_t
mpq_extract__publish(
    const mpq_extract_context_s *context, const char *temp_name, const char *filename
)
{
#ifdef _WIN32
    if (!MoveFileExA(temp_name, filename, MOVEFILE_REPLACE_EXISTING)) {
        unsigned long error = GetLastError();
        mpq_extract__error_prefix(context);
        ERROR("'%s': cannot publish output: Windows error %lu\n", filename, error);
        return OUTPUT_IO_FAILED;
    }
#else
    errno = 0;
    if (rename(temp_name, filename) != 0) {
        mpq_extract__output_error(context, filename, "publish", errno);
        return OUTPUT_IO_FAILED;
    }
#endif
    return 0;
}

static int32_t
mpq_extract__copy_stream(
    const mpq_extract_context_s *context, mpq_stream_s *stream, FILE *fp, const char *filename,
    uint8_t *buffer, libmpq__off_t size
)
{
    const libmpq__off_t request = EXTRACT_BUFFER_SIZE;
    libmpq__off_t total = 0;
    for (;;) {
        libmpq__off_t transferred = -1;
        int32_t result = libmpq__stream_read(stream, buffer, request, &transferred);
        if (result != 0) {
            return result;
        }
        if (transferred < 0 || transferred > request || transferred > size - total) {
            return LIBMPQ_ERROR_SIZE;
        }
        if (transferred == 0) {
            return total == size ? 0 : LIBMPQ_ERROR_SIZE;
        }

        size_t bytes = (size_t)transferred;
        errno = 0;
        if (fwrite(buffer, 1, bytes, fp) != bytes) {
            mpq_extract__output_error(context, filename, "write temporary", errno);
            return OUTPUT_IO_FAILED;
        }
        total += transferred;
    }
}

/* Extract one member to its generated filename. */
static int32_t
mpq_extract__extract_file(
    const mpq_extract_context_s *context, mpq_archive_s *mpq_archive, uint8_t *buffer, bool quiet
)
{
    char filename[MPQ_GENERATED_FILENAME_SIZE];
    int32_t result = mpq_extract__file_name(context->index, filename, sizeof(filename));
    if (result != 0) {
        return result;
    }

    mpq_stream_s *stream;
    result = libmpq__stream_open(mpq_archive, context->index, &stream);
    if (result != 0) {
        return result;
    }

    char temp_name[] = TEMP_FILENAME_TEMPLATE;
    FILE *fp = NULL;
    libmpq__off_t size = -1;
    result = libmpq__stream_size(stream, &size);
    if (result == 0) {
        if (size < 0) {
            result = LIBMPQ_ERROR_SIZE;
        } else {
            fp = mpq_extract__open_temp(context, temp_name, filename);
            result = fp != NULL
                         ? mpq_extract__copy_stream(context, stream, fp, filename, buffer, size)
                         : OUTPUT_OPEN_FAILED;
        }
    }

    int32_t stream_close_result = libmpq__stream_close(stream);
    if (result == 0) {
        result = stream_close_result;
    }
    if (fp == NULL) {
        return result;
    }

    errno = 0;
    int close_result = fclose(fp);
    if (close_result != 0 && result == 0) {
        mpq_extract__output_error(context, filename, "close temporary", errno);
        result = OUTPUT_IO_FAILED;
    }

    if (result == 0) {
        result = mpq_extract__publish(context, temp_name, filename);
    }
    if (result == 0) {
        if (!quiet) {
            NOTICE("extracting %s\n", filename);
        }
    } else {
        mpq_extract__discard_temp(context, temp_name);
    }
    return result;
}

static void
mpq_extract__library_error(const char *program, const char *path, int32_t status)
{
    const char *message = libmpq__strerror(status);
    fprintf(
        stderr, "%s extract: '%s': %s (%" PRId32 ")\n", program, path,
        message != NULL ? message : "unknown libmpq error", status
    );
}

static enum mpq_cli_status
mpq_extract__members(
    const char *program, const mpq_cli_request_s *request, mpq_archive_s *archive, uint32_t count
)
{
    const char *path = request->archives[0];
    bool all = request->index_count == 0;
    uint8_t *buffer = NULL;
    enum mpq_cli_status result = MPQ_CLI_SUCCESS;
    size_t selections = all ? count : request->index_count;
    for (size_t i = 0; i < selections; i++) {
        uint32_t index = all ? (uint32_t)i : request->indexes[i];
        mpq_extract_context_s context = { program, path, index };
        if (index >= count) {
            mpq_extract__error_prefix(&context);
            fprintf(stderr, "out of range\n");
            result = MPQ_CLI_FAILURE;
            break;
        }
        if (buffer == NULL) {
            const size_t capacity = EXTRACT_BUFFER_SIZE;
            buffer = malloc(capacity);
            if (buffer == NULL) {
                mpq_extract__library_error(program, path, LIBMPQ_ERROR_MALLOC);
                result = MPQ_CLI_FAILURE;
                break;
            }
        }
        if (request->verbose && !all) {
            fprintf(
                stderr, "%s extract: '%s': extracting member %" PRIu32 "\n", program, path, index
            );
        }
        int32_t status = mpq_extract__extract_file(&context, archive, buffer, request->quiet);
        if (status != 0) {
            if (status < 0) {
                const char *message = libmpq__strerror(status);
                mpq_extract__error_prefix(&context);
                fprintf(
                    stderr, "%s (%" PRId32 ")\n",
                    message != NULL ? message : "unknown libmpq error", status
                );
            }
            result = MPQ_CLI_FAILURE;
            break;
        }
    }
    free(buffer);
    return result;
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
    mpq_archive_s *archive;
    int32_t status = libmpq__archive_open(&archive, path, -1);
    if (status != 0) {
        mpq_extract__library_error(program, path, status);
        return MPQ_CLI_FAILURE;
    }

    uint32_t count;
    status = libmpq__archive_files(archive, &count);
    enum mpq_cli_status result = MPQ_CLI_FAILURE;
    if (status == 0) {
        result = mpq_extract__members(program, request, archive, count);
    }
    int32_t close_status = libmpq__archive_close(archive);
    if (result == MPQ_CLI_SUCCESS) {
        status = close_status;
    }
    if (status != 0) {
        mpq_extract__library_error(program, path, status);
        return MPQ_CLI_FAILURE;
    }
    return result;
}
