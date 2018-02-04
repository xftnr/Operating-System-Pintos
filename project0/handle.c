#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include "util.h"


/*
 *Function: handler
 *-----------------
 *handle two kind of the signal: sigint and siguser1
 *sigint will not exit process only print nice try.
 *siguser1 will exit the process and print exiting.
 *
 *sig: send the signal
 *
*/

// Yige Driving
void handler(int sig)
{
    ssize_t bytes;
    const int STDOUT = 1;
    //handle sigint
    if(sig==SIGINT){
    /*the handler have to use the write function to print,
    *because the printf function will interrupted by the signals. */
    bytes = write(STDOUT, "Nice try.\n", 10);
    if(bytes != 10)
        exit(-999);
    }
    // Pengdi Driving
    else if(sig==SIGUSR1){
        bytes = write(STDOUT, "exiting\n", 8);
        if(bytes != 8)
            exit(-999);
        exit(1);
    }
}

/*
 * First, print out the process ID of this process.
 *
 * Then, set up the signal handler so that ^C causes
 * the program to print "Nice try.\n" and continue looping.
 *
 * Finally, loop forever, printing "Still here\n" once every
 * second.
 */
// Yige Driving
int main(int argc, char **argv)
{
    // set of variables
    pid_t pid;
    struct timespec tim;
    struct timespec rem;
    if(argc != 1){
        fprintf(stderr, "Usage: ./handle\n");
        exit(-1);
    }
    //send the signal
    Signal(SIGUSR1, handler);
    Signal(SIGINT, handler);
    pid = getpid();
    printf("%d\n", pid);
    while(1) {
        //set up the nanosleep by 1 second
        tim.tv_sec  = 1;
        tim.tv_nsec = 0;
        //when process interrupted in middle, starts from the remaining time.
        while(nanosleep(&tim, &rem)==-1){
            tim=rem;
        }
        printf("Still here\n");
    }
    return 0;
}
