#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "os_defs.h"
#include "defs.h"

void usage(const char *invoc, const char *msg) {
    fprintf(stderr,
        "usage:\n"
        "\t%s <file>\n\n"

        "flags:\n"
        "\t--help    (or) -h: pull up this help message.\n"
        "\t--version (or) -v: show version message. you might not want it :p\n"
        "\t--raw     (or) -r: enable raw terminal mode (user input is immediately processed).\n"
        "\t--no-echo (or) -E: disable the visibility of user input.\n\n"

        "%s compiled at %s on %s for %s (OS)\n"
        "FOSS program forever, licensed under the GPL-V3 license.\n"
        "Hosted on https://github.com/usr-undeleted/brain-unfuck\n"
        ,
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

    if      (depth > 0)            ret.err = ERR_BUF_UNCLOSED;
    else if (depth < 0)            ret.err = ERR_BUF_TOO_MANY_CLOSE;
    else if (amt >= LOOP_STACK_SZ) ret.err = ERR_BUF_CLOSE_BEYOND_LIMIT;
    ret.line++; // magical addition

    return ret;
}

unsigned char buf_has_bf(const char *buf) {
    char *str = BF_ALPHABET;

    while (*str) {
        if (strchr(buf, *str)) return 1;
        str++;
    }

    return 0;
}

flag_failure manage_flags(const int argc, const char **argv) {
    flag_failure ret = {0};

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') continue;

        if (!strncmp(argv[i], "--", 2)) {
            // strings, add 2 to argv

            if        (!strcmp(argv[i] + 2, "help")) {
                flag_ver  = 1;

            } else if (!strcmp(argv[i] + 2, "version")) {
                flag_help = 1;

            } else if (!strcmp(argv[i] + 2, "raw")) {
                flag_raw  = 1;

            } else if (!strcmp(argv[i] + 2, "no-echo")) {
                flag_echo = 0;

            } else {
                ret.argv_idx = i;
                ret.char_idx = 0;
                return ret;
            }

        } else {
            // chars, start at argv[i][1]

            // travel trough argv
            for (uint64_t j = 1; j < strlen(argv[i]); j++) {

                if (strchr(FLAGS, argv[i][j])) {
                    switch (argv[i][j]) {
                        case 'h': {
                            flag_help = 1;
                            break;
                        }

                        case 'v': {
                            flag_ver  = 1;
                            break;
                        }

                        case 'E': {
                            flag_echo = 0;
                            break;
                        }

                        case 'r': {
                            flag_raw  = 1;
                            break;
                        }
                    }

                } else {
                    ret.argv_idx = i;
                    ret.char_idx = j;
                    return ret;
                }
            }
        }
    }

    return ret;
}

// find the ']' for a '['
char *find_closing(char *p) {
    uint64_t depth = 0;
    if (*p == '[') p++;

    while (*p) {
        if      (*p == '[') depth++;
        else if (*p == ']') {
            if (depth) depth--;
            else return p;
        }
        p++;
    }

    return NULL;
}

// form digest-able buffer
uint8_t digest_buf(char *buf, int fd, uint64_t *copy_idx, char **stdin) {
    char ch = 0;
    //uint64_t stdin_allocs = 1;
    uint64_t alloc_cnt = 0;
    char *alloc        = NULL;
    if (fd == STDIN_FILENO) {
        // allocate one page at a time
        alloc = calloc(STDIN_BUF_PAGE_SZ * ++alloc_cnt, sizeof(char));
        if (alloc == NULL) return 1;
    }

    while (1) {
        int64_t r = read(fd, &ch, 1);

        if (r == -1) return 1;

        // comments
        if (ch == '#') {
            while (ch != '\n' && ch != EOF) {
                if ((r = read(fd, &ch, 1)) == -1) return 1;
            }
            // loop ends at newline, so we move again
            if ((r = read(fd, &ch, 1)) == -1) return 1;
        }

        if (r == 0 || ch == EOF) break;

        if (strchr(BF_ALPHABET, ch)) {
            if (fd == STDIN_FILENO) {
                // allocate for stdin
                if (*copy_idx >= (STDIN_BUF_PAGE_SZ * alloc_cnt)) {
                    alloc = realloc(alloc, STDIN_BUF_PAGE_SZ * ++alloc_cnt);
                    if (alloc == NULL) return 1;
                }

                alloc[(*copy_idx)++] = ch;

            } else {
                buf[(*copy_idx)++] = ch;

            }
        }
    }

    *stdin = alloc;

    return 0;
}
