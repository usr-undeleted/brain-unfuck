# C brain-fuck interpreter
Thinking of the idea on a whim, I made this interpreter (finished to a working condition in under 24 hours!) because of, well, boredom, and wanting to learn more of C!  

This turned out to actually be fun, so I'll probably give this a bit more support, beyond just a basic interpreter. Note that I won't bother with porting to other systems (despite `os_defs.h`), but im 100% willingly to include code YOU make for porting!

(mostly) following `https://esolangs.org/wiki/Brainfuck#Conventions`, the interpreter follows these conventions:
1. '#'s will comment out the entire next line.
2. The array is 524288 cells big, each cell being by default unsigned 8 bits, but the type wint_t (for unicode printing on cells) can be enabled with the macro USE_WCHAR
3. The pointer will wrap around the array, meaning that going left at the start will lead you to the end, or going right at the end will leader you to the start.
4. Writing 10 to a cell and printing it will always lead to a new character.
5. Unicode characters may be written.

You may optionally have the interpreter use what my fork of brainfuck, "braindelete" (ty konakona!). It is enabled by compiling the program with the macros EXTENSIONS and (optionally) USE_WCHAR (replaces cells with the wint_t type, made up of 4 bytes).  
braindelete provides the following extensions:  
`*`: Return the value of the current cell, terminating the process.  
`=`: Changes the output stream to the value of the current cell. Only works for stdin (0), stdout (1), or stderr(2), any other value will end the execution of the interpreter.  
`~`: Flip the bits on a cell.
`?`: Set the cell's value to a random value.
`@`: Sleep for (at least) the time specified on a cell, in seconds.
`&`: Sleep for (at least) the time specified on a cell, in microseconds.
