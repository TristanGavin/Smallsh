# Smallsh
Small shell written in C for OSU's Operating Systems I class

Smallsh is an optimized UNIX shell with minimal functionality. The shell uses fork() and exec() to spawn new processes that
the user requests run. It can also handle writing stdout to files and input files using the dup2() command. Smallsh allows for
the execution of background processes using & at the end of the command line. The terminal handles signals passed in from the 
user such as CTR^C to exit a foreground process and CTR^Z to enter forground mode wich disables the ability to use background processes. 
