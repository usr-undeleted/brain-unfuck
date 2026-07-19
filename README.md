# C brain-fuck interpreter
Thinking of the idea on a whim, I made this interpreter (finished to a working condition in under 24 hours!) because of, well, boredom, and wanting to learn more of C!  

This turned out to actually be fun, so I'll probably give this a bit more support, beyond just a basic interpreter. Note that I won't bother with porting to other systems (despite `os_defs.h`), but im 100% willingly to include code YOU make for porting!

(mostly) following `https://esolangs.org/wiki/Brainfuck#Conventions`, the interpreter follows these conventions:
1. '#'s will comment out the entire next line.
2. The array is 524288 cells big, each cell being of sizeof(wchar_t) (usually 4 bytes on unix-like). The signed type is entirely dependent on your system's wchar.h
3. The pointer will wrap around the array, meaning that going left at the start will lead you to the end, or going right at the end will leader you to the start.
4. Writing 10 to a cell and printing it will always lead to a new character.
5. Unicode characters may be written.

You may optionally want to implement some extensions I made by defining the macro `EXTENSIONS`, that provides the following new characters:  
`*`: Return the value of the current cell, terminating the process
