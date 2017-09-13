//
// Created by matt on 9/7/17.
//

#ifndef YASH_HELPERS_H
#define YASH_HELPERS_H

//function declarations
struct PipedArgs
{
    char **args1;
    char **args2;
};
struct Job
{
    char *line;
    int pid_no;
    int runningStatus; //boolean
    int task_no;
};
char *readLineIn(void);
char **parseLine(char *line);
int countArgs(char **args);
int pipeQty(char **args);
int pipeBGExclusive(char **args);
struct PipedArgs getTwoArgs(char **args);
void yash_fg(struct Job *jobs, int activeJobSize);
int yash_bg(struct Job *jobs, int activeJobSize);
int yash_jobs(struct Job *jobs, int activeJobsSize);
int processToBackground(char **args);
int containsInRedir(char **args);
int containsOutRedir(char **args);
void addToJobs(struct Job *jobs, char *line, int *activeJobsSize);
void startJobsPID(struct Job *jobs, int pid, int activeJobsSize);
void removeFromJobs(struct Job *jobs, int pid, int *activeJobsSize);
void setJobStatus(struct Job *jobs, int pid, int activeJobsSize, int runningStatus);
void killProcs(struct Job *jobs, int *activeJobsSize);
int containsAmp(char **args);
void init_shell(void);
void removeLastFromJobs(struct Job *jobs, int *activeJobsSize);
void removeAmp(char **args);

//#include "helpers.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_INPUT_LENGTH 200
#define DELIMS " \n"
#define FINISHED_INPUT 1
#define BUILT_IN_FG "fg"
#define BUILT_IN_BG "bg"
#define BUILT_IN_JOBS "jobs"
#define MAX_NUMBER_JOBS 50
#define RUNNING 1
#define STOPPED 0

//global vars
int shell_pid;

//returns how many '|' are in the arguments
int pipeQty(char **args)
{
    int pipeCount = 0;
    int numArgs = countArgs(args);
    for (int i=0; i<numArgs;i++)
    {
        if(strstr(args[i],"|") && strlen(args[i]) == 1) pipeCount++;
    }
    return pipeCount;
}

//returns 0 if pipe and & are not exclusive
int pipeBGExclusive(char **args)
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
    return numArgs;
}

//read input until end of file or new line
char *readLineIn(void)
{
    char *line = calloc(MAX_INPUT_LENGTH + 1, sizeof(char));

    if(!line)
    {
        fprintf(stderr,"line in memory allocation error\n");
        exit(EXIT_FAILURE);
    }

    line = fgets(line,MAX_INPUT_LENGTH+1,stdin);
    if(line == NULL)
    {
        return NULL;
    }
    if(strcmp(line,"\n") == 0) return "";
    char *lineCopy = strdup(line);

    free(line);
    return lineCopy;
}

//parse the input line into arguments and return array of args
char **parseLine(char *line)
{
    char **args = malloc(MAX_INPUT_LENGTH * sizeof(char*));
    char *arg;
    int i = 0;
    const char *delim = DELIMS;

    char *lineCopy;
    lineCopy = strdup(line);

    do {
        arg = strsep(&lineCopy,delim);
        if(arg == NULL)
        {
            //args[i] = arg;
            break;
        }
        //TODO: multiple spaces in a row throws off args
        args[i] = arg;
        i++;
    } while(arg != NULL);

    args[countArgs(args)-1] = NULL;
    return args;
}


struct PipedArgs getTwoArgs(char **args)
{
    char **args1 = malloc(sizeof(char*) * MAX_INPUT_LENGTH);
    char **args2 = malloc(sizeof(char*) * MAX_INPUT_LENGTH);
    int numArgs = countArgs(args);
    int i = 0;
    int k = 0;
    while(!strstr(args[i],"|"))
    {
        args1[k] = args[i];
        printf("%s\n",args1[i]);
        i++;
        k++;
    };
    args1[k] = NULL;
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
    return pipedArgs;
}

