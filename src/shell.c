#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "defs.h"

// main() opens a shell mode
// every time the user enters, read the buffer (reallocate if needed), and execute it

#define SHELL_PROMPT "\x1b[0;1;35mbfsh> \x1b[0m"
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

int shell(void) {
    char buf[SHELL_BUF_SZ];

    // loop
    while (1) {
        // write prompt
        write(STDOUT_FILENO, SHELL_PROMPT, sizeof(SHELL_PROMPT));

        // read user input into buffer
        //scanf("%s", buf);
        fgets(buf, sizeof(buf), stdin);

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
        else if (!strcmp(buf, "clear"    )) printf("\x1b[H\x1b[2J\x1b[J");
        else if (!strcmp(buf, "help"     )) shell_help();

        interpreter(buf, strlen(buf));
        putchar('\n');
        memset(buf, '\0', SHELL_BUF_SZ);
    }

    return 0;
}
