#ifndef _MY_PARSE_H
#define _MY_PARSE_H

#include "siparse.h"
#include "utils.h"

char* cycleTheBuffor(char* buff, char* current, char* freeSpace, int sizeOfBuff);
char* tooLongInputLine(char* buff, char* c, char* current, char* freeSpace, int sizeOfBuff, int* dataInBuffor);
command* parseLine(char* current);
int pipelines_empty_command(line* ln);


#endif