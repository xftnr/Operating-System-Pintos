/*
* Authors: Yige Wang, Pengdi Xia
* Date: 1/27/2018
* Description: Sends a signal to kill a process.
*/

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include "util.h"
#include <string.h>

/*
 *Function: mykill
 *-----------------
 *kill the running function by passby pid send the signal by kill.
 *
 *arge: >2
 *argV: commandline and contain the pid
 *
 */

// Pengdi Driving
int main(int argc, char **argv)
{
    pid_t pid;      /* Process ID */

    // check precondition
    if(argc != 2){
        unix_error("mykill: input error");
    }

    // send SIGUSER1 to the process with pid entered by user
    pid = atoi(argv[1]);
    if(kill(pid, SIGUSR1)==-1){
        unix_error("mykill: kill error");
    }
    return 0;
}
