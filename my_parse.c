#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#include "config.h"
#include "siparse.h"
#include "utils.h"
#include "builtins.h"

char* cycleTheBuffor(char* buff, char* current, char* freeSpace, int sizeOfBuff) {
    char* c=current;
    while(c < buff+sizeOfBuff && c<freeSpace)
        c++;
    if (c+1<buff+sizeOfBuff)
        c++;

    int i;
    for (i=0; current+i < freeSpace; ++i)
        *(buff+i)= *(current+i);
    
    return buff+i;
}


int pipelines_empty_command(line* ln) {
    int i=-1;
    while(ln->pipelines[++i]!=NULL) {
        pipeline pipeln= ln->pipelines[i];
        int k = -1;
        command* com;
        while ( (com=pipeln[(++k)]) !=NULL) {
            if ( com==NULL || com->argv==NULL || com->argv[0]==NULL )
                return -1;
        }
    }
    return 0;
}


char* tooLongInputLine(char* buff, char* c, char* current, char* freeSpace, int sizeOfBuff, int* dataInBuffor) {
    fprintf(stderr, "%s\n", SYNTAX_ERROR_STR); 
    while((c==freeSpace) || (*c!='\n' && *c)) { // find EOL for that line
        c++;
        if (c>=freeSpace) {
            int r = read(0, buff, sizeOfBuff);
            freeSpace= buff + r;
            c=buff;
        }
    }
    *dataInBuffor= freeSpace-c;
    return c;
}


command* parseLine(char* current) {
    line* ln;
    ln= parseline(current);
                
    if (ln==NULL) { // parsing failed
        write(2, SYNTAX_ERROR_STR, sizeof(SYNTAX_ERROR_STR));
        write(2, "\n", 1);
        return NULL;
    }

    command* com= pickfirstcommand(ln);
    if (com==NULL || com->argv[0]==NULL)
        return NULL;


    return com;
}