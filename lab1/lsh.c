/*
 * Main source code file for lsh shell program
 *
 * You are free to add functions to this file.
 * If you want to add functions in a separate file
 * you will need to modify Makefile to compile
 * your additional functions.
 *
 * Add appropriate comments in your code to make it
 * easier for us while grading your assignment.
 *
 * Submit the entire lab1 folder as a tar archive (.tgz).
 * Command to create submission archive:
 $> tar cvf lab1.tgz lab1/
 *
 * All the best
 */


#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "parse.h"
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

/*
 * Function declarations
 */

void PrintCommand(int, Command *);
void PrintPgm(Pgm *);
void stripwhite(char *);
void ExecutePgm(Command *);
void ExecutePipe(Pgm *);
void SignalHandler(int);


/* Needed to kill a process with Ctrl-C */
pid_t runningprocess;

/* When non-zero, this global means the user is done using this program. */
int done = 0;

/*
 * Name: main
 *
 * Description: Gets the ball rolling...
 *
 */
int main(void)
{
    Command cmd;
    int n;
    
    /* Default behaviour is to ignore Ctrl-C */
    signal(SIGINT, SIG_IGN);
    
    while (!done) {
        
        char *line;
        line = readline("$ ");
        
        if (!line) {
            /* Encountered EOF at top level */
            done = 1;
        } else {
            /*
             * Remove leading and trailing whitespace from the line
             * Then, if there is anything left, add it to the history list
             * and execute it.
             */
            stripwhite(line);
            
            if(*line) {
                add_history(line);
                /* execute it */
                n = parse(line, &cmd);
                //PrintCommand(n, &cmd);
                ExecutePgm(&cmd);
            }
        }
        
        if(line) {
            free(line);
        }
    }
    return 0;
}

void SignalHandler(int sig) {
    /* Ignore Ctrl-C for now */
    signal(SIGINT, SIG_IGN);
    /* Kill the running process */
    kill(runningprocess,1);
    printf("\n");
    /* Restore the default */
    signal(SIGINT, SignalHandler);
}

void ExecutePgm(Command *cmd)
{
    
    /* Built-in 'exit'-command */
    if(strcmp("exit",cmd->pgm->pgmlist[0]) == 0) {
        printf("Exiting shell...\n");
        exit(0);
    }
    /* Built-in 'cd'-command */
    if (strcmp("cd",cmd->pgm->pgmlist[0]) == 0) {
        if (chdir(cmd->pgm->pgmlist[1]) == -1) {
            perror("Error");
        }
        return;
    }
    
    /* Fork the childprocess */
    pid_t childpid;
    childpid = fork();
    
    /* If it's not a background-process, overrite default-behaviour for Ctrl-C */
    if (cmd->bakground == 0) {
        runningprocess = childpid;
        signal(SIGINT, SignalHandler);
    }
    
    if (childpid < 0) {
        fprintf(stderr, "Fork failed");
    } else if (childpid == 0) {
        
        /* Replace STDIN with file */
        if (cmd->rstdin != NULL) {
            int inputfd = -1;
            
            inputfd = open(cmd->rstdin, O_RDONLY);
            
            if (inputfd < 0) {
                printf("open(2) file failed: %s\n ", cmd->rstdin);
                exit(EXIT_FAILURE);
            }
            dup2(inputfd,0);
            close(inputfd);
        }
        /* Replace STDOUT with file */
        if (cmd->rstdout != NULL) {
            int outputfd = -1;
            
            outputfd = open(cmd->rstdout, O_RDWR | O_CREAT, 00640);
            
            if (outputfd < 0) {
                printf("open(2) file failed: %s\n ", cmd->rstdout);
                exit(EXIT_FAILURE);
            }
            dup2(outputfd,1);
            close(outputfd);
        }
        
        /* If no pipes are used, execute the command. Otherwise call ExecutePipe-function to handle pipes */
        if (cmd->pgm->next == NULL) {
            char **cmd2ex = cmd->pgm->pgmlist;
            /* Print error, if something has gone wrong with the execution of the command */
            if (execvp(*cmd2ex,cmd2ex) == -1) {
                perror("command not found");
                exit(EXIT_FAILURE);
            }
        } else {
            ExecutePipe(cmd->pgm);
        }
        
    } else {
        /* Don't wait, until the process is terminated */
        if (cmd->bakground == 1)
            return;
        
        waitpid(childpid,NULL,0);
        
        /* Go back back to the default-behaviour for Ctrl-C */
        signal(SIGINT, SIG_IGN);
    }
    if (cmd->bakground == 1)
        return;
}

void ExecutePipe(Pgm *p)
{
    int fd[2];
    pid_t childpid;
    
    pipe(fd);
    
    childpid = fork();
    
    if (childpid == 0) {
        /* child process, i.e. the one writing to the pipe */
        dup2(fd[1], 1);
        close(fd[0]);
        close(fd[1]);
        
        /* If no more pipes are coming, execute command. Call the function recursively otherwise. */
        if (p->next->next == NULL) {
            char **childcmd = p->next->pgmlist;
            if (execvp(*childcmd,childcmd) == -1) {
                perror("command not found");
                exit(EXIT_FAILURE);
            }
        } else {
            ExecutePipe(p->next);
        }
        
    } else {
        /* parent process, i.e. the one reading from the pipe */
        dup2(fd[0], 0);
        close(fd[0]);
        close(fd[1]);
        char **parentcmd = p->pgmlist;
        if (execvp(*parentcmd,parentcmd) == -1) {
            perror("command not found");
            exit(EXIT_FAILURE);
        }
        
        int stat;
        while (wait(&stat) > 0)
        {}
    }
    
}


/*
 * Name: PrintCommand
 *
 * Description: Prints a Command structure as returned by parse on stdout.
 *
 */
void PrintCommand (int n, Command *cmd)
{
    printf("Parse returned %d:\n", n);
    printf("   stdin : %s\n", cmd->rstdin  ? cmd->rstdin  : "<none>" );
    printf("   stdout: %s\n", cmd->rstdout ? cmd->rstdout : "<none>" );
    printf("   bg    : %s\n", cmd->bakground ? "yes" : "no");
    PrintPgm(cmd->pgm);
}

/*
 * Name: PrintPgm
 *
 * Description: Prints a list of Pgm:s
 *
 */
void PrintPgm (Pgm *p)
{
    if (p == NULL) {
        return;
    } else {
        char **pl = p->pgmlist;
        
        /* The list is in reversed order so print
         * it reversed to get right
         */
        PrintPgm(p->next);
        printf("    [");
        while (*pl) {
            printf("%s ", *pl++);
        }
        printf("]\n");
    }
}

/*
 * Name: stripwhite
 *
 * Description: Strip whitespace from the start and end of STRING.
 */
void stripwhite (char *string)
{
    register int i = 0;
    
    while (whitespace( string[i] )) {
        i++;
    }
    
    if (i) {
        strcpy (string, string + i);
    }
    
    i = strlen( string ) - 1;
    while (i> 0 && whitespace (string[i])) {
        i--;
    }
    
    string [++i] = '\0';
}
