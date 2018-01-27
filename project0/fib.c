/*
 * Authors: Yige Wang, Pengdi Xia
 * Date: 1/27/2018
 * Description: ...
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
 * ??? parameter. returned value
 */

static void doFib(int n, int doPrint)
{
   // Yige driving
   if(n < 2)
      printf("%d\n", n);
   else{
      int a=0;
      int b=1;
      int tmp;
      pid_t fork_val = fork();


      if (fork_val < 0){
         fprintf(stderr, "fork error: %s/n", strerror(errno));
	     exit(1);
      } else if(fork_val == 0){ //child
      // Pengdi Driving
         tmp=b;
         b+=a;
         a=tmp;
         doFib(n-1,doPrint);
         exit(b);
      } else{ //parent
   int *status=NULL;
         int result = WEXITSTATUS(status);
      printf("%d\n",result );
      }


	}



}
