/*
* Authors: Yige Wang, Pengdi Xia
* Date: 1/27/2018
* Description: Compute the fibonacci number with fork() function
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

const int MAX = 13;

static void doFib(int n, int doPrint);


/*
* unix_error - unix-style error routine.
*/
inline static void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}


int main(int argc, char **argv)
{
    int arg;
    int print=1;

    if(argc != 2){
        fprintf(stderr, "Usage: fib <num>\n");
        exit(-1);
    }

    arg = atoi(argv[1]);
    if(arg < 0 || arg > MAX){
        fprintf(stderr, "number must be between 0 and %d\n", MAX);
        exit(-1);
    }

    doFib(arg, print);

    return 0;
}

/*
* Recursively compute the specified number. If print is
* true, print it. Otherwise, provide it to my parent process.
*
* NOTE: The solution must be recursive and it must fork
* a new child for each call. Each process should call
* doFib() exactly once.
*/

/*
* Function: doFib
* -----------------
* Compute the fibonacci recursively with f(x) = f(x-1) + f(x).
* fork() a process to calculate each value and return it as exit status.
* The parent process adds the two values of the children processse
* then either return its value to its parent or print the result.
*
* n: index of the fibonacci
* doPrint: wheather or not to print the result.
* true for original process and false for others.
*
* returns: the nth fibonacci result.
*/

static void doFib(int n, int doPrint)
{
    pid_t pid1,pid2;        /* Process ID */
    int status1,status2;    /* Status of children */
    int result1,result2;    /* Results from children */

    // Yige driving
    if (n < 2) {
        // Base case: when n is 0 or 1, return result directly
        if (doPrint) {
            printf("%d\n", n);
        } else {
            // Child process: exit the result to parent
            exit(n);
        }
    } else {
        // General case
        pid1 = fork();
        if (pid1 < 0) {
            unix_error("doFib: fork error");
        } else if (pid1 == 0) { // Child process to calculate f(n-1)
            // Pengdi Driving
            doFib(n-1,0);
        } else { // Parent process
            if(waitpid(pid1, &status1, 0) < 0){
                unix_error("waitfg: waitpid error");
            }
            // Stores f(n-1)
            result1 = WEXITSTATUS(status1);

            // Parent creates a second process to calculate f(n-2)
            pid2 = fork();
            if (pid2 < 0) {
                unix_error("doFib: fork error");
            } else if (pid2 == 0) { // Child process to calculate f(n-2)
                doFib(n-2,0);
            }
            if (waitpid(pid2, &status2, 0) < 0) {
                unix_error("waitfg: waitpid error");
            }
            // Stores f(n-2)
            result2 = WEXITSTATUS(status2);

            if (doPrint) {
                printf("%d\n", result1+result2);
            } else {
                exit(result1 + result2);
            }
        }
    }
}
