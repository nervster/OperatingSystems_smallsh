#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>



// create prototype for the functions
void getArgs(char*, char* [], int*, char[], char[], int);
void execute(char* [], int*, int*, int*, char[], char[], struct sigaction);
void tstpSignal(int);
void printStatus(int);
char* read_input(void);

// create global variable for background status
int bgStatus = 1;

int main() {

	// initialize variables
	int status = 1;
	int exitStatus = 0;
	int bgFlag = 0;
	int pid = getpid();
	char* args[512];
	char* line;
	char inputFile[500] = "";
	char outputFile[500] = "";
	

	// sigaction for CTRL^C
	struct sigaction SIGINT_action = { 0 };
	// ignore signal
	SIGINT_action.sa_handler = SIG_IGN;
	sigfillset(&SIGINT_action.sa_mask);
	SIGINT_action.sa_flags = 0;
	sigaction(SIGINT, &SIGINT_action, NULL);

	// Redirect tstp signal/CTRL^Z 
	struct sigaction SIGTSTP_action = { 0 };
	SIGTSTP_action.sa_handler = tstpSignal;
	sigfillset(&SIGTSTP_action.sa_mask);
	SIGTSTP_action.sa_flags = SA_RESTART;
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);

	// create array of null for arguments 
	for (int i = 0; i < 512; i++) {
		args[i] = NULL;
	}

	do {
		// Get input
		printf(": ");
		fflush(stdout);
		line = read_input();
		getArgs(line, args, &bgFlag, inputFile, outputFile, pid);

		// reshow prompt if comment or empty line
		if (args[0][0] == '#' || args[0][0] == '\0') {
			continue;
		}

		// built-in command for exit
		else if (strcmp(args[0], "exit") == 0) {
			status = 0;
		}

		// built-in command for change directory
		else if (strcmp(args[0], "cd") == 0) {
			// Change to the directory specified
			if (args[1]) {
				if (chdir(args[1]) == -1) {
					printf("Directory not found.\n");
					fflush(stdout);
				}
			}
			else {
				// go to HOME path ff dir is not specified
				chdir(getenv("HOME"));
			}
		}

		// display status of last terminated process
		else if (strcmp(args[0], "status") == 0) {
			printStatus(exitStatus);
		}

		// Anything else
		else {
			execute(args, &exitStatus, &bgFlag, &bgStatus, inputFile, outputFile, SIGINT_action );
		}

		// Reset variables
		for (int j = 0; args[j]; j++) {
			args[j] = NULL;
		}
		
		inputFile[0] = '\0';
		outputFile[0] = '\0';
		bgFlag = 0;

	} while (status);

	return 0;
}

// read input from stdin
char* read_input(void)
{
	int buffsize = 2048;
	int index = 0;
	char* buffer = malloc(sizeof(char) * buffsize);
	int c;


	//fgets(buffer, buffsize, stdin);

	while (1) {
		c = getchar();
		if (c == EOF || c == '\n') {
			buffer[index] = '\0';
			return buffer;
		}
		else {
			buffer[index] = c;
		}
		index++;

	}
	return buffer;
	
}

// replaces $$ with pid for each token as input
static char* strReplace(const char* token, const char* search, const char* pidRep)
{
	size_t newLen = strlen(token) + 1;
	char* newToken = malloc(newLen);
	size_t newTokOffset = 0;

	char* val;
	while (val = strstr(token, search)) {

		// copy everything up to the pattern
		memcpy(newToken + newTokOffset, token, val - token);
		newTokOffset += val - token;

		// skip the search in the token
		token = val + strlen(search);

		// adjust memory for pid
		newLen = newLen - strlen(search) + strlen(pidRep);
		newToken = realloc(newToken, newLen);

		// copy the search value
		memcpy(newToken + newTokOffset, pidRep, strlen(pidRep));
		newTokOffset += strlen(pidRep);
	}

	// copy the remaining input
	strcpy(newToken + newTokOffset, token);

	return newToken;
}


