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
uint64_t skip_whitespace(char **ptr) {
    uint64_t ret = 0;
    while (isspace(**ptr) || !strchr(BF_ALPHABET, **ptr)) {
        // comments
        if (**ptr == '#') *ptr = strchr(*ptr, '\n');

        // count lines
        if (**ptr == '\n') ret++;

        // increment
        (*ptr)++;
    }
    return ret;
}



// make sure loops close
validation_ret validate_buffer(char *p) {
    validation_ret ret = {0}; // returned
    uint64_t      line = 0;   // what line pointer is in
    uint16_t       amt = 0;   // how many openers
    int64_t      depth = 0;   // how deep are we

    while (*p) {
        // add lines to counter
        line += skip_whitespace(&p);

        switch (*p) {
            case '[': {
                amt++;
                depth++;
                ret.line = line;
                break;
            }

            case ']': {
                depth--;
                ret.line = line;
                break;
            }
        }

        p++;
    }

    if      (depth > 0)           ret.err = ERR_BUF_UNCLOSED;
    else if (depth < 0)           ret.err = ERR_BUF_TOO_MANY_CLOSE;
    else if (amt > LOOP_STACK_SZ) ret.err = ERR_BUF_CLOSE_BEYOND_LIMIT;
    ret.line++; // magical addition

    return ret;
}
