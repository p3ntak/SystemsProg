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
void printStrArr(int arrLength, char *varName, char **arr);

#include "helpers.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_INPUT_LENGTH 200
#define DELIMS " \n"

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
        //*********************only print input during testing*****************
        printf("%s\n",args[i]);
        //*********************************************************************
        i++;
    } while(arg != NULL);
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

void printStrArr(int arrLength, char *varName, char **arr)
{
    for(int i=0; i<arrLength; i++)
    {
        printf("%s[%d]: %s\n", varName, i, arr[i]);
    }
}

#endif //YASH_HELPERS_H
