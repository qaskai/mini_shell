#ifndef BUILTINS_H
#define BUILTINS_H

#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <limits.h>
#include <errno.h>

#include "builtins.h"

int echo(char*[]);
int fexit(char*[]);
int fcd(char*[]);
int fkill(char*[]);
int fls(char*[]);
int undefined(char *[]);

int strtoint(char*); // returns INT_MAX if failed

builtin_pair builtins_table[]={
	{"exit",	&fexit},
	{"lecho",	&echo},
	{"lcd",		&fcd},
	{"lkill",	&fkill},
	{"lls",		&fls},
	{NULL,NULL}
};

int 
echo( char * argv[])
{
	int i =1;
	while  (argv[i]!=NULL) {
        write(1, argv[i], strlen(argv[i++]));
        write(1, " ", 1);
    }

	write(1, "\n", 1);
	return 0;
}


int
fexit( char* argv[] )
{
    exit(0);
}

int
fcd( char* argv[] )
{
    if (argv[2]!=NULL) 
        return BUILTIN_ERROR;
    
    char* dest= argv[1];
    if (dest==NULL)
        dest= getenv("HOME");

    if (chdir(dest)) 
        return BUILTIN_ERROR;
    

    return 0;
}


int
fkill( char* argv[] ) 
{
    int sig= SIGTERM, pid;
    if ( (argv[3]!=NULL && argv[2]!=NULL ) || argv[1]==NULL)
        return BUILTIN_ERROR;
    if (argv[2]!=NULL) {
        sig= strtoint(argv[1]+1);
        pid= strtoint(argv[2]);
    }
    else
        pid= strtoint(argv[1]);

    if (sig==INT_MAX || pid==INT_MAX)
        return BUILTIN_ERROR;

    if (kill(pid, sig))
        return BUILTIN_ERROR;

    return 0;
}


int
fls( char* argv[] )
{

    DIR* dir= opendir(".");
    if (dir==NULL || argv[1]!=NULL)
        return BUILTIN_ERROR;

    struct dirent* content;
    while((content= readdir(dir))!=NULL) {
        if (content->d_name[0]=='.')
            continue;
        write(1, content->d_name, strlen(content->d_name));
        write(1, "\n", 1);
    }

    if (closedir(dir))
        return BUILTIN_ERROR;

    return 0;
}


int 
undefined(char * argv[])
{
	fprintf(stderr, "Command %s undefined.\n", argv[0]);
	return BUILTIN_ERROR;
}


//*********************************************************

int 
strtoint( char* string) {
    int q=0, sign=1;
    if (string[0]==0) // empty string
        return INT_MAX;
    
    if (string[0]=='-') {
        if (string[1]==0)
            return INT_MAX;
        ++q; sign=-1;
    }

    int end=q;
    while(string[q]!=0) ++q;

    int ret=0, powerOfTen=1;
    while(--q>=end) {
        char c = string[q];
        if ( c>'9' || c<'0' ) // found sth that is not a number
            return INT_MAX;
        ret += (c-48)*powerOfTen;
        powerOfTen*=10;
    }

    return ret*sign;    
}



#endif
