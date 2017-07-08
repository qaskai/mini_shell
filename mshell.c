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
#include "my_parse.h"

//#undef MAX_LINE_LENGTH
//#define MAX_LINE_LENGTH 10


volatile int counter=0;

typedef struct zombie {
    pid_t pid;
    int stat_val;
} zombie;


#define MAX_BACKGROUND_INFO 50

zombie background_info[MAX_BACKGROUND_INFO];
volatile int current_zombies_number=0;
pid_t foreground_processes[MAX_LINE_LENGTH];







int prepareFork(command* com, int lastReadFD, int fd[2], int background);

void change_signal_handling();
void sigint_handler(int sig);
void sigchld_handler(int sig);
int checkif_foreground(pid_t pid);
void print_zombies();


int
main(int argc, char *argv[])
{
    change_signal_handling();

    struct stat infoPrompt;

    fstat(0, &infoPrompt);

    char buff[2*MAX_LINE_LENGTH+1];
    int sizeOfBuff= sizeof(buff)-1;

    char* begOfFreeSpace= buff;
    char* currentBufforPosition= buff;

	while (1) {

        if (S_ISCHR(infoPrompt.st_mode)) {
            print_zombies();
            write(1, PROMPT_STR, sizeof(PROMPT_STR));
        }

        if ( buff+sizeOfBuff - begOfFreeSpace < MAX_LINE_LENGTH ) {
            begOfFreeSpace= cycleTheBuffor(buff, currentBufforPosition, begOfFreeSpace, sizeOfBuff);
            currentBufforPosition=buff;
        }
        
        int r;
        do {
            r= read(0, begOfFreeSpace, buff+sizeOfBuff-begOfFreeSpace);
        } while (r==-1 && errno==EINTR);
        if (!r) break; // EOF
            
        begOfFreeSpace+=r;
                
        while( currentBufforPosition<begOfFreeSpace ) {

            if ( *currentBufforPosition=='\n' || *currentBufforPosition==0 ) { // EMPTY LINE
                currentBufforPosition++; continue;
            }

            char* c=currentBufforPosition;

            while ( c < begOfFreeSpace && *c!='\n' && *c ) // find next EOL
                c++;
                
            if ( c - currentBufforPosition > MAX_LINE_LENGTH ) { // too long input, syntax error
                int r;
                currentBufforPosition= tooLongInputLine(buff, c, currentBufforPosition, begOfFreeSpace,
                                                            sizeOfBuff, &r);
                begOfFreeSpace= currentBufforPosition + r;
                continue; 
            }

            if (c==begOfFreeSpace) // whole line wasn't in the buffor 
                break;

            *c=0;  // mark end of line for parser

            line* ln;
            ln= parseline(currentBufforPosition);
            currentBufforPosition=c+1; // move cursor to indicate beginning of not parsed text

            if (ln==NULL) { // parsing failed
                write(2, SYNTAX_ERROR_STR, sizeof(SYNTAX_ERROR_STR));
                write(2, "\n", 1);
                continue;                
            }

            int background= (ln->flags) & LINBACKGROUND;
            
            //check for empty commands
            if ( ln->pipelines[1]!=NULL || ln->pipelines[0][1]!=NULL )
            	if ( pipelines_empty_command(ln) ) {
            		write(2, SYNTAX_ERROR_STR, sizeof(SYNTAX_ERROR_STR));
	            	write(2, "\n", 1);
	            	continue;
            	}

            // piplineseq
            int i=-1;
            while(ln->pipelines[++i]!=NULL) {
                pipeline pipeln= ln->pipelines[i];

                int k=-1, lastReadFD=0;
                
                while(pipeln[(++k+1)]!=NULL) {
                    int fd[2];
                    if (pipe(fd))
                        fprintf(stderr, "Pipe failed, file descriptor limit reached.\n");
                    lastReadFD= prepareFork(pipeln[k], lastReadFD, fd, background);
                    if (lastReadFD<0)
                        break;
                }
                if (lastReadFD<0)
                    continue;
                
                // block SIGCHILD
                sigset_t mask;
                sigemptyset(&mask);
                sigaddset(&mask, SIGCHLD);
                sigprocmask(SIG_BLOCK, &mask, NULL);
                
                if (k || callBuiltins(pipeln[k])<0) { // last command from pipe uses stdout
                    int fd[2] = { lastReadFD, 1 };
                    prepareFork(pipeln[k], lastReadFD, fd, background);
                }

                // unblock SIGCHILD
                sigprocmask(SIG_UNBLOCK, &mask, NULL);
            }
        }
	}

}


