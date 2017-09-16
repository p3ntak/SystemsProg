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
        // ignore sigint and sigtstp while waiting for input
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
        char *lineCpy = strdup(line);
        args = parseLine(line);
        status = executeLine(args, lineCpy);
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
    removeAmp(args);
    FILE *writeFilePointer = NULL;
    FILE *readFilePointer = NULL;
    int argCount = countArgs(args);
    int redirIn = containsInRedir(args);
    int redirOut = containsOutRedir(args);
    int fd = open("/dev/null", O_WRONLY);

    pid_ch1 = fork();
    if (pid_ch1 == 0)
    {
        setsid();
        dup2(fd, STDERR_FILENO);
        if (redirOut >= 0)
        {
            if (redirOut + 1 < argCount)
            {
                writeFilePointer = fopen(args[redirOut + 1], "w+");
                if (writeFilePointer)
                {
                    dup2(fileno(writeFilePointer), STDOUT_FILENO);
                    removeRedirArgs(args, redirOut);
                } else
                {
                    fprintf(stderr, "Cannot open file %s\n", args[redirOut + 1]);
                    return FINISHED_INPUT;
                }
            } else
            {
                fprintf(stderr, "Invalid Expression");
                return FINISHED_INPUT;
            }
        }

        if (redirIn >= 0)
        {
            if (redirIn + 1 < argCount)
            {
                readFilePointer = fopen(args[redirIn + 1], "r");
                if (readFilePointer)
                {
                    dup2(fileno(readFilePointer), STDIN_FILENO);
                    removeRedirArgs(args, redirIn);
                } else
                {
                    fprintf(stderr, "Cannot open file %s\n", args[redirIn + 1]);
                    return FINISHED_INPUT;
                }
            } else
            {
                fprintf(stderr, "Invalid Expression");
                return FINISHED_INPUT;
            }
        }

        if(redirIn > 0 && redirOut > 0)
        {
            if(execvp(args[0], args) == -1)
            {
                perror("Problem executing command");
                removeLastFromJobs(jobs, pactiveJobsSize);
                _Exit(EXIT_FAILURE);
            }
        } else if(redirIn < 0 && redirOut <0)
        {
            dup2(fd, STDOUT_FILENO);
            if(execvp(args[0], args) == -1)
            {
                perror("Problem executing command");
                removeLastFromJobs(jobs, pactiveJobsSize);
                _Exit(EXIT_FAILURE);
            }
        } else if(redirIn >= 0 && redirOut <0)
        {
            dup2(fd, STDOUT_FILENO);
            if(execvp(args[0], args) == -1)
            {
                perror("Problem executing command");
                removeLastFromJobs(jobs, pactiveJobsSize);
                _Exit(EXIT_FAILURE);
            }
        } else if(redirIn < 0 && redirOut >= 0)
        {
            if(execvp(args[0], args) == -1)
            {
                perror("Problem executing command");
                removeLastFromJobs(jobs, pactiveJobsSize);
                _Exit(EXIT_FAILURE);
            }
        }

    } else if (pid_ch1 < 0)
    {
        perror("error forking");
    } else if (pid_ch1 > 0)
    {
        signal(SIGCHLD, proc_exit);
        startJobsPID(jobs, pid_ch1, activeJobsSize);
    }
    if(writeFilePointer != NULL) fclose(writeFilePointer);
    if(readFilePointer != NULL) fclose(readFilePointer);
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
        int count = 0;
        while(count<1)
        {
            pid = waitpid(pid_ch1, &status, WUNTRACED | WCONTINUED);
            if (pid == -1) {
                perror("waitpid");
            }
            if (WIFEXITED(status)) {
                removeFromJobs(jobs, pid_ch1, pactiveJobsSize);
                count++;
            } else if (WIFSTOPPED(status)) {
                setJobStatus(jobs, pid_ch1, activeJobsSize, STOPPED);
                count++;
            }
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
    FILE *writeFilePointer = NULL;
    FILE *readFilePointer = NULL;
    int argCount1 = countArgs(args1);
    int argCount2 = countArgs(args2);
    int redirIn1 = containsInRedir(args1);
    int redirIn2 = containsInRedir(args2);
    int redirOut1 = containsOutRedir(args1);
    int redirOut2 = containsOutRedir(args2);

    if (pipe(pfd) == -1)
    {
        perror("pipe");
        return FINISHED_INPUT;
    }

    pid_ch1 = fork();
    if(pid_ch1 > 0)
    {
        //parent
        pid_ch2 = fork();
        if(pid_ch2 > 0)
        {
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
                    count++;
                } else if(WIFSTOPPED(status))
                {
                    setJobStatus(jobs, pid_ch1, activeJobsSize, STOPPED);
                    count++;
                } else if(WIFCONTINUED(status))
                {
                    setJobStatus(jobs, pid_ch1, activeJobsSize, RUNNING);
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

            if(redirOut2 >= 0)
            {
                if(redirOut2+1 < argCount2)
                {
                    writeFilePointer = fopen(args2[redirOut2 + 1], "w+");
                    if (writeFilePointer)
                    {
                        dup2(fileno(writeFilePointer), STDOUT_FILENO);
                        removeRedirArgs(args2, redirOut2);
                    } else
                    {
                        fprintf(stderr, "Cannot open file %s\n",args2[redirOut2 + 1]);
                        return FINISHED_INPUT;
                    }
                } else
                {
                    fprintf(stderr, "Invalid Expression");
                    return FINISHED_INPUT;
                }
            }

            if(redirIn2 >= 0)
            {
                if(redirIn2+1 < argCount2)
                {
                    readFilePointer = fopen(args2[redirIn2+1], "r");
                    if(readFilePointer)
                    {
                        dup2(fileno(readFilePointer), STDIN_FILENO);
                        removeRedirArgs(args2, redirIn2);
                    } else
                    {
                        fprintf(stderr, "Cannot open file %s\n",args2[redirIn2 + 1]);
                        return FINISHED_INPUT;
                    }
                } else
                {
                    fprintf(stderr, "Invalid Expression");
                    return FINISHED_INPUT;
                }
            }
            if(execvp(args2[0], args2) == -1)
            {
                perror("Problem executing command 2");
                removeLastFromJobs(jobs, pactiveJobsSize);
                _Exit(EXIT_FAILURE);
                return FINISHED_INPUT;
            }
        }
    } else
    {
        setsid();
        close(pfd[0]);
        dup2(pfd[1], STDOUT_FILENO);

        if(redirOut1 >= 0)
        {
            if(redirOut1+1 < argCount1)
            {
                writeFilePointer = fopen(args1[redirOut1 + 1], "w+");
                if (writeFilePointer)
                {
                    dup2(fileno(writeFilePointer), STDOUT_FILENO);
                    removeRedirArgs(args1, redirOut1);
                } else
                {
                    fprintf(stderr, "Cannot open file %s\n",args1[redirOut1 + 1]);
                    return FINISHED_INPUT;
                }
            } else
            {
                fprintf(stderr, "Invalid Expression");
                return FINISHED_INPUT;
            }
        }

        if(redirIn1 >= 0)
        {
            if(redirIn1+1 < argCount1)
            {
                readFilePointer = fopen(args1[redirIn1+1], "r");
                if(readFilePointer)
                {
                    dup2(fileno(readFilePointer), STDIN_FILENO);
                    removeRedirArgs(args1, redirIn1);
                } else
                {
                    fprintf(stderr, "Cannot open file %s\n",args1[redirIn1 + 1]);
                    return FINISHED_INPUT;
                }
            } else
            {
                fprintf(stderr, "Invalid Expression");
                return FINISHED_INPUT;
            }
        }
        if(execvp(args1[0], args1) == -1)
        {
            perror("Problem executing command 1");
            removeLastFromJobs(jobs, pactiveJobsSize);
            _Exit(EXIT_FAILURE);
            return FINISHED_INPUT;
        }
    }
    if(writeFilePointer != NULL) fclose(writeFilePointer);
    if(readFilePointer != NULL) fclose(readFilePointer);
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
    pid_t	sig_chld_pid;
    sig_chld_pid = wait(NULL);
    for(int i=0; i<activeJobsSize; i++)
    {
        if(jobs[i].pid_no == sig_chld_pid)
            printf("\n[%d] DONE    %s\n", jobs[i].task_no, jobs[i].line);
    }
    removeFromJobs(jobs, sig_chld_pid, pactiveJobsSize);
    signal(SIGCHLD,SIG_DFL);
    return;
}

void fg_handler(int signo)
{
    pid_t	sig_chld_pid;

    sig_chld_pid = wait(NULL);
    removeFromJobs(jobs, sig_chld_pid, pactiveJobsSize);
    signal(SIGCHLD,SIG_DFL);
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
