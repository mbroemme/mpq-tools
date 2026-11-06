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

#include "mpq-cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const command_names[] = { [MPQ_CLI_INFO] = "info",
                                             [MPQ_CLI_LIST] = "list",
                                             [MPQ_CLI_EXTRACT] = "extract" };

const char *
mpq_cli__command_name(enum mpq_cli_command command)
{
    return command_names[command];
}

static bool
mpq_cli__is_option(const char *token)
{
    return token[0] == '-' && token[1] != '\0';
}

static bool
mpq_cli__is_help(const char *token)
{
    return strcmp(token, "-h") == 0 || strcmp(token, "--help") == 0;
}

static bool
mpq_cli__is_version(const char *token)
{
    return strcmp(token, "-v") == 0 || strcmp(token, "--version") == 0;
}

static bool
mpq_cli__parse_index(const char *token, uint32_t *index)
{
    if (*token == '\0') {
        return false;
    }

    uint32_t value = 0;
    for (; *token != '\0'; token++) {
        if (*token < '0' || *token > '9') {
            return false;
        }
        uint32_t digit = (uint32_t)(*token - '0');
        if (value > (UINT32_MAX - digit) / 10u) {
            return false;
        }
        value = value * 10u + digit;
    }
    *index = value;
    return true;
}

static enum mpq_cli_status
mpq_cli__usage_error(
    const char *program, enum mpq_cli_command command, const char *message, const char *token
)
{
    const char *name = mpq_cli__command_name(command);
    const char *separator = name != NULL ? " " : "";
    if (name == NULL) {
        name = "";
    }
    fprintf(stderr, "%s%s%s: %s", program, separator, name, message);
    if (token != NULL) {
        fprintf(stderr, " '%s'", token);
    }
    fprintf(
        stderr,
        "\n"
        "Try '%s%s%s --help' for usage.\n",
        program, separator, name
    );
    return MPQ_CLI_USAGE;
}

static enum mpq_cli_status
mpq_cli__parse_command(
    const char *program, int argc, char *const argv[], int position, mpq_cli_request_s *parsed
)
{
    enum mpq_cli_command command = parsed->command;
    if (position == argc - 1 && mpq_cli__is_help(argv[position])) {
        parsed->action = MPQ_CLI_HELP;
        return MPQ_CLI_SUCCESS;
    }

    bool literal_operands = false;
    for (; position < argc && mpq_cli__is_option(argv[position]); position++) {
        const char *token = argv[position];
        if (strcmp(token, "--") == 0) {
            literal_operands = true;
            position++;
            break;
        }
        if (mpq_cli__is_help(token)) {
            return mpq_cli__usage_error(program, command, "help must stand alone", token);
        }

        int remaining = argc - position;
        const char *value;
        if (command != MPQ_CLI_INFO &&
            (strcmp(token, "-i") == 0 || strcmp(token, "--index") == 0)) {
            if (position == argc - 1 || strcmp(argv[position + 1], "--") == 0) {
                return mpq_cli__usage_error(program, command, "option requires an index", token);
            }
            value = argv[++position];
        } else if (command != MPQ_CLI_INFO && strncmp(token, "--index=", 8) == 0) {
            value = token + 8;
        } else {
            return mpq_cli__usage_error(program, command, "unknown command option", token);
        }

        uint32_t index;
        if (!mpq_cli__parse_index(value, &index)) {
            return mpq_cli__usage_error(program, command, "invalid index", value);
        }
        if (parsed->indexes == NULL) {
            if ((uintmax_t)remaining <= SIZE_MAX / sizeof(*parsed->indexes)) {
                parsed->indexes = malloc((size_t)remaining * sizeof(*parsed->indexes));
            }
            if (parsed->indexes == NULL) {
                fprintf(
                    stderr, "%s %s: cannot allocate index selection\n", program,
                    mpq_cli__command_name(command)
                );
                return MPQ_CLI_FAILURE;
            }
        }
        parsed->indexes[parsed->index_count++] = index;
    }

    if (!literal_operands) {
        for (int i = position; i < argc; i++) {
            if (mpq_cli__is_option(argv[i])) {
                return mpq_cli__usage_error(
                    program, command, "option after archive operand", argv[i]
                );
            }
        }
    }
    if (position == argc) {
        return mpq_cli__usage_error(program, command, "missing archive operand", NULL);
    }
    if (command != MPQ_CLI_INFO && position != argc - 1) {
        return mpq_cli__usage_error(
            program, command, "unexpected archive operand", argv[position + 1]
        );
    }
    parsed->archives = argv + position;
    parsed->archive_count = (size_t)(argc - position);
    return MPQ_CLI_SUCCESS;
}

enum mpq_cli_status
mpq_cli__parse(const char *program, int argc, char *const argv[], mpq_cli_request_s *request)
{
    *request = (mpq_cli_request_s){ 0 };
    if (argc == 2 && (mpq_cli__is_help(argv[1]) || mpq_cli__is_version(argv[1]))) {
        request->action = mpq_cli__is_help(argv[1]) ? MPQ_CLI_HELP : MPQ_CLI_VERSION;
        return MPQ_CLI_SUCCESS;
    }

    mpq_cli_request_s parsed = { 0 };
    int position = 1;
    for (; position < argc && mpq_cli__is_option(argv[position]); position++) {
        const char *token = argv[position];
        if (strcmp(token, "-q") == 0 || strcmp(token, "--quiet") == 0) {
            parsed.quiet = true;
        } else if (strcmp(token, "--verbose") == 0) {
            parsed.verbose = true;
        } else if (mpq_cli__is_help(token) || mpq_cli__is_version(token)) {
            return mpq_cli__usage_error(
                program, MPQ_CLI_NO_COMMAND, "option must stand alone", token
            );
        } else {
            return mpq_cli__usage_error(
                program, MPQ_CLI_NO_COMMAND, "unknown global option", token
            );
        }
    }

    if (parsed.quiet && parsed.verbose) {
        return mpq_cli__usage_error(
            program, MPQ_CLI_NO_COMMAND, "--quiet and --verbose cannot be combined", NULL
        );
    }
    if (position >= argc) {
        return mpq_cli__usage_error(program, MPQ_CLI_NO_COMMAND, "missing command", NULL);
    }
    for (enum mpq_cli_command command = MPQ_CLI_INFO; command <= MPQ_CLI_EXTRACT; command++) {
        if (strcmp(argv[position], mpq_cli__command_name(command)) == 0) {
            parsed.command = command;
            break;
        }
    }
    if (parsed.command == MPQ_CLI_NO_COMMAND) {
        return mpq_cli__usage_error(program, MPQ_CLI_NO_COMMAND, "unknown command", argv[position]);
    }

    enum mpq_cli_status result = mpq_cli__parse_command(program, argc, argv, position + 1, &parsed);
    if (result == MPQ_CLI_SUCCESS) {
        *request = parsed;
    } else {
        mpq_cli__clear(&parsed);
    }
    return result;
}

void
mpq_cli__clear(mpq_cli_request_s *request)
{
    free(request->indexes);
    *request = (mpq_cli_request_s){ 0 };
}
