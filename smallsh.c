/*
Tristan Gavin
2/16/2020
smallsh is a shell that will fork and execute new proccesses
it is like a stripped down bash. can run background proccesses.
*/

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

//Function Declarations
void getCmd(char* cmd, int length, char* spid);
int parseCmd(char* cmd, char** parsedCmd, int length);
int runCmd(char** parsedCmd, int cmdCount, int bgProcs[100], struct sigaction SIGINT_action); //runs the command given by user returns status
void catchSIGINT(int signo);
void catchBackground(int signo);
void catchCHLD(int signo);
//void parse

int foregroundMode = 0;
int bgCounter = 0; 
//main proccess this is the shell
int main(){

    int i;                  // counter for loops
    int length = 2048;      // max length of cmd
    char cmd[length];       // store initial input from user (not parsed)
    char *parsedCmd[512];   // parsed commands
    int exit = 1;           // exit while loop
    int status = 0;         // exit status of last proccess
    int cmdCount =0;        // number of comands
    int bgProcs[100];       // array of pids of bg proccesses
    char spid[20]; 

    int pid = getpid();
    sprintf(spid, "%i", pid);

    //create and sig action structs for structs
    struct sigaction SIGINT_action = {0}, background_action = {0};

    //ctr-c catch
    SIGINT_action.sa_handler = SIG_IGN;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = 0;

    //ctr-z catch enter foreground only mode
    background_action.sa_handler = catchBackground;

    //initiate signal catch for parent process
    sigaction(SIGINT, &SIGINT_action, NULL);
    sigaction(SIGTSTP, &background_action, NULL);


    while(exit){
        //set all string memory to \0
        memset(cmd, '\0', length);

        //get command from cmd line and store in cmd
        getCmd(cmd, length, spid);

        //parse cmd for individual words "space seperated"
        cmdCount = parseCmd(cmd, parsedCmd, length);
        



        //if comment ("#") or blank line do nothing 
        if(strncmp(cmd, "#", 1) == 0){
            //do nothing
        }
        //if cmd is status
        else if(strcmp(parsedCmd[0], "status") == 0){
            printf("exit value %i", status);
            fflush(stdout);
            status = 0;
        }
        //if cmd is cd
        //no args change to HOME env var
        //one arg the path to a directory
        else if(strcmp(parsedCmd[0], "cd") == 0){
            if(cmdCount == 1){
                chdir(getenv("HOME"));
                status = 0;
            }
            else if(cmdCount == 2){
                //if directory given doesn't exist print error; else chdir.
                if(chdir(parsedCmd[1]) == -1){
                    printf("Unable to access directory %s\n", parsedCmd[1]);
                    fflush(stdout);
                    status = 1;
                }
                else{
                    chdir(parsedCmd[1]);
                    status = 0;
                }
                
            }    
        }
        //if cmd is exit
        else if(strcmp(parsedCmd[0], "exit") == 0) {
            if(cmdCount > 1){
                printf("exit cannot take parameters\n");
                fflush(stdout);
                status = 1;
            }
            else{
                exit = 0;
            }
        }

        else{
            //run the command!
            status = runCmd(parsedCmd, cmdCount, bgProcs, SIGINT_action); 
        }

        //free all allocated memory for parsedcmd
        for(i = 0; i < cmdCount; i++){
            free(parsedCmd[i]); 
            parsedCmd[i] = NULL;    //sets this memorry to null for reuse.
        }

    }
    return 0;
}

/* ------ GETCMD ------ */