//*********************************************FUNCTIONS*****************************************************//



int prepareFork(command* com, int lastReadFD, int fd[2], int background) {
    if ( com==NULL || com->argv==NULL || com->argv[0]==NULL )
        return -1; 

    int f= fork();
    if (f==-1) exit(1);
    if (f) { // parnet
        if (!background) 
            foreground_processes[counter++]= f;

        if (lastReadFD!=0 && close(lastReadFD)) {
            fprintf(stderr, "Close on %d failed\n", lastReadFD); exit(1);
        }
        if (fd[1]!=1 && close(fd[1])) {
            fprintf(stderr, "Close on %d failed\n", fd[1]); exit(1);
        }
        if (fd[1]==1) { // last command in pipe, wait for children if in foreground
            while (counter>0) {
                sigset_t mask;
                sigprocmask(SIG_BLOCK, NULL, &mask);
                sigdelset(&mask, SIGCHLD);
                sigsuspend(&mask);
            }
        }
        return fd[0];
    }
    else  { // child
        // reset SIGINT and SIGCHLD handling
        struct sigaction act;
        act.sa_handler=SIG_DFL;
        act.sa_flags=0;
        sigemptyset(&act.sa_mask);
        sigaction(SIGINT, &act, NULL);
        sigaction(SIGCHLD, &act, NULL);

        // set new dinasty
        if (background)
            if ( setsid()==-1 )
                fprintf(stderr, "ERROR\n");

        if ( fd[0]!=lastReadFD && close(fd[0]) ) {
            fprintf(stderr, "Close on %d failed\n", fd[0]); exit(1);
        }

        performExec(com, lastReadFD, fd[1]);
    }
    
    return -1; // control shouldn't end up here
}





int checkif_foreground(pid_t pid) {
    int i;
    for (i=0; i<counter; ++i)
        if (foreground_processes[i]==pid) {
            foreground_processes[i]= foreground_processes[counter-1];
            return 1;
        }

    return 0;
}


void print_zombies() {
    //block SIGCHILD and print awaiting zombies
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, NULL);
            
    int i;
    for (i=0; i<current_zombies_number; ++i) {
        int status, pid= background_info[i].pid;

        if (WIFEXITED(background_info[i].stat_val)) {
            status= WEXITSTATUS(background_info[i].stat_val);
            printf("Background process %d terminated. (exited with status %d)\n", pid, status);
        }
        else if (WIFSIGNALED(background_info[i].stat_val)) {
            status= WTERMSIG(background_info[i].stat_val);
            printf("Background process %d terminated. (killed by signal %d)\n", pid, status);
        }
    }

    current_zombies_number=0;

    // unblock SIGCHILD
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}



void change_signal_handling() {
    // change SIGCHILD signal handling

    struct sigaction act;
    act.sa_handler= sigchld_handler;
    act.sa_flags=0;
    sigemptyset(&act.sa_mask);

    sigaction(SIGCHLD, &act, NULL);

    // change SIGINT signal handling

    act.sa_handler= sigint_handler;   // check later if sigaction can be called 
    //act.sa_flags=0;                 // on the same structure for different purposes
    //sigemptyset(&act.sa_mask);
    sigaddset(&act.sa_mask, SIGCHLD);
    sigaction(SIGINT, &act, NULL);
}


void sigchld_handler(int sig) {
    pid_t child;
    do {
        int stat_val;
        child = waitpid(-1, &stat_val, WNOHANG);
        if (child>0) {
            if (checkif_foreground(child)) {
                counter--;
            }
            else {
                // set info in background_info table
                background_info[current_zombies_number].pid=child;
                background_info[current_zombies_number++].stat_val=stat_val;
            }
        }
    } while ( child>0 );
}


void sigint_handler(int sig) {
    /*struct sigaction act;
    act.sa_handler=SIG_IGN;
    act.sa_flags=0;
    sigemptyset(&act.sa_mask);

    struct sigaction oact;
    sigaction(SIGINT, &act ,&oact); // not sure if necessary*/
    int i;
    for (i=0; i<counter; ++i)
        kill(foreground_processes[i], SIGINT);

    struct stat infoPrompt;
    fstat(0, &infoPrompt);
    if (S_ISCHR(infoPrompt.st_mode))
        write(1, "\n", 1);
    //sigaction(SIGINT, &oact, NULL);
}
