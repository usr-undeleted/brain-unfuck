#include <sys/termios.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <limits.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include "os_defs.h"
#include "defs.h"

uint8_t flag_help = 0;
uint8_t flag_ver  = 0;
uint8_t flag_raw  = 0;
uint8_t flag_echo = 1;
uint8_t is_stdin  = 0;

#ifdef DEBUG
#include <signal.h>
char **d_argv;
int    d_argc;
int d_arg_loc;
int d_arg_cnt;
char  *d_file;

// debug dump
void debug_end(int d) {
    (void)d;

    write(STDOUT_FILENO, "\x1b[0;1;33;40m", sizeof("\x1b[0;1;33;40m"));

    fprintf(stderr,
        "\ninfo dump:\n"
        "- arguments:\n"
        "\targ_loc:   %d\n"
        "\targ_count: %d\n"
        "\targc:      %d\n"
        "\targv: { ",
        d_arg_loc, d_arg_cnt, d_argc
    );
    // print all args inside curly brackets
    for (int i = 0; i < d_argc; i++) {
        fprintf(stderr, "%s%s", d_argv[i], i != d_argc - 1 ? ", " : "");
    }
    fprintf(stderr, " }\n");

    // flags
    fprintf(stderr,
        "- flags:\n"
        "\tstdin: %d\n"
        "\thelp:  %d\n"
        "\techo:  %d\n"
        "\tver:   %d\n"
        "\traw:   %d\n",
        is_stdin,
        flag_help,
        flag_echo,
        flag_ver,
        flag_raw
    );

    write(STDOUT_FILENO, "\x1b[0m", sizeof("\x1b[0m"));

    exit(ERR_DEBUG);
}

#endif

// applies terminal options back
struct termios backup = {0};
void handle_end(void) {
    tcsetattr(STDERR_FILENO, TCSAFLUSH, &backup);
}

