#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

// global variable to track foreground-only mode
int f_flag = 0;

/* 
*	A CTRL-Z command from the keyboard sends a SIGTSTP signal to the
*	parent shell process and all children at the same time
*/
void handle_SIGTSTP(int signo)
{
	// if foreground-only mode
	if (f_flag == 1) 
	{
		// establish normal mode
		f_flag = 0;

		// use write vs printf since it is reentrant
		char* exit_mode = "\nExiting foreground-only mode\n: ";

		// specify 32 vs strlen since it is reentrant
		write(STDOUT_FILENO, exit_mode, 32);

		// flush output buffers each time text is output
		fflush(stdout);
	}
	
	// if normal mode
	else 
	{
		// establish foreground-only mode
		f_flag = 1;

		// use write vs printf since it is reentrant
		char* enter_mode = "\nEntering foreground-only mode (& is now ignored)\n: ";

		// specify 52 vs strlen since it is reentrant
		write(STDOUT_FILENO, enter_mode, 52);

		// flush output buffers each time text is output
		fflush(stdout);
	}
}

/*
*	Expands any instance of "$$" in a command
*	into the process ID of the shell
*/
void cmdPid(int pid_shell, char* with_pid) 
{
	// pid shell as string 
	char pid_string[255];
	sprintf(pid_string, "%d", pid_shell);

	// store new string with "$$" replaced by pid shell
	char store[2048];

	// to check for instance of "$$"
	char* instance = with_pid;

	// while instance of "$$" exists in string
	while (instance = strstr(instance, "$$")) 
	{
		// calculate number of characters prior to $$ occurance
		int count_char = instance - with_pid;

		// copy the string up to the point of the $$ occurance
		strncpy(store, with_pid, count_char);

		// null terminate string segment from above
		store[count_char] = '\0';

		// append shell pid to string segment
		strcat(store, pid_string);

		// append the rest of the string after shell pid
		strcat(store, instance + strlen("$$"));

		// copy into token and advance instance pointer
		strcpy(with_pid, store);
		instance += 1;
	}
}

/*
*	Shell kills any other processes or jobs it
*	started before it terminates itself
*	Built-in command
*/
void cmdExit(int pid_count, pid_t processes[])
{
	// shell kills any other processes or jobs 
	// that it has started
	int other = 0;
	while (other < pid_count)
	{
		// kill process or job that shell
		// started using pid
		kill(processes[other], SIGTERM);

		// next pid
		other += 1;
	}

	// shell terminates itself
	exit(0);
}

/*
*   Changes the working directory of shell
*   With no arguments, it changes to the directory
*   specified in the HOME environment variable
*   Possible argument is the absolute or relative
*   path of a directory to change to
*	Built-in command
*/
void cmdCd(char** args) 
{
	int success;
	char* folder;

	char* home = getenv("HOME");
	
	// cd with no path
	if (args[1] == NULL) 
	{
		folder = home;
	}

	// cd with path
	else 
	{
		folder = args[1];
	}

	success = chdir(folder);

	// print error message if cd fails
	if (success != 0)
	{
		printf("%s: no such directory\n", folder);
	}
}

/*
*	Prints either the exit status or the terminating
*   signal of the last foreground process ran by shell
*	Built-in command
*/
void cmdStatus(int status)
{
	// if child process terminated normally,
	// print exit value
	if (WIFEXITED(status))
	{
		printf("exit value %d\n", WEXITSTATUS(status));

		// flush output buffers each time text is output
		fflush(stdout);
	}

	// if child process terminated abnormally,
	// print signal
	else
	{
		printf("terminated by signal %d\n", status);

		// flush output buffers each time text is output
		fflush(stdout);
	}
}