void addToJobs(struct Job *jobs, char *line, int *activeJobsSize)
{
    jobs[*activeJobsSize].line = strdup(line);
    if(*activeJobsSize == 0)
    {
        jobs[*activeJobsSize].task_no = 1;
    } else
    {
        jobs[*activeJobsSize].task_no = jobs[*activeJobsSize-1].task_no + 1;
    }
    jobs[*activeJobsSize].runningStatus = STOPPED;
    jobs[*activeJobsSize].pid_no = 0;

    (*activeJobsSize)++;
    return;
}

int yash_jobs(struct Job *jobs, int activeJobsSize)
{
    for(int i=0; i<activeJobsSize; i++)
    {
        char *runningStr;
        if(jobs[i].runningStatus)
        {
            runningStr = "Running";
        } else runningStr = "Stopped";
        if(i == activeJobsSize-1)
        {
            printf("[%d] + %s    %s    %d\n", jobs[i].task_no, runningStr , jobs[i].line, jobs[i].pid_no);

        } else
        {
            printf("[%d] - %s    %s    %d\n", jobs[i].task_no, runningStr, jobs[i].line, jobs[i].pid_no);
        }
    }
    if(activeJobsSize == 0) printf("No active jobs\n");
    return FINISHED_INPUT;
}

void yash_fg(struct Job *jobs, int activeJobSize)
{
    if(activeJobSize == 0)
    {
        printf("yash: No active jobs");
    }
    int pid = jobs[activeJobSize-1].pid_no;
//    tcsetpgrp(STDIN_FILENO,pid);
    kill(pid,SIGCONT);
//    kill(pid,SIGTTIN);
    return;
}

int yash_bg(struct Job *jobs, int activeJobSize)
{
    return FINISHED_INPUT;
}

void startJobsPID(struct Job *jobs, int pid, int activeJobsSize)
{
    jobs[activeJobsSize-1].pid_no = pid;
    jobs[activeJobsSize-1].runningStatus = RUNNING;
    return;
}

void removeFromJobs(struct Job *jobs, int pid, int *activeJobsSize)
{
    for(int i=0; i<*activeJobsSize; i++)
    {
        if((jobs[i].pid_no == pid))
        {
            for(int j=i; j<(*activeJobsSize-1); j++)
            {
                jobs[j].pid_no = jobs[j+1].pid_no;
                jobs[j].runningStatus = jobs[j+1].runningStatus;
                jobs[j].task_no = jobs[j+1].task_no;
                jobs[j].line = strdup(jobs[j+1].line);
            }
            jobs[*activeJobsSize-1].pid_no = 0;
            jobs[*activeJobsSize-1].runningStatus = STOPPED;
            jobs[*activeJobsSize-1].task_no = 0;
            jobs[*activeJobsSize-1].line = NULL;
        }
    }
    (*activeJobsSize)--;
    return;
}

void setJobStatus(struct Job *jobs, int pid, int activeJobsSize, int runningStatus)
{
    for(int i=0; i<activeJobsSize; i++)
    {
        if(jobs[i].pid_no == pid) jobs[i].runningStatus = runningStatus;
    }
    return;
}

void killProcs(struct Job *jobs, int *activeJobsSize)
{
    for(int i=0; i<*activeJobsSize; i++)
    {
        kill(-jobs[i].pid_no, SIGINT);
        removeFromJobs(jobs, jobs[i].pid_no, activeJobsSize);
        (*activeJobsSize)--;
    }
}

int containsAmp(char **args)
{
    int argCount = countArgs(args);
    for (int i=0; i<argCount; i++)
    {
        if(strstr(args[i],"&")) return 1;
    }
    return 0;
}

void init_shell(void)
{
    shell_pid = getpid();
    printf("Shell_pid: %d\n", shell_pid);
}

void removeLastFromJobs(struct Job *jobs, int *activeJobsSize)
{
    jobs[*activeJobsSize-1].pid_no = 0;
    jobs[*activeJobsSize-1].runningStatus = STOPPED;
    jobs[*activeJobsSize-1].task_no = 0;
    jobs[*activeJobsSize-1].line = NULL;

    (*activeJobsSize)--;
    return;
}

void removeAmp(char **args)
{
    int argCount = countArgs(args);
    if(strcmp(args[argCount-1],"&") == 0)
    {
        args[argCount-1] = NULL;
    }
    return;
}

#endif //YASH_HELPERS_H
