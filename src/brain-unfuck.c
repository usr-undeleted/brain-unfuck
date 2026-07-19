#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <wchar.h>
#include "defs.h"

// to be added:
// 1: stdin support
// 2: extensions (such as exiting the program)

int main (const volatile int argc, const char *argv[]) {
    if (argc < 2) {
        usage(argv[0], "Not enough arguments");
        return 1;
    }

    // validate invocation
    int arg_count = 0; // number of file names provided
    int arg_loc   = 0; // argument with file name

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (!strcmp(argv[i], "--help") || strchr(argv[i], 'h')) {

                // help msg
                usage(argv[0], NULL);
                return SUCCESS;

            } else {
                usage(argv[0], "Invalid flag");
                return ERR_USER;
            }

        } else {
            arg_count += 1;
            arg_loc    = i;
        }
    }

    int ret_code = 0;

    if (arg_count > 1) {
        usage(argv[0], "Too many arguments");
        return ERR_USER;
    }
    else if (argc < 1) {
        usage(argv[0], "No file provided");
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

    return ret_code;
}
