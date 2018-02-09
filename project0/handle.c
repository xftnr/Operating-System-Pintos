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
 * Function: sigint_handler
 * -----------------
 * Handles SIGINT
 * Writes "Nice try." to standart output when SIGINT is received.
 *
 * sig: the signal received by this process
 *
*/

// Yige Driving
void sigint_handler(int sig)
{
    ssize_t bytes;          /* number of bytes written  */
    const int STDOUT = 1;   /* standard output */
    bytes = write(STDOUT, "Nice try.\n", 10);
    if(bytes != 10)
        exit(-1);
}

/*
 * Function: siguser1_handler
 * -----------------
 * Handles SIGUSER1
 * Writes "exiting" to standart output and exits when SIGUSER1 is received.
 *
 * sig: the signal received by this process
 *
*/

// Pengdi Driving
void siguser1_handler(int sig)
{
    ssize_t bytes;          /* number of bytes written  */
    const int STDOUT = 1;   /* standard output */
    bytes = write(STDOUT, "exiting\n", 8);
    if(bytes != 8)
        exit(-1);
    exit(0);
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
    Signal(SIGINT, sigint_handler);
    Signal(SIGUSR1, siguser1_handler);

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
