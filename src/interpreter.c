#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "os_defs.h"
#include "defs.h"

void interpreter(const char *file, const uint64_t copy_idx) {
    // get a random number for '?'
    #ifdef EXTENSIONS
    uint64_t r_extra; // undefined on purpose
    // lovely little function call
    srand((time(NULL) * (uint64_t)file) / copy_idx );
    #endif

    // store pointers to loops on a stack
    uint16_t loop_idx                  = 0;     // how deep we are on loops
    char    *loop_stack[LOOP_STACK_SZ] = {0};   // loop pointers

    array_t  array[BF_ARRAY_SZ]        = {0};   // cells
    array_t *array_ptr                 = array; // user pointer
    char    *file_ptr                  = (char *)file;  // the file pointer

    // make a FILE pointer for the default file descriptor, 1, aka, stdout
    // if extensions are enabled, '=' can change it
    FILE *output_stream = stdout;

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
                if (*array_ptr == 10) {
                    write(STDOUT_FILENO, PLATFORM_NL, sizeof(PLATFORM_NL));
                } else {
                    PUTC(*array_ptr, output_stream);
                }
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
                exit(*array_ptr);
                break;
            }

            case '=': {
                // change the output file stream to cell
                switch (*array_ptr) {
                    case STDIN_FILENO:  output_stream = stdin;  break;
                    case STDOUT_FILENO: output_stream = stdout; break;
                    case STDERR_FILENO: output_stream = stderr; break;
                    default: {
                        fprintf(stderr,
                            "Refusing to operate further: file descriptor '%d' on cell '%ld' is invalid.\n",
                            *array_ptr, array_ptr - array);
                        DEBUG_ERR_LOC;
                        exit(ERR_USER);
                        break;
                    }
                }

                break;
            }

            case '~': {
                // simple bit flip
                *array_ptr = ~*array_ptr;
                break;
            }

            case '?': {
                // get a random number
                *array_ptr = rand();
                break;
            }

            case '@': {
                // sleep() wrapper
                sleep(*array_ptr);
                break;
            }

            case '&': {
                // usleep() wrapper
                usleep(*array_ptr);
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
        #else
        printf("\x1b[0;1;32;40m%c\x1b[0m", *array_ptr);
        #endif // wchar
        #endif // debug

        file_ptr++;
    }
}
