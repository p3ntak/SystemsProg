#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include "helpers.h"
#include <fcntl.h>
#include <sys/types.h>


//function declarations
int executeLine(char **args, char *line);
void mainLoop(void);
int startPipedOperation(char **args1, char **args2);
int startOperation(char **args);
int startBgOperation(char **args);
static void sig_int(int signo);
static void sig_tstp(int signo);
static void sig_handler(int signo);
void proc_exit(int signo);

// Global Vars
int pid_ch1, pid_ch2, pid;
int activeJobsSize; //goes up and down as jobs finish
struct Job *jobs;
int *pactiveJobsSize = &activeJobsSize;

//main to take arguments and start a loop
int main(int argc, char **argv)
{
    init_shell();
    jobs = malloc(sizeof(struct Job) * MAX_NUMBER_JOBS);

    mainLoop();

    free(jobs);
    return EXIT_SUCCESS;
}


void mainLoop(void)
{
    int status;
    char *line;
    char **args;
    activeJobsSize = 0;

    //read input line
    //parse input
    //stay in loop until an exit is requested
    //while waiting for user input SIGINT is ignored so ctrl+c will not stop the shell
    do
    {
        // ignore sigint and sigtstp while no processes
        signal(SIGINT, sig_handler);
        signal(SIGTSTP, sig_handler);
        printf("# ");
        line = readLineIn();
        if(line == NULL)
        {
            printf("\n");
            killProcs(jobs, pactiveJobsSize);
            break;
        }
        if(strcmp(line,"") == 0) continue;
        args = parseLine(line);
        status = executeLine(args, line);
        printf("\n");
    } while(status);
    return;
}


int executeLine(char **args, char *line)
{
    if(!*args) return FINISHED_INPUT;
    int returnVal;

    if(!(
            (strcmp(args[0], BUILT_IN_BG) == 0) ||
            (strcmp(args[0], BUILT_IN_FG) == 0) ||
            (strcmp(args[0], BUILT_IN_JOBS) == 0)))
    {
        addToJobs(jobs, line, pactiveJobsSize);
    }

    if(strcmp(args[0], BUILT_IN_BG) == 0)
    {
        yash_bg(jobs, activeJobsSize);
        return FINISHED_INPUT;
    }
    if(strcmp(args[0], BUILT_IN_FG) == 0)
    {
        yash_fg(jobs, activeJobsSize);
        return FINISHED_INPUT;
    }
    if(strcmp(args[0], BUILT_IN_JOBS) == 0)
    {
        return yash_jobs(jobs, activeJobsSize);
    }

    int inputPiped = pipeQty(args);

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
        returnVal = startPipedOperation(pipedArgs.args1, pipedArgs.args2);
        free(pipedArgs.args1);
        free(pipedArgs.args2);
        free(args);
        return returnVal;
    } else if (inputPiped > 1 || inputPiped < 0)
    {
        printf("Only one '|' allowed per line");
        return FINISHED_INPUT;
    }

    //check if args contains '&'
    int inBackground = containsAmp(args);
    if(inBackground)
    {
        returnVal = startBgOperation(args);
    } else
    {
        returnVal = startOperation(args);
    }
    free(args);
    return returnVal;
}

