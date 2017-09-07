#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>

//constant values
#define MAX_INPUT_LENGTH 200
#define DELIMS " "
#define OPERATORS "&><|"
#define FINISHED_INPUT 1
#define EXIT_YASH 0

//function declarations
char *readLineIn(void);
char **parseLine(char *line);
int executeLine(char **args);
void mainLoop(void);
int countArgs(char **args);
int startPipedOperation(char **args1, char **args2);
int startOperation(char **args);
int pipeQty(char **args);
int pipeBGExclusiveCheck(char **args);
static void sig_int(int signo);
static void sig_tstp(int signo);
void splitPipedArgs(char **args);
struct PipedArgs getTwoArgs(char **args);

// Global Vars
int pid_ch1, pid_ch2, pid;

//Structs
struct PipedArgs
{
    char **args1;
    char **args2;
};

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
    do
    {
        printf("# ");
        line = readLineIn();
        if(strcmp(line,"") == 0) continue;
        args = parseLine(line);
        status = executeLine(args);
        printf("\n");
    } while(status);
    return;
}

//read input until end of file or new line
char *readLineIn(void)
{
    //TODO: will have memory leak here unless line is freed
    char *line = calloc(MAX_INPUT_LENGTH + 1, sizeof(char));
    int i = 0;

    if(!line)
    {
        fprintf(stderr,"line in memory allocation error\n");
        exit(EXIT_FAILURE);
    }

    line = fgets(line,MAX_INPUT_LENGTH+1,stdin);
    if(line == NULL) exit(EXIT_FAILURE);
    if(strcmp(line,"\n") == 0) return "";

    return line;
}

//parse the input line into arguments and return array of args
char **parseLine(char *line)
{
    //TODO: will have memory leak here unless arg and args are freed
    char **args = malloc(MAX_INPUT_LENGTH * sizeof(char*));
    char *arg = malloc(MAX_INPUT_LENGTH * sizeof(char));
    int i = 0;
    const char *delim = DELIMS;

    char *lineCopy;
    lineCopy = strdup(line);
    while(arg != NULL)
    {
        arg = strsep(&lineCopy,delim);
        if(arg == NULL) break;
        //TODO: multiple spaces in a row throws off args
        args[i] = arg;
        //*********************only print input during testing*****************
        printf("%s\n",args[i]);
        //*********************************************************************
        i++;
    }
    return args;
}

int executeLine(char **args)
{
    if(!*args) return FINISHED_INPUT;

    int inputPiped = pipeQty(args);
    printf("INPUT PIPED: %d\n",inputPiped);

    if(inputPiped > 0)
    {
        struct PipedArgs pipedArgs = getTwoArgs(args);
        return startPipedOperation(pipedArgs.args1, pipedArgs.args2);
    }
    return startOperation(args);
}

//determine how many arguments were given to input
int countArgs(char **args)
{
    if(!*args) return 0;
    int numArgs = 0;
    int i=0;
    while(args[i] != NULL)
    {
        numArgs++;
        i++;
    }
    return numArgs++;
}

int startOperation(char **args)
{
    pid_t pid, wpid;
    int status;

    pid = fork();
    if(pid == 0)
    {
        if(execvp(args[0], args) == -1)
        {
            perror("problem executing command");
        }
        return FINISHED_INPUT;
    } else if(pid < 0)
    {
        perror("error forking");
    } else
    {
        int count = 0;
        while(count<2)
        {
            wpid = waitpid(-1, &status, WUNTRACED | WCONTINUED);
            if(wpid == -1)
            {
                perror("waitpid");
                return FINISHED_INPUT;
            }
            if(WIFEXITED(status))
            {
                printf("child_%d_exited,_status=%d\n", wpid, WEXITSTATUS(status));
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
    int arg1Count = countArgs(args1);
    int arg2Count = countArgs(args2);

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
                    return FINISHED_INPUT;
                }
                if(WIFEXITED(status))
                {
                    printf("child_%d_exited,_status=%d\n", pid, WEXITSTATUS(status));
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
            execvp(args2[0], args2);
        }
    } else
    {
        setsid();
        close(pfd[0]);
        dup2(pfd[1], STDOUT_FILENO);
        execvp(args1[0], args1);
    }

    return FINISHED_INPUT;
}

//returns how many '|' are in the arguments
int pipeQty(char **args)
{
    int pipeCount = 0;
    int numArgs = countArgs(args);
    for (int i=0; i<numArgs;i++)
    {
        if(strstr(args[i],"|")) pipeCount++;
    }
    return pipeCount;
}

//returns 0 if pipe and & are not exclusive
int pipeBGExclusiveCheck(char **args)
{
    int pipeCount = 0;
    int backgroundCount = 0;
    int numArgs = countArgs(args);
    for (int i=0; i<numArgs; i++)
    {
        if(strstr(args[i],"|")) pipeCount++;
        if(strstr(args[i],"&")) backgroundCount++;
    }
    if((pipeCount>0) && (backgroundCount>0))
    {
        return 0;
    }
    return 1;
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

struct PipedArgs getTwoArgs(char **args)
{
    char **args1 = malloc(sizeof(char*) * MAX_INPUT_LENGTH);
    char **args2 = malloc(sizeof(char*) * MAX_INPUT_LENGTH);
    int numArgs = countArgs(args);
    int i = 0;
    while(!strstr(args[i],"|"))
    {
        args1[i] = args[i];
        printf("%s",args1[i]);
        i++;
    };
    args1[i] = NULL;
    i++;
    int j = 0;
    while(i < numArgs)
    {
        args2[j] = args[i];
        i++;
        j++;
    };
    args2[j] = NULL;
    struct PipedArgs pipedArgs = {args1, args2};
    printf("Piped Args args1: %s\nPiped Args args2: %s\n", pipedArgs.args1[0], pipedArgs.args2[0]);
    return pipedArgs;
}

//TODO:for ctrl+d to kill program spawn thread at beginning and have that thread wait for ctrl+d
//then send sigkill to parent