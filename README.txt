Jillian Crowley
smallsh (Portfolio Assignment)

Writes smallsh, a shell in C. Smallsh will implement a subset of 
features of well-known shells, such as bash. The program:
- provides a prompt for running commands
- handles blank lines and comments, which are lines beginning with the # character
- provides expansion for the variable $$
- executes 3 commands exit, cd, and status via code built into the shell
- executes other commands by creating new processes using a function from the exec family of functions
- supports input and output redirection
- supports running commands in foreground and background processes
- implements custom handlers for 2 signals, SIGINT and SIGTSTP


Instructions:

Establish SSH connection to server using a tool like PuTTy
Place code file smallsh.c and the grading script p3testscript in the same 
directory on the server and navigate to that directory in the terminal


Type the following two lines into the terminal to compile the code using gcc to 
create an executable file named smallsh and run the program: 
gcc --std=gnu99 -o smallsh smallsh.c
./smallsh


Sample terminal output after compiling and running smallsh.c:
:

To run the grading script, place it in the same directory as smallsh, 
chmod it (chmod +x ./p3testscript)
Then run this command from a bash prompt:
./p3testscript 2>&1
or
./p3testscript 2>&1 | more
or
./p3testscript > mytestresults 2>&1 
