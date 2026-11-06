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

#include "config.h"
#include "mpq-cli.h"

#include <stdio.h>

#include <libmpq/mpq.h>

static const char *
mpq_main__program_name(const char *path)
{
    if (path == NULL) {
        return "mpq";
    }

    const char *name = path;
    for (; *path != '\0'; path++) {
        if (*path == '/'
#ifdef _WIN32
            || *path == '\\'
#endif
        ) {
            name = path + 1;
        }
    }
    return *name != '\0' ? name : "mpq";
}

static void
mpq_main__help(const char *program)
{
    printf(
        "Usage: %s [GLOBAL OPTIONS] COMMAND [COMMAND OPTIONS] [--] ARGUMENTS...\n"
        "\n"
        "Commands:\n"
        "  info             Display archive information\n"
        "  list             List archive members\n"
        "  extract          Extract archive members\n"
        "\n"
        "Global options:\n"
        "  -h, --help       Display this help\n"
        "  -v, --version    Display version information\n"
        "  -q, --quiet      Suppress successful extraction notices\n"
        "      --verbose    Display additional diagnostic information\n"
        "\n"
        "Run '%s COMMAND --help' for command-specific help.\n",
        program, program
    );
}

static void
mpq_main__command_help(const char *program, enum mpq_cli_command command)
{
    const char *name = mpq_cli__command_name(command);
    if (command == MPQ_CLI_INFO) {
        printf(
            "Usage: %s info [--] ARCHIVE [ARCHIVE ...]\n"
            "Display information for each archive in operand order.\n",
            program
        );
    } else {
        printf(
            "Usage: %s %s [-i INDEX | --index INDEX | --index=INDEX]... [--] ARCHIVE\n"
            "%s all members by default.\n",
            program, name, command == MPQ_CLI_LIST ? "List" : "Extract"
        );
    }

    printf(
        "\n"
        "  -h, --help       Display this help when used alone after the command\n"
        "      --           Treat all following tokens as archive paths\n"
    );
    if (command != MPQ_CLI_INFO) {
        printf(
            "  -i, --index INDEX\n"
            "                   Select a zero-based member index. Repeat to select\n"
            "                   multiple indexes in the given order.\n"
            "\n"
            "Example: %s %s -i 0 --index=7 archive.mpq\n",
            program, name
        );
    } else {
        printf(
            "\n"
            "Example: %s info first.mpq second.mpq\n",
            program
        );
    }
}

static void
mpq_main__version(const char *program)
{
    printf(
        "%s %s (libmpq %s)\n"
        "Written by Maik Broemme <mbroemme@libmpq.org>\n"
        "\n"
        "This is free software; see the source for copying conditions.  There is NO\n"
        "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n",
        program, VERSION, libmpq__version()
    );
}

static enum mpq_cli_status
mpq_main__unavailable(const char *program, const mpq_cli_request_s *request)
{
    fprintf(
        stderr, "%s %s: operation is not yet available in this development build\n", program,
        mpq_cli__command_name(request->command)
    );
    return MPQ_CLI_FAILURE;
}

static enum mpq_cli_status
mpq_main__dispatch(const char *program, const mpq_cli_request_s *request)
{
    static const mpq_cli_handler handlers[] = { [MPQ_CLI_INFO] = mpq_main__unavailable,
                                                [MPQ_CLI_LIST] = mpq_main__unavailable,
                                                [MPQ_CLI_EXTRACT] = mpq_main__unavailable };
    return handlers[request->command](program, request);
}

int
main(int argc, char **argv)
{
    const char *program = mpq_main__program_name(argc > 0 ? argv[0] : NULL);
    mpq_cli_request_s request;
    enum mpq_cli_status result = mpq_cli__parse(program, argc, argv, &request);
    if (result == MPQ_CLI_SUCCESS) {
        if (request.action == MPQ_CLI_HELP) {
            if (request.command == MPQ_CLI_NO_COMMAND) {
                mpq_main__help(program);
            } else {
                mpq_main__command_help(program, request.command);
            }
        } else if (request.action == MPQ_CLI_VERSION) {
            mpq_main__version(program);
        } else {
            result = mpq_main__dispatch(program, &request);
        }

        if (fflush(stdout) == EOF || ferror(stdout)) {
            if (result == MPQ_CLI_SUCCESS) {
                fprintf(stderr, "%s: cannot write standard output\n", program);
                result = MPQ_CLI_FAILURE;
            }
        }
    }
    mpq_cli__clear(&request);
    return result;
}
