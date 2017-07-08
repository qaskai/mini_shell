#ifndef _EXECUTE_H_
#define _EXECUTE_H_

#include "siparse.h"
#include "utils.h"

int callBuiltins(command* com);
int performExec(command* com, int inFD, int outFD);
int printErrorRedirs(char* path);
int redirections(command* com);



#endif