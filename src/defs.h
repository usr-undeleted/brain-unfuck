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

// brainfuck alphabet
// '<>':        move pointer
// '[]':        set loop (while(current_cell != 0)). must validate to find closing
// '.':         print current cell
// ',':         read user input (single char)
// '+' and '-': read user input
#ifdef EXTENSIONS
#include <time.h>
#define BF_ALPHABET "<>+-[],.*=~?@&"
// '*': return value of current cell, terminating program
// '=': switch the current file descriptor of output
// '~': flip bits on a cell
// '?': get a random value

#else
#define BF_ALPHABET "<>+-[],."

#endif

// strive for 2mb or 4mb
#define BF_ARRAY_SZ    2 << 18

// type for array
#ifndef USE_WCHAR
typedef uint8_t array_t;

#define GETC(v)    getchar()
#define PUTC(c, s) putc(c, s)

#else
#include <locale.h>
#include <wchar.h>
typedef wint_t array_t;

#define GETC(v) getwchar()
#define PUTC(x, s) fprintf(s, "%lc", x)
#endif

// find ']' for '['
char *find_closing(char *p);

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
unsigned char buf_has_bf(const char *buf);

#define SUCCESS   0
// mistake made by code
#define ERR_CODE  1
// mistake made by user
#define ERR_USER  2
// debugging
#define ERR_DEBUG 3

// file_name:N error
#ifdef DEBUG
// shows a message showing the source file and line
// ALWAYS put it right before returning/exiting
#define DEBUG_ERR_LOC fprintf(stderr, "\x1b[0;1;33;40mLocated at: %s:%d\x1b[0m\n", __FILE_NAME__, __LINE__)
#else
// ignore, only matters when DEBUG is set
// you should still ALWAYS put it right before returning/exiting
#define DEBUG_ERR_LOC do {} while(0)
#endif

// flag stuff
#define FLAGS         "hvrE"

// on success, set argv_idx to 0
// on unknown full string flag, keep char_idx at 0
// on unknown char, fill all fields
typedef struct {
    // idx of argv
    uint32_t argv_idx;
    // idx of char
    uint32_t char_idx;
} flag_failure;

// global flags
extern uint8_t flag_help; // help
extern uint8_t flag_echo; // wether to echo or not
extern uint8_t flag_ver;  // version
extern uint8_t flag_raw;  // raw term mode

// excludes argv[0] on its own, hand it the real argc and argv
flag_failure manage_flags(const int argc, const char **argv);

// digest buffer
#define STDIN_BUF_PAGE_SZ 1024
uint8_t digest_buf(char *buf, int fd, uint64_t *copy_idx, char **stdin);

// funny!
#define VERSION "I don't keep the versions of my programs, so uhhh, brain-unfuck 3000!!!\n"

// main function
void interpreter(const char *file, const uint64_t copy_idx);

#endif