int main (const volatile int argc, const char *argv[]) {
    #ifdef DEBUG
    // set signals
    struct sigaction s;
    s.sa_handler = debug_end;
    if (sigaction(SIGINT, &s, NULL) == -1 || sigaction(SIGTERM, &s, NULL)) {
        fprintf(stderr, "Couldn't set signal for debugging: %s\n", strerror(errno));
        DEBUG_ERR_LOC;
        return ERR_DEBUG;
    }

    #endif
    // check stdin
    if (!isatty(STDIN_FILENO)) is_stdin = 1;

    #ifdef DEBUG
    d_argv = (char **)argv;
    d_argc = argc;
    #endif

    if (argc < 2 && !is_stdin) {
        usage(argv[0], "Not enough arguments");
        DEBUG_ERR_LOC;
        return 1;
    }

    // form flags
    flag_failure flag_val = manage_flags(argc, argv);

    // validate flags
    if (flag_val.argv_idx) {
        if (flag_val.char_idx) {
            // chars
            usage(argv[0], NULL);
            fprintf(stderr, "\nError: Unknown flag %s -> %c!\n",
                argv[flag_val.argv_idx], argv[flag_val.argv_idx][flag_val.char_idx]);
            DEBUG_ERR_LOC;
            return ERR_USER;

        } else {
            // single string
            usage(argv[0], NULL);
            fprintf(stderr, "\nError: Unknown flag %s!\n",
                argv[flag_val.argv_idx]);
            DEBUG_ERR_LOC;
            return ERR_USER;
        }
    }

    // consume flags
    if (flag_help) {
        usage(argv[0], NULL);
        return 0;
    }

    if (flag_ver) {
        fprintf(stderr, VERSION);
        return 0;
    }

    // validate invocation
    int arg_count = 0; // number of file names provided
    int arg_loc   = 0; // argument with file name

    // find arg_loc and set arg_count
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') continue;
        else {
            arg_count++;
            arg_loc = i;
        }
    }

    #ifdef DEBUG
    d_arg_cnt = arg_count;
    d_arg_loc = arg_loc;
    #endif

    if (arg_loc == 0 && !is_stdin) {
        usage(argv[0], "No file provided");
        DEBUG_ERR_LOC;

        return ERR_USER;
    }

    if (arg_count > (1 - is_stdin)) {
        is_stdin ?
            // stdin
            usage(argv[0], "Too many files provided (stdin was provided)")
        :
            // regular
            usage(argv[0], "Too many files provided ");
        DEBUG_ERR_LOC;
        return ERR_USER;
    }

    int             fd = STDIN_FILENO;
    uint64_t m_aligned = 0;
    uint64_t copy_idx  = 0;
    char        *file  = NULL; // keep null for stdin
    if (!is_stdin) {
        // no stdin branch
        // open file descriptor
        if ((fd = open(argv[arg_loc], O_RDONLY)) == -1) {
            fprintf(stderr, "Failed to open file '%s': %s\n", argv[arg_loc], strerror(errno));
            DEBUG_ERR_LOC;
            return ERR_USER;
        }

        // get file stats
        struct stat st;
        if (fstat(fd, &st) == -1) {
            fprintf(stderr, "Failed to get stats on file '%s': %s\n", argv[arg_loc], strerror(errno));
            DEBUG_ERR_LOC;
            return ERR_CODE;
        }

        // form page alignment for munmap() and mmap()
        int64_t page_size = sysconf(_SC_PAGESIZE);
        if (page_size == -1) {
            fprintf(stderr, "Failed to obtain system page size: %s\n", strerror(errno));
            DEBUG_ERR_LOC;
            return ERR_CODE;
        }
        m_aligned = (st.st_size + page_size - 1) & ~(page_size - 1); // oooo fancy bitwise!

        // make a mmap() of file, copy the contents over, then make
        // the file more digest-able for the parser (removing comments)
        if ((file = mmap(0, m_aligned, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)) == MAP_FAILED) {
            fprintf(stderr, "Failed to allocate memory for file '%s': %s\n", argv[arg_loc], strerror(errno));
            DEBUG_ERR_LOC;
            return ERR_CODE;
        }
    }

    #ifdef DEBUG
    d_file = file;
    #endif

    // make buffer with no comments
    // deals with stdin automatically
    char *stdin_b;
    if (digest_buf(file, fd, &copy_idx, &stdin_b)) {
        // pos value = failure
        fprintf(stderr, "Failed to digest buffer: %s\n", strerror(errno));
        DEBUG_ERR_LOC;
        return ERR_CODE;
    }
    if (is_stdin) file = stdin_b;

    if (!buf_has_bf(file)) {
        fprintf(stderr, "Provided file has no valid brainfuck code.\n");
        DEBUG_ERR_LOC;
        return ERR_USER;
    }

    if (is_stdin && strchr(file, ',')) {
        fprintf(stderr, "User input cannot be used when stdin is provided (remove commas from code).\n");
        DEBUG_ERR_LOC;
        return ERR_USER;
    }

    #ifdef DEBUG
    printf("\x1b[0;1;33;40m%s\x1b[0m\n", file);
    #endif

    validation_ret val = validate_buffer(file);
    if (val.err) {
        fprintf(stderr, "Can't execute (line %ld): ", val.line);
        switch (val.err) {
            case ERR_BUF_TOO_MANY_CLOSE: {
                fprintf(stderr, "Too many closing brackets.");
                DEBUG_ERR_LOC;
                break;
            }

            case ERR_BUF_UNCLOSED: {
                fprintf(stderr, "Too many opening brackets.");
                DEBUG_ERR_LOC;
                break;
            }

            case ERR_BUF_CLOSE_BEYOND_LIMIT: {
                fprintf(stderr, "Input file has more loops than can be handled. Sorry!");
                DEBUG_ERR_LOC;
                break;
            }
        }
        putchar('\n');

        return ERR_USER;
    }

    #ifdef USE_WCHAR
    // set locale for unicode printing
    if (setlocale(LC_ALL, "") == NULL) {
        fprintf(stderr, "Failed to set locale for unicode printing.\n");
        DEBUG_ERR_LOC;
        return ERR_CODE;
    }
    #endif

    // incase the user specified terminal-changing options
    if (flag_raw || !flag_echo) {
        // set up new terminal options
        struct termios new_term = {0};
        // populate backup and set backup
        if (tcgetattr(STDERR_FILENO, &backup) == -1) {
            fprintf(stderr, "Failed to get old terminal definitions: %s\n", strerror(errno));
            DEBUG_ERR_LOC;
            return ERR_CODE;
        }

        if (atexit(handle_end) != 0) {
            fprintf(stderr, "Failed to set up backup function:\n");
            DEBUG_ERR_LOC;
            return ERR_CODE;
        }

        // populate new options
        new_term = backup;
        if (flag_raw) {
            new_term.c_lflag    &= ~(ICANON); // set to raw
            new_term.c_cc[VMIN]  = 1;         // get one char at a time
            new_term.c_cc[VTIME] = 0;         // remove all delay
        }
        if (!flag_echo) {
            new_term.c_lflag    &= ~(ECHO);   // clear echoding
        }
        if (tcsetattr(STDERR_FILENO, TCSAFLUSH, &new_term) == -1) {
            fprintf(stderr, "Failed to set new terminal definitions: %s\n", strerror(errno));
            DEBUG_ERR_LOC;
            return ERR_CODE;
        }
    }

    interpreter(file, copy_idx);

    if (!is_stdin) {
        // finish up
        // close mmap() allocated file
        if (munmap(file, m_aligned) == -1) {
            fprintf(stderr, "Failed to close mapped memory: %s\n", strerror(errno));
            DEBUG_ERR_LOC;
            return ERR_CODE;
        }

        // close file descriptor
        if (close(fd) == -1) {
            fprintf(stderr, "Failed to close file '%s': %s\n", argv[arg_loc], strerror(errno));
            DEBUG_ERR_LOC;
            return ERR_CODE;
        }
    } else {
        free(file);
    }

    return SUCCESS;
}
