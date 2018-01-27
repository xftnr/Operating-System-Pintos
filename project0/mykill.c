#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

// Pengdi Driving
int main(int argc, char **argv)
{
    if(argc != 2){
        fprintf(stderr, "Usage: ./mykill <num>\n");
        exit(-1);
    }
    pid_t pid;
    pid = atoi(argv[1]);
    kill(pid, SIGUSR1);
    return 0;
}
