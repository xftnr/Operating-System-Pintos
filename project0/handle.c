#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include "util.h"

// Yige Driving
void handler(int sig)
{
    ssize_t bytes;
    const int STDOUT = 1;
    if(sig==SIGINT){
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
    if(argc != 1){
        fprintf(stderr, "Usage: ./handle\n");
        exit(-1);
    }
    Signal(SIGUSR1, handler);
    Signal(SIGINT, handler);
    pid_t pid = getpid();

    ssize_t bytes;
    const int STDOUT = 1;
    printf("%d\n", pid);

    struct timespec tim;
    struct timespec rem;
    while(1) {
        tim.tv_sec  = 1;
        tim.tv_nsec = 0;
        while(nanosleep(&tim, &rem)==-1){
            tim=rem;
        }
        bytes = write(STDOUT, "Still here\n", 11);
        if(bytes != 11)
            exit(-999);
    }
    return 0;
}
