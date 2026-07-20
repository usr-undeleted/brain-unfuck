#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include "defs.h"

// main() opens a shell mode
// every time the user enters, read the buffer (reallocate if needed), and execute it

#define SHELL_COLOR  "\x1b[0;1;35m"
#define ERROR_COLOR  "\x1b[0;1;31m"
#define SHELL_PROMPT "bfsh> \x1b[0m"
#define SHELL_BUF_SZ 8192

void shell_help(void) {
    // dont add newline at the end
    printf(
        "brainfuck shell basics:\n"
        "- commands:\n"
        "\texit:  exit the program. You may specify a second argument to return that specific number.\n"
        "\thelp:  show this help message.\n"
        "\tclear: clear the terminal.\n\n"

        "- brainfuck alphabet: " BF_ALPHABET
    );
}

void handle_sigint(int d) {
    (void)d;
    putchar('\n');
}

int shell(void) {
    // ctrl+c handler
    struct sigaction s;
    s.sa_handler = handle_sigint;
    if (sigaction(SIGINT, &s, NULL) == -1) {
        fprintf(stderr, "Failed to setup signal handler: %s\n", strerror(errno));
        return ERR_CODE;
    }

    char buf[SHELL_BUF_SZ];

    // loop
    int loop_ret = 0;
    while (1) {
        // write prompt with error
        if (loop_ret) {
            printf(ERROR_COLOR "[%d] " SHELL_COLOR SHELL_PROMPT, loop_ret);
        } else {
            write(STDOUT_FILENO,
                SHELL_COLOR SHELL_PROMPT,
                sizeof(SHELL_COLOR SHELL_PROMPT));
        }
        loop_ret = 0;

        // read user input into buffer
        if (!fgets(buf, sizeof(buf), stdin)) continue;

        // form_buf() does buffer formation too, trough a fd.
        // i need to separate the digest part from it, making a separate function
        // to use here...
        // maybe make it take one const buf to copy from, and another to copy to

        if (!strncmp(buf, "exit", 4) &&
            (isspace(buf[4]) || buf[4] == '\0')) {
                // parse second argument
                char *p = buf;
                int ret = strtol(&buf[5], &p, 0);

                // if the number provided isn't perfect, strtol
                // doesn't move 'p' to the null terminator or whitespace
                if (*p != '\0' && !isspace(*p)) {
                    fprintf(stderr, "bfsh: imperfect parsing, stopped at \"%.*s\".\n", (int)strlen(p) - 1, p);
                }

                return ret;
        }
        else if (!strncmp(buf, "clear", 5) &&
            (isspace(buf[5]) || buf[5] == '\0')) {
            printf("\x1b[H\x1b[2J\x1b[J");
        }

        else if (!strncmp(buf, "help", 4) &&
            (isspace(buf[4]) || buf[4] == '\0')) {
                // help command
                shell_help();
        }

        else if (!buf_has_bf(buf)) {
            // invalid command
            // remove space
            char *space = strchr(buf, ' ');
            if (space) {
                *space = '\0';
            } else {
                // if that failed, remove just the newline
                space = strchr(buf, '\n');
                if (space) *space = '\0';
            }

            fprintf(stderr, "bfsh: no such built-in \"%s\".",  buf);
            loop_ret = ERR_USER;
        }

        interpreter(buf, strlen(buf));
        putchar('\n');
        memset(buf, '\0', SHELL_BUF_SZ);
    }

    return 0;
}