//this needs to get input from user replaces $$ with shell pid
void getCmd(char* cmd, int length, char* spid){

    int exit = 1;
    //command prompt get user input
    do{
        printf(": ");
        fflush(stdout);
        fgets(cmd, length, stdin);
        //replace newline with null terminator
        char* newline = strchr(cmd, '\n');
        if (newline)
            *newline = 0;
    }while(strcmp(cmd, "") == 0);  //make sure user actually put something in


    //this replaces all "$$" with the pid;
    //cmd is orig // inset is 
    char *original;
    char *result;
    int len_pid = strlen(spid); //length of spid
    char *ins;  //next insert point 
    char *temp;
    char *c = "$$";
    int len_rep = strlen(c); //lenth of "$$"
    int len_front; //distance between first pid and last pid
    int pidcnt = 0; //number of replacements

    original = cmd;

    ins = cmd;
    //count the occurences
    for (pidcnt = 0; temp = strstr(ins, c); ++pidcnt) {
        ins = temp + len_rep;
    }
    
    //allocate memory for the length of cmd + new chars - old chars.
    temp = result = malloc(strlen(cmd) + (len_pid - len_pid) * pidcnt + 1);

    //ins points to the next occurence
    //tmp points to end of string
    while(pidcnt--){
        ins = strstr(cmd, c); // move ins to $$
        len_front = ins - cmd; // get the length front to insertion spot
        temp = strncpy(temp, cmd, len_front) + len_front; //copy cmd into temp up to the insertion
        temp = strcpy(temp, spid) + len_pid; //insert pid into insertion spot.
        cmd += len_front + len_rep; //move cmd to end of the insertion.
    }

    strcpy(temp, cmd); // copy the last part of cmd into temp
    cmd = original;
    strcpy(cmd, result);
    free(result);
}

/* ------- PARSECMD ------ */

//takes the cmd line and parses it with " " as delimiter
//then stores each new word in parsedCmd[i][]
int parseCmd(char* cmd, char** parsedCmd, int length){
    char* token;
    int i = 0;

    token = strtok(cmd, " ");
    parsedCmd[i] = malloc(sizeof(char) * 250);
    strcpy(parsedCmd[i], token);
    i++;
    while(token != NULL){   //loop through the string passed and tokenize on " "
        token = strtok(NULL, " ");
        if(token != NULL){
            parsedCmd[i] = malloc(sizeof(char) * 250);
            strcpy(parsedCmd[i], token);
            i++;
        }
    }
    return i;
}



/* ------- RUNCMD ------ */

//first check if command actually exists.
//if command does exist fork() shell and then exec();
//check for < and > in parsedCmd this means the next cmd is a file
//return status

