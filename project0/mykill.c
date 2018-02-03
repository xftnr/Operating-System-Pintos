#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#include <string.h>

// Pengdi Driving
int main(int argc, char **argv)
{
    if(argc != 2){
        fprintf(stderr, "Usage: ./mykill <num>\n");
        exit(-1);
    }
    pid_t pid;
    pid = atoi(argv[1]);
    if(kill(pid, SIGUSR1)==-1){
        fprintf(stderr, "kill error: %s\n", strerror(errno));
        exit(1);
    }
    return 0;
}