/*
*   Creates smallsh, a shell in C
*/
int main() 
{
	// custom signal handler
	// from class Exploration: Signal Handling API
	// redirect ctrl + z
	// initialize empty struct
	struct sigaction SIGTSTP_action = { 0 };
	// fill out struct and register signal handler
	SIGTSTP_action.sa_handler = &handle_SIGTSTP;
	// block all catchable signals while handler runs
	sigfillset(&SIGTSTP_action.sa_mask);
	// set SA_RESTART flag for automated 
	// restart of the interrupted system call or library
	// function after the signal handler finishes
	SIGTSTP_action.sa_flags = SA_RESTART;
	// install signal handler
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);

	// custom signal handler
	// from class Exploration: Signal Handling API
	// ignore ctrl + c
	// initialize empty struct
	struct sigaction SIGINT_action = { 0 };
	// fill out struct and register signal handler
	SIGINT_action.sa_handler = SIG_IGN;
	// install signal handler
	sigaction(SIGINT, &SIGINT_action, NULL);

	// loop condition
	int loop = 1;

	// allocate memory for user input
	char* line = malloc(2048);
	int num_chars;
	char* token = NULL;

	// set maximum size for arguments array
	char* args_list[513];

	// store pid
	int pid;
	// set maximum size for process pid array
	pid_t processes[512];
	// count number of process pids in array
	int pid_count = 0;

	// file redirection
	int redir = -1;
	// input redirection
	char* file_in = NULL;
	// output redirection
	char* file_out = NULL;

	// expansion of $$ variable
	char with_pid[2048];

	// track status
	int status = 0;

	while (loop) 
	{	
		// print the command prompt
		printf(": ");

		// flush output buffers each time text is output
		fflush(stdout);

		// command to be executed in background
		int b_flag = 0;

		// store user input
		// general syntax of a command line, brackets being optional:
		// command [arg1 arg2 ...] [< input_file] [> output_file] [&]
		ssize_t buffer_size = 0;
		num_chars = getline(&line, &buffer_size, stdin);
		if (num_chars == -1)
		{
			// reset stdin status
			clearerr(stdin);
		}		

		// token for entire command line entry prior to enter
		token = strtok(line, " \n");

		// set line to NULL for next command
		line = NULL;

		int args_count = 0;
		while (token != NULL)
		{
			// if file input specified
			if (strcmp(token, "<") == 0)
			{
				token = (strtok(NULL, " \n"));

				// save name of input file
				file_in = strdup(token);

				// get next token
				token = strtok(NULL, " \n");
			}
			
			// if file output specified
			else if (strcmp(token, ">") == 0)
			{
				token = (strtok(NULL, " \n"));

				// save name of output file
				file_out = strdup(token);

				// get next token
				token = strtok(NULL, " \n");
			}

			else 
			{
				// if $$ present in token
				if (strstr(token, "$$") != NULL)
				{
					strcpy(with_pid, token);

					// shell pid
					int pid_shell = getpid();

					// expand $$ to shell_pid
					cmdPid(pid_shell, with_pid);

					// store new string in token
					token = with_pid;
				}

				// store argument in array	
				args_list[args_count] = strdup(token);

				// next argument
				token = strtok(NULL, " \n");
				args_count += 1;
			}

		}

		// if arguments provided
		args_count -= 1;
		if (args_list[args_count] != NULL)
		{
			// if last argument is &
			if (strcmp(args_list[args_count], "&") == 0)
			{
				// null terminate array
				args_list[args_count] = '\0';

				// command to be executed in background if in normal mode
				if (f_flag == 0) 
				{
					b_flag = 1;
				}
			}
		}
		
		// set last argument to NULL
		args_count += 1;
		args_list[args_count] = NULL;

		// if input is a blank line
		if (args_list[0] == NULL)
		{
			// shell re-prompts for another command 
			// when it receives a blank line		
		}

		// if argument is a comment
		else if (strncmp(args_list[0], "#", 1) == 0)
		{
			// shell re-prompts for another command 
			// when it receives a comment
		}

		// if input is exit: built-in command
		else if (strcmp(args_list[0], "exit") == 0)
		{
			cmdExit(pid_count, processes);
		}

		// if input is cd: built-in command
		else if (strcmp(args_list[0], "cd") == 0)
		{
			cmdCd(args_list);
		}

		// if input is status: built-in command
		else if (strcmp(args_list[0], "status") == 0)
		{
			cmdStatus(status);
		}

		// execute command other than the three 
		// built-in commands: exit, cd, status
		else 
		{
			// create child process
			pid = fork();

			// if error creating child process
			if (pid < 0) 
			{
				int job = 0;

				// print error message
				// kill processes or jobs shell has started
				printf("error creating child process\n");
				
				while (job < pid_count)
				{
					// kill process or job that shell
					// started using pid
					kill(processes[job], SIGTERM);

					// increment to next pid
					job += 1;
				}

				// set status to 1
				status = 1;
				break;
			}

			// in the child process
			else if (pid == 0) 
			{
				// if command is to be executed in foreground
				if (b_flag == 0) 
				{
					// specify default action to be
					// taken by signal handler for SIGINT
					SIGINT_action.sa_handler = SIG_DFL;
					sigaction(SIGINT, &SIGINT_action, NULL);
				}

				// if input redirection via stdin
				if (file_in != NULL) 
				{

					// open file for reading only
					redir = open(file_in, O_RDONLY);

					// if shell cannot open file for reading,
					// print an error message and exit
					if (redir == -1) 
					{
						printf("cannot open file %s for input\n", file_in);

						// flush output buffers each time text is output
						fflush(stdout);

						//set exit status to 1 without exiting the shell
						status = 1;
						exit(1);
					}

					// if error in input redirection
					else if (dup2(redir, 0) == -1) 
					{
						printf("error in input redirection from %s\n", file_in);

						// flush output buffers each time text is output
						fflush(stdout);

						// exit(1) without exiting the shell
						exit(1);
					}

					// close input file
					close(redir);
				}

				//if output redirection via stdout
				if (file_out != NULL) 
				{
					// open file for writing only, truncate if  
					// already exists or create if does not exist
					redir = open(file_out, O_WRONLY | O_CREAT | O_TRUNC, 0644);

					// if shell cannot open file for writing,
					// print an error message and exit
					if (redir == -1) 
					{
						printf("cannot open %s for output\n", file_out);

						// flush output buffers each time text is output
						fflush(stdout);

						// set exit status to 1 without exiting the shell
						status = 1;
						exit(1);
					}

					// if error in output redirection
					else if (dup2(redir, 1) == -1) 
					{
						printf("error in output redirection to %s\n", file_out);

						// flush output buffers each time text is output
						fflush(stdout);

						// exit(1) without exiting the shell
						exit(1);
					}

					// close output file.
					close(redir);
				}

				// if command is to be executed in background
				if (b_flag == 1) 
				{
					// if input file not given
					if (file_in == NULL)
					{
						// if shell cannot open 
						// null device to discard data, 
						// print an error message
						redir = open("/dev/null", O_RDONLY);
						if (redir == -1)
						{
							printf("cannot open /dev/null for input\n");

							// flush output buffers each time text is output
							fflush(stdout);

							// exit(1) without exiting the shell
							exit(1);
						}

						// if error in input redirection
						else if (dup2(redir, 0) == -1)
						{
							printf("error in input redirection from /dev/null\n");

							// flush output buffers each time text is output
							fflush(stdout);

							// exit(1) without exiting the shell
							exit(1);
						}

						// close input file
						close(redir);
					}

					//if no output file was specified
					if (file_out == NULL) 
					{
						// if shell cannot open 
						// null device to discard data, 
						// print an error message
						redir = open("/dev/null", O_WRONLY);
						if (redir == -1) 
						{
							printf("cannot open /dev/null for output\n");

							// flush output buffers each time text is output
							fflush(stdout);

							// exit(1) without exiting the shell
							exit(1);
						}

						// if error in output redirection
						else if (dup2(redir, 1) == -1) 
						{
							printf("error in output redirection to /dev/null\n");

							// flush output buffers each time text is output
							fflush(stdout);

							// exit(1) without exiting the shell
							exit(1);
						}

						// close output file
						close(redir);
					}
				}

				// if error in child command execution given arguments
				if (execvp(args_list[0], args_list))
				{
					printf("%s: no such file or directory\n", args_list[0]);

					// flush output buffers each time text is output
					fflush(stdout);

					// set exit status to 1 without exiting the shell
					status = 1;
					exit(1);
				}
			}

			// in parent process
			else 
			{
				// if command is to be executed in the foreground
				if (b_flag == 0) 
				{
					// wait for child's termination			
					pid = waitpid(pid, &status, 0);

					// if process was terminated abnormally, 
					// print error message with signal
					if (WIFSIGNALED(status)) 
					{
						printf("terminated by signal %d\n", status);

						// flush output buffers each time text is output
						fflush(stdout);
					}
				}

				// if command is to be executed in the background
				if (b_flag == 1)
				{
					// parent process is not blocked and
					// continues its execution
					waitpid(pid, &status, WNOHANG);

					// include pid in array
					processes[pid_count] = pid;

					// print pid
					printf("background pid is %d\n", pid);

					// flush output buffers each time text is output
					fflush(stdout);

					// next pid
					pid_count += 1;
				}
			}
		}

		int index = 0;

		// set values to NULL for next loop iteration
		file_in = NULL;
		file_out = NULL;
		while (args_count >= index)
		{
			args_list[index] = NULL;

			// next index
			index += 1;
		}

		// free malloc memory for user input
		free(line);
		free(file_in);
		free(file_out);

		// periodically check for the background child processes to complete
		// WNOHANG: if no child process has terminated, then waitpid returns 0
		pid = waitpid(-1, &status, WNOHANG);

		// while processes still running
		while (pid > 0) 
		{
			// if process terminated normally,
			// print pid and exit value
			if (WIFEXITED(status)) 
			{
				printf("background pid %d is done: exit value %d\n", pid, status);

				// flush output buffers each time text is output
				fflush(stdout);
			}

			// if process terminated abnormally,
			// print pid and signal
			else 
			{
				printf("background pid %d is done: terminated by signal %d\n", pid, status);

				// flush output buffers each time text is output
				fflush(stdout);
			}

			// next background pid
			pid = waitpid(-1, &status, WNOHANG);
		}
	}
	return 0;
}