int runCmd(char** parsedCmd, int cmdCount, int bgProcs[100], struct sigaction SIGINT_action){

    int i;                  // counter for loops
    int result;             //used in dup2()
    int status = 0;         //return status number to main
    int childExitMethod;    //for signals from forked proc
    pid_t childPID;         //returned pid from wait()
    int isBackground = 0;   // 1 is yes.
    int fdIn;               //filedescriptor InputFile
    int fdOut;              //filedescriptor OutputFile
    char inputFile[500];
    char outputFile[500];
    memset(inputFile, '\0', 500);
    memset(outputFile, '\0', 500);


            //check for background process
    if(strcmp(parsedCmd[cmdCount-1], "&") == 0){
        isBackground = 1;
    }
    else{
        //if not background allow ctr-c
        // if(foregroundMode != 1)
        SIGINT_action.sa_handler = SIG_DFL;
    }
    //check for input and output files
    for(i = 0; i < cmdCount; i++){
        if(strcmp(parsedCmd[i], "<") == 0){
            strcpy(inputFile, parsedCmd[i+1]);
            parsedCmd[i] = NULL;    //set these to null so that they don't get passed to exec.
            parsedCmd[i+1] = NULL;
            i++;
        }
        else if(strcmp(parsedCmd[i], ">") == 0){
            strcpy(outputFile, parsedCmd[i+1]);
            parsedCmd[i] = NULL;    //set these to null so that they don't get passed to exec.
            parsedCmd[i+1] = NULL;
            break;
        }
    }

    //do fork here.
    //code from lecture 3.2 and 3.4
    pid_t spawnpid = -5;
    
    spawnpid = fork();
    switch (spawnpid)
    {
    case -1: 
        perror("Hull Breach!");
        exit(1);
        break;
    
    //this is the child proccess.
    case 0: 

        //allow for cntr-c
 
        sigaction(SIGINT, &SIGINT_action, NULL);
        //deal with input files/ output files
        if(strcmp(inputFile, "") != 0){
            fdIn = open(inputFile, O_RDONLY);
            if(fdIn == -1){
                printf("No such inputfile\n");
                fflush(stdout);
                status = 1;
                exit(1);
            }

            //redirect stdIn from where fdIn points to
            result = dup2(fdIn, 0);
            if(result == -1){
                printf("dup2 error");
                status = 1;
                exit(2);
            }
            //close on exec
            fcntl(fdIn, F_SETFD, FD_CLOEXEC);

        }
        
        //if output file exists redirect all stdout to that file
        if(strcmp(outputFile, "") != 0){
            fdOut = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644); //open file for writing create if doesn't exist and write over if it does.
            if(fdOut == -1){
                printf("File error");
                fflush(stdout);
                status = 1;
                exit(1);
            }

            //send stdout to where fdOut points to
            result = dup2(fdOut, 1);
            if(result == -1) { 
                printf("dup2 error");
                status = 1; 
                exit(2);
            }

            //close on exec()
            fcntl(fdOut, F_SETFD, FD_CLOEXEC);
            
        }
        
        //remove "&" from parsedcmd
        if(isBackground){
            parsedCmd[cmdCount-1] = NULL;
        }

        //execute entire command. 
        if (execvp(parsedCmd[0], parsedCmd) < 0) {    
            printf("ERROR: exec failed\n");
            fflush(stdout);
            exit(1); break;
        }
    
    //this is the parent proccess
    default:
    //wait until child process terminates unless child proc is background.
        if(isBackground && foregroundMode == 0){
            //let parent process run this one runs in background (WNOHANG)
            //ignore sigint (add this)
            sleep(1);
            childPID = waitpid(spawnpid, &childExitMethod, WNOHANG);
            bgProcs[bgCounter] = spawnpid;      //add background proc to array of bgprcs
            bgCounter++;
            printf("background pid is %i\n", spawnpid);
            break;
        }
        else{
            childPID = waitpid(spawnpid, &childExitMethod, 0);
            //if child exited from exit() set status to return val
            if(WIFEXITED(childExitMethod) != 0){
                status = WEXITSTATUS(childExitMethod);  //set status to child return val
            }
            //exited with signal
            else if(WIFSIGNALED(childExitMethod) != 0){
                printf("Terminated by signal %i\n", WTERMSIG(childExitMethod)); //print signal number
                fflush(stdout);
            }
        }
        break;
    }
    
    //here we run a loop to check all background proccesses are still running
    pid_t temp;
    for(i = 0; i < bgCounter; i++){
        temp = waitpid((pid_t)bgProcs[i], &childExitMethod, WNOHANG); //returns zero if proc still running.
        if(temp > 0){
            // exited normally
            if(WIFEXITED(childExitMethod) != 0){
                status = WEXITSTATUS(childExitMethod);  //get return val
                printf("background process %i exited with satus: %i\n", temp, status);
                fflush(stdout);
            }
            //terminated by signal. (pkill)
            else if(WIFSIGNALED(childExitMethod) != 0){
                printf("background proccess %i terminated by signal %i\n", temp, WTERMSIG(childExitMethod)); //print signal number
                fflush(stdout);
            }
            //move every proc left one to fill subtracted array spot.
            int k;
            for(k = i; k < bgCounter-1; k++){
                bgProcs[k] = bgProcs[k+1];
            }
            bgCounter--;    //decrement bgCounter for removed 
            i--;            //since next command is now in current i spot we need to dec i
        }
    }

    return status;

}

//sigint on any foreground proccesses.
// void catchSIGINT(int signo){
//     sleep(1);
//     char *message = "\n";
//     write(STDOUT_FILENO, message, 2);
// }

//set global var foregroundMode to 1 if background operations not allowed.
void catchBackground(int signo){
    if(foregroundMode == 0){
        char *message = "Entering foreground-only mode (& is now ignored)\n";  
        write(STDOUT_FILENO, message, 50);  //cant use printf
        foregroundMode = 1;         
    }
    else{
        char *message = "Exiting foreground-only mode\n";
        write(STDOUT_FILENO, message, 30);
        foregroundMode = 0;
    }
}

//couldnt get this sig catch working. maybe try and fix later
// terminate background proccess
// void catchCHLD(int signo){
//     char *message = "here";
//     write(STDOUT_FILENO, message, 5);
//     // pid_t childProc;
//     // int status;
//     // childProc = wait(&status);

// }