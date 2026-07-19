#ifndef DEFS_H
#define DEFS_H

#include <stdint.h>

// definitions follow https://esolangs.org/wiki/Brainfuck#Conventions, mostly

// memory should be 524288 (2 << 18) wchar_t cells, forming a 2mb buffer (2097152 bytes).
// sign-type and size are up to <wchar.h>
// the user pointer should wrap on the array (aka, moving left on start moves pointer to end, or the opposite)
// input is line-buffered
// the new line character MUST be 0xA, no matter the OS. whenever more OSs are supported, make sure to achieve this.

// extensions (when they are added) will be:
// 1. '*': return the value of cell
// 2.

// flags
#define FLAG_HELP 1 << 0
#define FLAG_STD

// brainfuck alphabet
#define BF_ALPHABET "<>+-[],."
// '<>':        move pointer
// '[]':        set loop (while(current_cell != 0)). must validate to find closing
// '.':         print current cell
// ',':         read user input (single char)
// '+' and '-': read user input

// strive for 2mb or 4mb
#define BF_ARRAY_SZ    2 << 18
// loop pointers need to be stored somewhere, so we make a "stack"
// this will store 8192 entries (65kb)
#define LOOP_STACK_SZ  2 << 12

void     usage          (const char *invoc, const char *msg);
uint64_t skip_whitespace(char **ptr);

// buffer validation
#define ERR_BUF_UNCLOSED           1 << 0
#define ERR_BUF_TOO_MANY_CLOSE     1 << 1
#define ERR_BUF_CLOSE_BEYOND_LIMIT 1 << 2

// returned struct to diagnose error
typedef struct {
    // error code, keep at 0 for success
    unsigned char err;
    // line where theres a syntax fail
    uint64_t     line;
} validation_ret;

validation_ret validate_buffer(char *p);

#define SUCCESS  0
// mistake made by code
#define ERR_CODE 1
// mistake made by user
#define ERR_USER 2

#endif
