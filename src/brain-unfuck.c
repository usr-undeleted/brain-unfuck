#include <sys/mman.h>
#include <sys/stat.h>
#include <limits.h>
#include <string.h>
#include <locale.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <wchar.h>
#include "defs.h"

// to be added:
// 1: stdin support

uint8_t flag_help = 0;
uint8_t flag_ver  = 0;

int main (const volatile int argc, const char *argv[]) {
    if (argc < 2) {
        usage(argv[0], "Not enough arguments");
        return 1;
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

    if (arg_loc == 0) {
        fprintf(stderr, "No file provided.\n");
        return ERR_USER;
    }

    if (arg_count > 1) {
        fprintf(stderr, "Too many files provided.\n");
        return ERR_USER;
    }

    // form flags
    flag_failure flag_val = manage_flags(argc, argv);

    // validate flags
    if (flag_val.argv_idx) {
        if (flag_val.char_idx) {
            // chars
            usage(argv[0], NULL);
            fprintf(stderr, "\nError: Unknown flag (%s -> %c)!\n",
                argv[flag_val.argv_idx], argv[flag_val.argv_idx][flag_val.char_idx]);
            return ERR_USER;

        } else {
            // single string
            usage(argv[0], NULL);
            fprintf(stderr, "\nError: Unknown flag (%s)!\n",
                argv[flag_val.argv_idx]);
            return ERR_USER;
        }
    }

    // consume flags
    // you'll have to read the comments in the manage_flags()
    // function to understand why we have it like this
    if (flag_ver) {
        usage(argv[0], NULL);
        return 0;
    }

    if (flag_help) {
        fprintf(stderr, VERSION);
        return 0;
    }

    // open file descriptor
    int fd;
    if ((fd = open(argv[arg_loc], O_RDONLY)) == -1) {
        fprintf(stderr, "Failed to open file '%s': %s\n", argv[arg_loc], strerror(errno));
        return ERR_USER;
    }

    // get file stats
    struct stat st;
    if (fstat(fd, &st) == -1) {
        fprintf(stderr, "Failed to get stats on file '%s': %s\n", argv[arg_loc], strerror(errno));
        return ERR_CODE;
    }

    // form page alignment for munmap() and mmap()
    int64_t page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) {
        fprintf(stderr, "Failed to obtain system page size: %s\n", strerror(errno));
        return ERR_CODE;
    }
    uint64_t m_aligned = (st.st_size + page_size - 1) & ~(page_size - 1); // oooo fancy bitwise!

    char *file;
    if ((file = mmap(0, m_aligned, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, 0)) == NULL) {
        fprintf(stderr, "Failed to map file '%s': %s\n", argv[arg_loc], strerror(errno));
        return ERR_CODE;
    }

    if (file == MAP_FAILED || m_aligned == 0 || !buf_has_bf(file)) {
        fprintf(stderr, "Provided file has no valid brainfuck code.\n");
        return ERR_USER;
    }

    validation_ret val = validate_buffer(file);
    if (val.err) {
        fprintf(stderr, "Can't execute (line %ld): ", val.line);
        switch (val.err) {
            case ERR_BUF_TOO_MANY_CLOSE: {
                fprintf(stderr, "Too many closing brackets.");
                break;
            }

            case ERR_BUF_UNCLOSED: {
                fprintf(stderr, "Too many opening brackets.");
                break;
            }

            case ERR_BUF_CLOSE_BEYOND_LIMIT: {
                fprintf(stderr, "Input file has more loops than can be handled. Sorry!");
                break;
            }
        }
        putchar('\n');

        return ERR_USER;
    }

    // set locale for unicode printing
    if (setlocale(LC_ALL, "") == NULL) {
        fprintf(stderr, "Failed to set locale for unicode printing.\n");
        return ERR_CODE;
    }

    // store pointers to loops on a stack
    uint16_t loop_idx                  = 0;     // how deep we are on loops
    char    *loop_stack[LOOP_STACK_SZ] = {0};   // loop pointers

    wchar_t  array[BF_ARRAY_SZ]        = {0};   // main loop
    wchar_t *array_ptr                 = array; // user pointer
    char    *file_ptr                  = file;  // the file pointer

    // main loop
    while (1) {
        skip_whitespace(&file_ptr);

        // i guess this is needed..?
        if (file_ptr > (file + st.st_size) || file_ptr < file) break;

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
                putwchar(*array_ptr);
                break;
            }

            case ',': {
                // read user input
                *array_ptr = getwchar();
                break;
            }

            case '[': {
                // store loop
                loop_stack[loop_idx++] = file_ptr;
                break;
            }

            case ']': {
                // end loop

                if (
                    // define when to end loop
                    #ifdef END_LOOP_ON_NEG
                    *array_ptr > 0
                    #else
                    *array_ptr != 0
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
            array_ptr = &array[(sizeof(array) / sizeof(array[0]))];
        }

        if (array_ptr > &array[(sizeof(array) / sizeof(array[0]))]) {
            // pointer is beyond, move to start
            array_ptr = array;
        }

        file_ptr++;
    }

    // finish up
    // close mmap() allocated file
    if (munmap(file, m_aligned) == -1) {
        fprintf(stderr, "Failed to close mapped memory: %s\n", strerror(errno));
        return ERR_CODE;
    }

    // close file descriptor
    if (close(fd) == -1) {
        fprintf(stderr, "Failed to close file '%s': %s\n", argv[arg_loc], strerror(errno));
        return ERR_CODE;
    }

    return 0;
}
