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

// extra for regular exiting
uint8_t atexit_is_caller = 0;
void debug_end(int d);
void atexit_debug(void) {
    atexit_is_caller = 1;
    debug_end(0);
}

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

    if (!atexit_is_caller) exit(3);
}

#endif

// applies terminal options back
struct termios backup = {0};
void handle_end(void) {
    tcsetattr(STDERR_FILENO, TCSAFLUSH, &backup);
}

int main (const volatile int argc, const char *argv[]) {
    #ifdef DEBUG
    // set atexit
    if (atexit(atexit_debug) != 0) {
        fprintf(stderr, "Couldn't set exiting for debugging.\n");
        DEBUG_ERR_LOC;
        return ERR_DEBUG;
    }

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
    char *stdin;
    if (digest_buf(file, fd, &copy_idx, &stdin)) {
        // pos value = failure
        fprintf(stderr, "Failed to digest buffer: %s\n", strerror(errno));
        DEBUG_ERR_LOC;
        return ERR_CODE;
    }
    if (is_stdin) file = stdin;

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

    // store pointers to loops on a stack
    uint16_t loop_idx                  = 0;     // how deep we are on loops
    char    *loop_stack[LOOP_STACK_SZ] = {0};   // loop pointers

    array_t  array[BF_ARRAY_SZ]        = {0};   // cells
    array_t *array_ptr                 = array; // user pointer
    char    *file_ptr                  = file;  // the file pointer

    // main loop
    while (1) {
        // i guess this is needed..?
        if (file_ptr > (file + copy_idx) || file_ptr < file) break;

        #ifdef DEBUG
        printf ("\x1b[0;1;34;40m%c\x1b[0m", *file_ptr);
        #endif // debug

        switch (*file_ptr) {
            case '+': {
                // increment
                (*array_ptr)++;
                break;
            }

            case '-': {
                // decrement
                (*array_ptr)--;
                break;
            }

            case '<': {
                // move pointer left
                array_ptr--;
                break;
            }

            case '>': {
                // move pointer right
                array_ptr++;
                break;
            }

            case '.': {
                // print cell
                PUTC(*array_ptr);
                break;
            }

            case ',': {
                // read user input
                *array_ptr = GETC();
                break;
            }

            case '[': {
                // store loop
                if (
                    #ifndef END_LOOP_ON_NEG
                    *array_ptr == 0
                    #else
                    *array_ptr < 0
                    #endif
                ) {
                    // skip when cell is zero
                    file_ptr = find_closing(file_ptr);

                } else {
                    loop_stack[loop_idx++] = file_ptr;
                }
                break;
            }

            case ']': {
                // end loop

                if (
                    // define when to end loop
                    #ifndef END_LOOP_ON_NEG
                    *array_ptr != 0
                    #else
                    *array_ptr > 0
                    #endif

                    ) {

                    file_ptr = loop_stack[loop_idx - 1];
                } else {
                    // cell is negative, keep going
                    loop_idx--;
                }

                break;
            }

            #ifdef EXTENSIONS

            case '*': {
                // return value of cell
                return *array_ptr;
                break;
            }

            #endif

            default: {
                break;
            }
        }

        // treat wrapping
        if (array_ptr < array) {
            // pointer is behind, move to end
            array_ptr = &array[(BF_ARRAY_SZ) - 1];
        }

        if (array_ptr >= &array[(sizeof(array) / sizeof(array[0]))]) {
            // pointer is beyond, move to start
            array_ptr = array;
        }

        #ifdef DEBUG
        #ifdef USE_WCHAR
        printf("\x1b[0;1;32;40m%lc\x1b[0m", *array_ptr);
        DEBUG_ERR_LOC;
        #else
        printf("\x1b[0;1;32;40m%c\x1b[0m", *array_ptr);
        DEBUG_ERR_LOC;
        #endif // wchar
        #endif // debug

        file_ptr++;
    }

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