int startBgOperation(char **args)
{
    int status;
    removeAmp(args);
    FILE *writeFilePointer = NULL;
    FILE *readFilePointer = NULL;
    int argCount = countArgs(args);
    int redirIn = containsInRedir(args);
    int redirOut = containsOutRedir(args);

    int pfd[2];

    if (pipe(pfd) == -1)
    {
        perror("pipe");
        return FINISHED_INPUT;
    }

    pid_ch1 = fork();
    if(pid_ch1 == 0)
    {
        // child 1
        setsid();
        pid_ch2 = fork();
        if(pid_ch2 == 0)
        {
            // child 2
            if(redirOut >= 0)
            {
                if(redirOut+1 < argCount)
                {
                    writeFilePointer = fopen(args[redirOut + 1], "w+");
                    if (writeFilePointer)
                    {
                        close(pfd[0]);
                        dup2(fileno(writeFilePointer), STDOUT_FILENO);
                        removeRedirArgs(args, redirOut);
                    } else
                    {
                        fprintf(stderr, "Cannot open file %s\n",args[redirOut + 1]);
                        return FINISHED_INPUT;
                    }
                } else
                {
                    fprintf(stderr, "Invalid Expression");
                    return FINISHED_INPUT;
                }
            }

            if(redirIn >= 0)
            {
                if(redirIn+1 < argCount)
                {
                    readFilePointer = fopen(args[redirIn+1], "r");
                    if(readFilePointer)
                    {
                        close(pfd[1]);
                        dup2(fileno(readFilePointer), STDIN_FILENO);
                        removeRedirArgs(args, redirIn);
                    } else
                    {
                        fprintf(stderr, "Cannot open file %s\n",args[redirIn + 1]);
                        return FINISHED_INPUT;
                    }
                } else
                {
                    fprintf(stderr, "Invalid Expression");
                    return FINISHED_INPUT;
                }
            }
            //child 2
            if(execvp(args[0], args) == -1)
            {
                perror("Problem executing command");
                removeLastFromJobs(jobs, pactiveJobsSize);
                _Exit(EXIT_FAILURE);
            }
        } else if(pid_ch2 < 0)
        {
            perror("error forking");
        } else if(pid_ch2 > 0)
        {
            int fd = open("/dev/null", O_WRONLY);
            close(pfd[0]);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            startJobsPID(jobs, pid_ch1, activeJobsSize);
            pid = waitpid(-1, &status, WUNTRACED | WCONTINUED);
            if (pid == -1)
            {
                perror("waitpid");
            }
            if (WIFEXITED(status))
            {
                removeFromJobs(jobs, pid_ch1, pactiveJobsSize);
            } else if (WIFSIGNALED(status)) {
            } else if (WIFSTOPPED(status))
            {
                setJobStatus(jobs, pid_ch1, activeJobsSize, STOPPED);
            } else if (WIFCONTINUED(status))
            {
                setJobStatus(jobs, pid_ch1, activeJobsSize, RUNNING);
            }
        }
    } else if(pid_ch1 < 0)
    {
        perror("error forking");
    } else if(pid_ch1 > 0)
    {
        // parent
        waitpid(-1, &status, WNOHANG);
        tcsetpgrp(STDIN_FILENO, shell_pid);
    }
    return FINISHED_INPUT;
}

int startOperation(char **args)
{
    int status;
    removeAmp(args);
    FILE *writeFilePointer = NULL;
    FILE *readFilePointer = NULL;
    int argCount = countArgs(args);
    int redirIn = containsInRedir(args);
    int redirOut = containsOutRedir(args);

    int pfd[2];

    if (pipe(pfd) == -1)
    {
        perror("pipe");
        return FINISHED_INPUT;
    }

    signal(SIGCHLD, proc_exit);

    pid_ch1 = fork();
    if(pid_ch1 == 0)
    {
        if(redirOut >= 0)
        {
            if(redirOut+1 < argCount)
            {
                writeFilePointer = fopen(args[redirOut + 1], "w+");
                if (writeFilePointer)
                {
                    close(pfd[0]);
                    dup2(fileno(writeFilePointer), STDOUT_FILENO);
                    removeRedirArgs(args, redirOut);
                } else
                {
                    fprintf(stderr, "Cannot open file %s\n",args[redirOut + 1]);
                    return FINISHED_INPUT;
                }
            } else
            {
                fprintf(stderr, "Invalid Expression");
                return FINISHED_INPUT;
            }
        }

        if(redirIn >= 0)
        {
            if(redirIn+1 < argCount)
            {
                readFilePointer = fopen(args[redirIn+1], "r");
                if(readFilePointer)
                {
                    close(pfd[1]);
                    dup2(fileno(readFilePointer), STDIN_FILENO);
                    removeRedirArgs(args, redirIn);
                } else
                {
                    fprintf(stderr, "Cannot open file %s\n",args[redirIn + 1]);
                    return FINISHED_INPUT;
                }
            } else
            {
                fprintf(stderr, "Invalid Expression");
                return FINISHED_INPUT;
            }
        }

        if(execvp(args[0], args) == -1)
        {
            perror("Problem executing command");
            removeLastFromJobs(jobs, pactiveJobsSize);
            _Exit(EXIT_FAILURE);
        }
    } else if(pid_ch1 < 0)
    {
        perror("error forking");
    } else
    {
        // Parent process
        startJobsPID(jobs, pid_ch1, activeJobsSize);
        // change sig catchers back to not ignore signals
        if (signal(SIGINT, sig_int) == SIG_ERR)
        {
            printf("signal(SIGINT)_error");
        }
        if (signal(SIGTSTP, sig_tstp) == SIG_ERR)
        {
            printf("signal(SIGTSTP)_error");
        }
        pid = waitpid(pid_ch1, &status, WUNTRACED | WCONTINUED);
        if (pid == -1)
        {
            perror("waitpid");
        }
        if (WIFEXITED(status))
        {
            removeFromJobs(jobs, pid_ch1, pactiveJobsSize);
        } else if (WIFSIGNALED(status)) {
        } else if (WIFSTOPPED(status))
        {
            setJobStatus(jobs, pid_ch1, activeJobsSize, STOPPED);
        } else if (WIFCONTINUED(status))
        {
            setJobStatus(jobs, pid_ch1, activeJobsSize, RUNNING);
        }

    }
    if(writeFilePointer != NULL) fclose(writeFilePointer);
    if(readFilePointer != NULL) fclose(readFilePointer);
    return FINISHED_INPUT;
}


