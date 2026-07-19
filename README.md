# C brain-fuck interpreter
Thinking of the idea on a whim, I made this interpreter (finished to a working condition in under 24 hours!) because of, well, boredom, and wanting to learn more of C!  

This turned out to actually be fun, so I'll probably give this a bit more support, beyond just a basic interpreter. Note that I won't bother with porting to other systems (despite `os_defs.h`), but im 100% willingly to include code YOU make for porting!

You may optionally want to implement some extensions I made by defining the macro `EXTENSIONS`, that provides the following new characters:
`*`: Return the value of the current cell, terminating the process
