/*
* Authors: Yige Wang, Pengdi Xia
* Date: 1/27/2018
* Description: Compute the fibonacci with fork() function
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
* compute the fibonacci by the recursive f(x)= f(x-1)+f(x)
* aternatively we use the fork() function by exit status
*
* n: index of the fibonacci
* doPrint: boolean to decide print the result
*
* returns: the nth fibonacci result.
*/

static void doFib(int n, int doPrint)
{
    // set of veriables
    pid_t pid1,pid2;        /* Process ID */
    int status1,status2;    /*status of child*/
    int result1,result2;    /*result child passed*/
    // Yige driving
    // base case when index is 0 or 1 the value is 0 and 1
    if(n < 2){
        if(doPrint){
            printf("%d\n", n);
        }
        //exit the the child status for recursive add
        else{
            exit(n);
        }
    }
    //general case
    else{
        pid1 = fork();
        if (pid1 < 0){
            unix_error("doFib: fork error");
        } else if(pid1 == 0){ //child
            // Pengdi Driving
            doFib(n-1,0);
        } else { //parent
            if(waitpid(pid1, &status1, 0)<0){
                unix_error("waitfg: waitpid error");
            }
            // store the first result
            result1 = WEXITSTATUS(status1);
            //parent create the second fork to check x-2
            pid2=fork();
            if (pid2 < 0){
                unix_error("doFib: fork error");
            } else if(pid2 == 0){ //child
                doFib(n-2,0);
            }
            if(waitpid(pid2, &status2, 0)<0){
                unix_error("waitfg: waitpid error");
            }
            //store the second result and do compute
            result2=WEXITSTATUS(status2);
            if(doPrint){
                printf("%d\n", result1+result2);
            }else{
                exit(result1+result2);
            }
        }
    }
}

