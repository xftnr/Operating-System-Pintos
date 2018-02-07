/*
* Authors: Yige Wang, Pengdi Xia
* Date: 1/27/2018
* Description: Handles SIGINT and SIGUSER1 sent to a running process.
*/

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include "util.h"


/*
 * Function: handler
 * -----------------
 * Handles two kind of the signal: SIGINT and SIGUSER1
 * Writes "Nice try." to standart output when SIGINT is received.
 * Writes "exiting" to standart output and exits when SIGUSER1 is received.
 *
 * sig: the signal received by this process
 *
*/

// Yige Driving
void handler(int sig)
{
    ssize_t bytes;          /* number of bytes written  */
    const int STDOUT = 1;   /* standard output */

    if (sig == SIGINT) {        // Handles SIGINT
        bytes = write(STDOUT, "Nice try.\n", 10);
        if(bytes != 10)
            exit(-1);
    } else if (sig == SIGUSR1) {        // Handles SIGUSER1
        bytes = write(STDOUT, "exiting\n", 8);
        if(bytes != 8)
            exit(-1);
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
    pid_t pid;              /* Process ID */
    struct timespec tim, rem;    /* Time to sleep */

    if(argc != 1){
        fprintf(stderr, "Usage: ./handle\n");
        exit(-1);
    }

    // let handler handle both SIGINT and SIGUSER1
    Signal(SIGINT, handler);
    Signal(SIGUSR1, handler);

    pid = getpid();
    printf("%d\n", pid);
    while(1) {
        // sleep for 1 second before printing "Still here"
        tim.tv_sec  = 1;
        tim.tv_nsec = 0;
        while (nanosleep(&tim, &rem) == -1) {
            // sleep interrupted by a signal, only sleep the time remaining
            tim = rem;
        }
        printf("Still here\n");
    }
    return 0;
}