int startPipedOperation(char **args1, char **args2)
{
    int status;
    int pfd[2];

    if (pipe(pfd) == -1)
    {
        perror("pipe");
        return FINISHED_INPUT;
    }

    pid_ch1 = fork();
    if(pid_ch1 > 0)
    {
        printf("Child1_pid_=_%d\n",pid_ch1);
        //parent
        pid_ch2 = fork();
        if(pid_ch2 > 0)
        {
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
                startJobsPID(jobs, pid_ch1, activeJobsSize);
                if(pid == -1)
                {
                    perror("waitpid");
                    return FINISHED_INPUT;
                }
                if(WIFEXITED(status))
                {
                    removeFromJobs(jobs, pid_ch1, pactiveJobsSize);
                    count++;
                } else if(WIFSIGNALED(status))
                {
                    printf("child_%d_killed_by_signal%d\n", pid, WTERMSIG(status));
                    count++;
                } else if(WIFSTOPPED(status))
                {
                    setJobStatus(jobs, pid_ch1, activeJobsSize, STOPPED);
                    count++;
                } else if(WIFCONTINUED(status))
                {
                    count = 0;
                    setJobStatus(jobs, pid_ch1, activeJobsSize, RUNNING);
                    printf("Continuing_%d\n", pid);
                    pid = waitpid(-1, &status, WUNTRACED | WCONTINUED);
                }
            }
            return FINISHED_INPUT;
        } else
        {
            // child 2
            sleep(1);
            setpgid(0, pid_ch1);
            close(pfd[1]);
            dup2(pfd[0],STDIN_FILENO);
            if(execvp(args2[0], args2) == -1)
            {
                perror("Problem executing command 2");
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
            perror("Problem executing command 1");
            _Exit(EXIT_FAILURE);
            return FINISHED_INPUT;
        }
    }
    return FINISHED_INPUT;
}


static void sig_int(int signo)
{
    kill(-pid_ch1, SIGINT);
}


static void sig_tstp(int signo)
{
    kill(-pid_ch1, SIGTSTP);
}

void proc_exit(int signo)
{
    int status;
    pid_t	sig_chld_pid;

    while (1) {
        sig_chld_pid = waitpid (-1, &status, WNOHANG);
        if (sig_chld_pid == 0)
            return;
        else if (sig_chld_pid == -1)
            return;
        else
        {
            removeFromJobs(jobs, sig_chld_pid, pactiveJobsSize);
            printf ("SIGCHLD CAUGHT Return code: %d\n", sig_chld_pid);
            return;
        }

    }
}

static void sig_handler(int signo) {
    switch(signo){
        case SIGINT:
            signal(signo,SIG_IGN);
            signal(SIGINT,sig_handler);
            break;
        case SIGTSTP:
            signal(signo,SIG_IGN);
            signal(SIGTSTP,sig_handler);
            break;
        case SIGCHLD:
            signal(signo,SIG_IGN);
            signal(SIGCHLD, sig_handler);
            break;
        default:
            return;
    }

}