// using the line read from stdin (currently in line variable), split each argument using the space delimiter
// Also, if > or < character, then store arg after into output or input variable
// Also, replace $$ with pid for each argument
// Store array of arguments in args
void getArgs(char* line, char* args[], int* bgFlag, char inputFile[], char outputFile[], int pid) {

	int i, j;
	char* inputLine = strdup(line);
	char pidStr[50];
	sprintf(pidStr, "%d", pid);

	if (strcmp(inputLine, "") == 0) {
		args[0] = strdup("");
		return;
	}

	// create tokens from the inputLine
	char* token = strtok(inputLine, " ");

	for (i = 0; token; i++) {
		// Check for & for background process
		if (strcmp(token, "&") == 0) {
			*bgFlag = 1;
		}
		// Check for < for input file
		else if (strcmp(token, "<") == 0) {
			token = strtok(NULL, " ");
			strcpy(inputFile, token);
		}
		// Check for > for output file
		else if (strcmp(token, ">") == 0) {
			token = strtok(NULL, " ");
			strcpy(outputFile, token);
		}
		
		else {
			// replace $$ with pid using strReplace function
			token = strReplace(token, "$$", pidStr);
			args[i] = token;
		}
		
		token = strtok(NULL, " ");
	}
}

// print out the signal for exit or terminated signals
void printStatus(int exitStatus) {

	// exit signal
	if (WIFEXITED(exitStatus)) {
	
		printf("exit value %d\n", WEXITSTATUS(exitStatus));
	}
	// terminated signal
	else {
		
		printf("terminated by signal %d\n", WTERMSIG(exitStatus));
	}
}

void tstpSignal(int signo) {

	if (bgStatus == 1) {
		char* message = "Entering foreground-only mode (& is now ignored)\n";
		write(1, message, 49);
		fflush(stdout);
		bgStatus = 0;
	}

	else {
		char* message = "Exiting foreground-only mode\n";
		write(1, message, 29);
		fflush(stdout);
		bgStatus = 1;
	}
}


void execute(char* args[], int* exitStatus,  int* bgFlag, int* bgStatus, char inputFile[], char outputFile[], struct sigaction signalAction ) {

	int input;
	int output;
	int result;
	pid_t spawnPid = -5;

	spawnPid = fork();
	switch (spawnPid) {

	case -1:
		perror("fork() error\n");
		exit(1);
		break;

	case 0:
		//child process

		// The process will now take ^C as default
		signalAction.sa_handler = SIG_DFL;
		sigaction(SIGINT, &signalAction, NULL);

		
		if (strcmp(inputFile, "") != 0) {
			// open input file
			input = open(inputFile, O_RDONLY);
			if (input == -1) {
				perror("Unable to open input file\n");
				exit(1);
			}
			// copy to stdin
			result = dup2(input, 0);
			if (result == -1) {
				perror("Unable to copy input file\n");
				exit(2);
			}
			// close out file
			fcntl(input, F_SETFD, FD_CLOEXEC);
		}

		
		if (strcmp(outputFile, "") != 0) {

			// open output file
			output = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (output == -1) {
				perror("Unable to open output file\n");
				exit(1);
			}
			// copy to result
			result = dup2(output, 1);
			if (result == -1) {
				perror("Unable to copy output file\n");
				exit(2);
			}
			// close out file
			fcntl(output, F_SETFD, FD_CLOEXEC);
		}

		
		if (execvp(args[0], (char* const*)args)) {
			printf("%s: no such file or directory\n", args[0]);
			fflush(stdout);
			exit(2);
		}
		break;

	default:
		// Execute a process in the background ONLY if allowed
		if (*bgFlag && bgStatus) {
			pid_t actualPid = waitpid(spawnPid, exitStatus, WNOHANG);
			printf("background pid %d is done\n", spawnPid);
			fflush(stdout);
		}
		else {
			pid_t actualPid = waitpid(spawnPid, exitStatus, 0);
		}

		// Check for terminated background processes
		while ((spawnPid = waitpid(-1, exitStatus, WNOHANG)) > 0) {
			printf("background pid %d is done: ", spawnPid);
			printStatus(*exitStatus);
			fflush(stdout);
		}
	}
}




