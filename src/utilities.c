#include "defs.h"
#include "os_defs.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

void usage(const char *invoc, const char *msg) {
    fprintf(stderr,
        "usage:\n"
        "\t%s <file>\n\n"

        "options:\n"
        "\t%s --help (or) -h: pull up this help message.\n\n"

        "%s compiled at %s on %s for %s (OS)\n"
        "FOSS program forever, licensed under the GPL-V3 license.\n"
        "Hosted on https://github.com/usr-undeleted/brain-unfuck\n"
        ,
        invoc,

        invoc,

            // time of compilation, CC version, OS
        invoc, __TIMESTAMP__, __VERSION__, OS
    );

    // print error if specified
    if (msg) {
        fprintf(stderr, "\nError: %s\n", msg);
    }
}

// puts pointer on the character right after a whitespace
void skip_whitespace(char **ptr) {
    while (isspace(**ptr) || !strchr(BF_ALPHABET, **ptr)) (*ptr)++;
}
