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
#include "execute.h"

int performExec(command* com, int inFD, int outFD) {
    if ( com==NULL || com->argv==NULL || com->argv[0]==NULL )
        return -1; 
    
    if (dup2(inFD, 0)<0) {
        fprintf(stderr, "Duplication of file %d descriptor failed\n", inFD);
        exit(1);
    }
    if (0!=inFD)
        if (close(inFD)) {
        fprintf(stderr, "Close on %d failed\n", inFD); exit(1);
        }
    if (dup2(outFD, 1)<0) {
        fprintf(stderr, "Duplication of file %d descriptor failed\n", outFD);
        exit(1);
    }
    if (1!=outFD)
        if (close(outFD)) {
            fprintf(stderr, "Close on %d failed\n", outFD); exit(1);
        }

    if (redirections(com)) { // need to add taking care of errors here
        _exit(1);
    }
    int t = execvp(com->argv[0], com->argv);
    if (t==-1) {
        write(2, com->argv[0], strlen(com->argv[0])); 
        if (errno==EACCES)  // no access
            write(2, ": permission denied\n", strlen(": permission denied\n"));
        else if (errno==ENOENT)  // no such file to execute
            write(2, ": no such file or directory\n", strlen(": no such file or directory\n"));
        else  // other error
            write(2, ": exec error\n", strlen(": exec error\n"));
                            
        _exit(EXEC_FAILURE);
    }
    exit(0);
}


int callBuiltins(command* com) {
    if ( com==NULL || com->argv==NULL || com->argv[0]==NULL )
        return -1; 
    int q=-1;
    while (builtins_table[++q].name!=NULL) {
        if (!strcmp(builtins_table[q].name, com->argv[0])) {
            if (builtins_table[q].fun(com->argv))
                fprintf(stderr, "Builtin %s error.\n", com->argv[0]);
            return q;
        }
    }
    return -1;
}



int printErrorRedirs(char* path) {
    if (errno==EACCES)
        fprintf(stderr, "%s: permission denied\n", path);
    else if (errno==ENOENT)
        fprintf(stderr, "%s: no such file or directory\n", path);
    else // something else went wrong
        fprintf(stderr, "Open failed\n");
    return -1;
}


int redirections(command* com) {  // change errors in this function to print massage on stderr from shell thread
    if ( com==NULL || com->argv==NULL || com->argv[0]==NULL )
        return -1; 
    int i=-1;

    while(com->redirs[++i]!=NULL) {
        char* path= com->redirs[i]->filename;
        int flags= com->redirs[i]->flags;
        int existance= access(path, F_OK);
        int fd; // file descriptor

        if (IS_RIN(flags)) {
            int cl = close(0);
            if (cl==-1) {
                fprintf(stderr, "Close failed\n");
                return -1;
            }
            fd= open(path, O_RDONLY);
            if (fd==-1)
                return printErrorRedirs(path);
            else if (fd!=0) {
                fprintf(stderr, "File opened at wrong file descriptor\n");
                return -1;
            }
        }
        if (IS_ROUT(flags) || IS_RAPPEND(flags)) {
            int cl= close(1);
            if (cl==-1) {
                fprintf(stderr, "Close failed\n");
                return -1;
            }

            if (IS_ROUT(flags))
                fd=open(path, O_WRONLY | O_TRUNC | O_CREAT);
            else if (IS_RAPPEND(flags)) {
                fd=open(path, O_WRONLY | O_APPEND | O_CREAT);
            }
            if (fd!=-1 && existance==-1) { // created new file, set standard modes
                chmod(path, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            }
            if (fd==-1) 
                return printErrorRedirs(path);
            else if (fd!=1) {
                fprintf(stderr, "File opened at wrong file descriptor\n");
                return -1;
            }
        }
    }

    return 0;
}
