#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
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
        "\texit:  exit the program.\n"
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
        scanf("%s", buf);

        // form_buf() does buffer formation too, trough a fd.
        // i need to separate the digest part from it, making a separate function
        // to use here...
        // maybe make it take one const buf to copy from, and another to copy to

        if      (!strcmp(buf, "exit"))  exit(0);
        else if (!strcmp(buf, "clear")) printf("\x1b[H\x1b[2J\x1b[J");
        else if (!strcmp(buf, "help"))  shell_help();

        interpreter(buf, strlen(buf));
        putchar('\n');
        memset(buf, '\0', SHELL_BUF_SZ);
    }

    return 0;
}
