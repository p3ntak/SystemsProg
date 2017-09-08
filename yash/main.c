#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include "helpers.h"
#include <sys/types.h>

//constant values
#define MAX_INPUT_LENGTH 200
#define DELIMS " "
#define OPERATORS "&><|"
#define FINISHED_INPUT 1
#define EXIT_YASH 0

//function declarations
int executeLine(char **args);
void mainLoop(void);
int startPipedOperation(char **args1, char **args2);
int startOperation(char **args);
static void sig_int(int signo);
static void sig_tstp(int signo);
void intHandler(int sig);

// Global Vars
int pid_ch1, pid_ch2, pid;

//main to take arguments and start a loop
int main(int argc, char **argv)
{

    mainLoop();

    return EXIT_SUCCESS;
}

void mainLoop(void)
{
    int status = 1;
    char *line;
    char **args;

    //read input line
    //parse input
    //stay in loop until an exit is requested
    //while waiting for user input SIGINT is ignored so ctrl+c will not stop the shell
    do
    {
        signal(SIGINT, intHandler);
        printf("# ");
        line = readLineIn();
        if(strcmp(line,"") == 0) continue;
        args = parseLine(line);
        status = executeLine(args);
        free(args);
        printf("\n");
        signal(SIGINT, intHandler);
    } while(status);
    return;
}

int executeLine(char **args)
{
    if(!*args) return FINISHED_INPUT;

    int inputPiped = pipeQty(args);
    printf("INPUT PIPED: %d\n",inputPiped);

    //make sure & and | are not both in the argument
    if(!pipeBGExclusive(args))
    {
        printf("Cannot background and pipeline commands "
                       "('&' and '|' must be used separately).");
        return FINISHED_INPUT;
    }

    //if there is a | in the argument then
    if(inputPiped == 1)
    {
        struct PipedArgs pipedArgs = getTwoArgs(args);
        return startPipedOperation(pipedArgs.args1, pipedArgs.args2);
    } else if (inputPiped > 1)
    {
        printf("Only one '|' allowed per line");
        return FINISHED_INPUT;
    }

    return startOperation(args);
}

int startOperation(char **args)
{
    pid_t pid, wpid;
    int status;

    pid = fork();
    if(pid == 0)
    {
        printf("PID value: %d\n", pid);
        if(execvp(args[0], args) == -1)
        {
            perror("Problem executing command");
            _Exit(EXIT_FAILURE);
            return FINISHED_INPUT;
        }
    } else if(pid < 0)
    {
        perror("error forking");
    } else
    {
        int count = 0;
        while(count<1)
        {
            wpid = waitpid(-1, &status, WUNTRACED | WCONTINUED);
            if(wpid == -1)
            {
                perror("waitpid");
                break;
            }
            if(WIFEXITED(status))
            {
                count++;
            } else if(WIFSIGNALED(status))
            {
                printf("child_%d_killed_by_signal%d\n", wpid, WTERMSIG(status));
                count++;
            } else if(WIFSTOPPED(status))
            {
                printf("child_%d_stopped_by_signal%d\n", wpid, WSTOPSIG(status));
                printf("Sending_CONT_to_%d\n", wpid);
                sleep(4);
                kill(wpid, SIGCONT);
            } else if(WIFCONTINUED(status))
            {
                printf("Continuing_%d\n", wpid);
            }
        }
        return FINISHED_INPUT;
    }

    return FINISHED_INPUT;
}

int startPipedOperation(char **args1, char **args2)
{
    int status;
    int pfd[2];

    if (pipe(pfd) == -1)
    {
        perror("pipe");
        exit(-1);
    }

    pid_ch1 = fork();
    if(pid_ch1 > 0)
    {
        printf("Child1_pid_=_%d\n",pid_ch1);
        pid_ch2 = fork();
        if(pid_ch2 > 0)
        {
            //parent
            printf("Child2_pid_=_%d\n",pid_ch2);
            if(signal(SIGINT, sig_int) == SIG_ERR)
            {
                printf("signal(SIGINT)_error");
            }
            if(signal(SIGTSTP, sig_tstp) == SIG_ERR)
            {
                printf("signal(SIGTSTP)_error");
            }
            close(pfd[0]);
            close(pfd[1]);
            int count = 0;
            while(count<2)
            {
                pid = waitpid(-1, &status, WUNTRACED | WCONTINUED);
                if(pid == -1)
                {
                    perror("waitpid");
                    _Exit(EXIT_FAILURE);
                    return FINISHED_INPUT;
                }
                if(WIFEXITED(status))
                {
                    count++;
                } else if(WIFSIGNALED(status))
                {
                    printf("child_%d_killed_by_signal%d\n", pid, WTERMSIG(status));
                    count++;
                } else if(WIFSTOPPED(status))
                {
                    printf("child_%d_stopped_by_signal%d\n", pid, WSTOPSIG(status));
                    printf("Sending_CONT_to_%d\n", pid);
                    sleep(4);
                    kill(pid, SIGCONT);
                } else if(WIFCONTINUED(status))
                {
                    printf("Continuing_%d\n", pid);
                }
            }
            exit(1);
        } else
        {
            // child 2
            sleep(1);
            setpgid(0, pid_ch1);
            close(pfd[1]);
            dup2(pfd[0],STDIN_FILENO);
            if(execvp(args2[0], args2) == -1)
            {
                perror("Problem executing command");
                _Exit(EXIT_FAILURE);
                return FINISHED_INPUT;
            }
        }
    } else
    {
        setsid();
        close(pfd[0]);
        dup2(pfd[1], STDOUT_FILENO);
        if(execvp(args1[0], args1) == -1)
        {
            perror("Problem executing command");
            _Exit(EXIT_FAILURE);
            return FINISHED_INPUT;
        }
    }

    return FINISHED_INPUT;
}

static void sig_int(int signo)
{
    kill(-pid_ch1, SIGINT);
    printf("\nsig_int caught");
}

static void sig_tstp(int signo)
{
    kill(-pid_ch1, SIGTSTP);
    printf("\nsig_tstp caught");
}


void intHandler(int sig)
{
    signal(sig, SIG_IGN);
    signal(SIGINT,intHandler);
}

//TODO: make sure ctrl + d kills processes before quitting