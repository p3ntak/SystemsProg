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
char *readLineIn(void);
char **parseLine(char *line);
int countArgs(char **args);
int pipeQty(char **args);
int pipeBGExclusive(char **args);
struct PipedArgs getTwoArgs(char **args);

#include "helpers.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_INPUT_LENGTH 200
#define DELIMS " "

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
        printf("\n");
        exit(EXIT_FAILURE);
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
    char *arg = malloc((MAX_INPUT_LENGTH + 1) * sizeof(char));
    int i = 0;
    const char *delim = DELIMS;

    char *lineCopy;
    lineCopy = strdup(line);
    while(arg != NULL)
    {
        arg = strsep(&lineCopy,delim);
        if(arg == NULL)
        {
            args[i] = arg;
            break;
        }
        //TODO: multiple spaces in a row throws off args
        args[i] = arg;
        //*********************only print input during testing*****************
        printf("%s\n",args[i]);
        //*********************************************************************
        i++;
    }
    free(arg);
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
        args1[i] = args[i];
        printf("%s",args1[i]);
        i++;
        k++;
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
    free(args1);
    free(args2);
    return pipedArgs;
}

#endif //YASH_HELPERS_H
