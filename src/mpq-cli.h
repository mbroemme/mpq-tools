/*
 * Copyright (c) 2026 Maik Broemme <mbroemme@libmpq.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#ifndef MPQ_CLI_H
#define MPQ_CLI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum mpq_cli_command
{
    MPQ_CLI_NO_COMMAND,
    MPQ_CLI_INFO,
    MPQ_CLI_LIST,
    MPQ_CLI_EXTRACT
};

enum mpq_cli_action
{
    MPQ_CLI_RUN,
    MPQ_CLI_HELP,
    MPQ_CLI_VERSION
};

enum mpq_cli_status
{
    MPQ_CLI_SUCCESS = 0,
    MPQ_CLI_FAILURE = 1,
    MPQ_CLI_USAGE = 2
};

typedef struct mpq_cli_request
{
    enum mpq_cli_command command;
    enum mpq_cli_action action;
    bool quiet;
    bool verbose;
    char *const *archives;
    size_t archive_count;
    uint32_t *indexes;
    size_t index_count;
} mpq_cli_request_s;

typedef enum mpq_cli_status (*mpq_cli_handler)(
    const char *program, const mpq_cli_request_s *request
);

const char *mpq_cli__command_name(enum mpq_cli_command command);

/* Parse into a fresh or cleared request. Archive paths borrow argv storage. */
enum mpq_cli_status
mpq_cli__parse(const char *program, int argc, char *const argv[], mpq_cli_request_s *request);

void mpq_cli__clear(mpq_cli_request_s *request);

#endif
