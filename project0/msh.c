/*
* msh - A mini shell program with job control
*
* <Put your name and login ID here>
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
#include "util.h"
#include "jobs.h"


/* Global variables */
int verbose = 0;            /* if true, print additional output */

extern char **environ;      /* defined in libc */
static char prompt[] = "msh> ";    /* command line prompt (DO NOT CHANGE) */
static struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */


/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
void usage(void);
void sigquit_handler(int sig);



/*
* main - The shell's main routine
*/
int main(int argc, char **argv)
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
    * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
            case 'h':             /* print help message */
            usage();
            break;
            case 'v':             /* emit additional diagnostic info */
            verbose = 1;
            break;
            case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
            break;
            default:
            usage();
        }
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler);

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1) {

        /* Read command line */
        if (emit_prompt) {
            printf("%s", prompt);
            fflush(stdout);
        }
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
        app_error("fgets error");
        if (feof(stdin)) { /* End of file (ctrl-d) */
            fflush(stdout);
            exit(0);
        }

        /* Evaluate the command line */
        eval(cmdline);
        fflush(stdout);
        fflush(stdout);
    }

    exit(0); /* control never reaches here */
}

/*
* eval - Evaluate the command line that the user has just typed in
*
* If the user has requested a built-in command (quit, jobs, bg or fg)
* then execute it immediately. Otherwise, fork a child process and
* run the job in the context of the child. If the job is running in
* the foreground, wait for it to terminate and then return.  Note:
* each child process must have a unique process group ID so that our
* background children don't receive SIGINT (SIGTSTP) from the kernel
* when we type ctrl-c (ctrl-z) at the keyboard.
*/

// Yige driving
void eval(char *cmdline)//add job plz
{

    char *argv[MAXARGS];  /* Argument list execve() */
    char buf[MAXLINE];    /* Holds modified command line */
    int bg;               /* Should the job run in bg or fg? */
    pid_t pid;            /* Process ID */
    sigset_t mask, prevmask;        /* Stores blocked signals */

    strcpy(buf, cmdline);
    bg = parseline(buf, argv);
    if (argv[0] == NULL)
    return;            /* Ignore empty lines */

    if (!builtin_cmd(argv)) {
        /* Not a built-in command, load and run the executable program */
        // block SIGCHLD to prevent race conditions
        if (sigemptyset(&mask) < 0) {
            unix_error("eval: sigemptyset error");
        }
        if (sigaddset(&mask,SIGCHLD) < 0) {
            unix_error("eval: sigaddset error");
        }
        if (sigprocmask(SIG_BLOCK,&mask, &prevmask) < 0 ) {
            unix_error("eval: sigprocmask error");
        }
        // fork a child process to execute the program
        pid = fork();
        if (pid < 0) {
            unix_error("eval: fork error");
        } else if (pid == 0) {
            /* Child execute program */
            // Set up gpid and unblock SIGCHLD before executing
            setpgid(pid, pid);
            if (sigprocmask(SIG_UNBLOCK,&mask, &prevmask) < 0 ) {
                unix_error("eval: sigprocmask error");
            }
            if (execve(argv[0], argv, environ) < 0) {
                printf("%s: Command not found\n", argv[0]);
                exit(0);
            }
        } else {        /* Parent process*/
            if (!bg) {
                /* Waits for foreground process to finish */
                addjob(jobs, pid, FG, cmdline);
                if (sigprocmask(SIG_UNBLOCK,&mask, &prevmask) < 0 ) {
                    unix_error("eval: sigprocmask error");
                }
                waitfg(pid);
            } else {
                /* Prints a confirmation message for background process*/
                addjob(jobs, pid, BG, cmdline);
                printf("[%d] (%d) %s",pid2jid(jobs, pid), pid, cmdline);
                if (sigprocmask(SIG_UNBLOCK,&mask, &prevmask) < 0 ) {
                    unix_error("eval: sigprocmask error");
                }
            }
        }
    }
    return;
}


/*
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.
 * Return 1 if a builtin command was executed; return 0
 * if the argument passed in is *not* a builtin command.
 */
int builtin_cmd(char **argv)
{
    sigset_t mask_all, prev_all;        /* Stores blocked signals */
    if (!strcmp(argv[0], "quit")) {      /* quit command */
        exit(0);
    }
    if (!strcmp(argv[0], "fg")) {        /* fg command */
        do_bgfg(argv);
        return 1;
    }
    if (!strcmp(argv[0], "bg")) {        /* bg command */
        do_bgfg(argv);
        return 1;
    }
    sigfillset(&mask_all);      //Blocks all signals before accessing jobs
    if (!strcmp(argv[0], "jobs")) {      /* jobs command */
        sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
        listjobs(jobs);
        sigprocmask(SIG_UNBLOCK, &mask_all, &prev_all);
        return 1;
    }
    if (!strcmp(argv[0], "&")) {        /* Ignore singleton */
        return 1;
    }
    return 0;     /* not a builtin command */
}


/*
 * do_bgfg - Execute the builtin bg and fg commands
 */
// Pengdi driving
void do_bgfg(char **argv)
{
    if (argv[1] == NULL) {
        printf("%s command requires PID or %%jobid argument\n", argv[0]);
        return;
    }
    sigset_t mask_all, prev_all;        /* Stores blocked signals */
    int jid;
    pid_t pid;
    struct job_t *currjob = NULL;

    if (argv[1][0] == '%') {      // Access job through jid
        // Checks wheather jid is a number
        if (atoi(&argv[1][1]) == 0) {
            printf("%s: argument must be a PID or %%jobid\n", argv[0]);
            return;
        }
        jid = atoi(&argv[1][1]);
        // Checks wheather such job exists
        if (getjobjid(jobs,jid) == NULL) {
            printf("%%%d: No such job\n", jid);
            return;
        }
        currjob = getjobjid(jobs, jid);
    } else {        // Access job through pid
        // Checks wheather pid is a number
        if (atoi(argv[1]) == 0) {
            printf("%s: argument must be a PID or %%jobid\n", argv[0]);
            return;
        }
        pid = atoi(argv[1]);
        // Checks wheather such process exists
        if (getjobpid(jobs,pid) == NULL) {
            printf("(%d): No such process\n", pid);
            return;
        }
        currjob = getjobpid(jobs, pid);
    }

    // Block signals before modefying jobs
    sigfillset(&mask_all);
    sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
    if (!strcmp(argv[0], "bg")) {       /* bg command */
        kill(-(currjob->pid), SIGCONT);
        currjob->state = BG;
        sigprocmask(SIG_UNBLOCK, &mask_all, &prev_all);
        printf("[%d] (%d) %s",currjob->jid, currjob->pid, currjob->cmdline);
    } else {        /* fg command */
        kill(-(currjob->pid), SIGCONT);
        currjob->state = FG;
        sigprocmask(SIG_UNBLOCK, &mask_all, &prev_all);
        waitfg(currjob->pid);
    }
    return;
}

/*
* waitfg - Block until process pid is no longer the foreground process
*/
void waitfg(pid_t pid)
{
    sigset_t mask, prevmask;        /* Stores blocked signals */
    sigemptyset(&mask);
    sigaddset(&mask,SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &prevmask);
    // Suspends shell until forground process has changed
    while(fgpid(jobs)==pid) {
        sigsuspend(&prevmask);
    }
    sigprocmask(SIG_UNBLOCK, &mask, &prevmask);
    return;
}

/*****************
* Signal handlers
*****************/

/*
* sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
*     a child job terminates (becomes a zombie), or stops because it
*     received a SIGSTOP or SIGTSTP signal. The handler reaps all
*     available zombie children, but doesn't wait for any other
*     currently running children to terminate.
*/
void sigchld_handler(int sig)
{
    int status;         /* Status of process */
    pid_t pid;          /* Process ID */
    char str[1024];       /* Stores message to print */
    ssize_t bytes;      /* Number of bytes written */
    const int STDOUT = 1;

    while ((pid = waitpid(-1, &status, WNOHANG|WUNTRACED)) > 0) {
        /* Child process has terminated or stopped */
        struct job_t *job = getjobpid(jobs, pid);
        if (WIFSIGNALED(status)) {  /* child terminated by uncaught signal */
            sprintf(str, "Job [%d] (%d) terminated by signal %d\n", job->jid, job->pid, WTERMSIG(status));
            bytes = write(STDOUT, str, strlen(str));
            if (bytes != strlen(str)) {
                exit(-999);
            }
            deletejob(jobs, job->pid);
        } else if (WIFEXITED(status)) {     /* Child terminated normally */
            deletejob(jobs, job->pid);
        } else if (WIFSTOPPED(status)) {        /* Child stopped by signal */
            sprintf(str, "Job [%d] (%d) stopped by signal %d\n", job->jid, job->pid, WSTOPSIG(status));
            bytes = write(STDOUT, str, strlen(str));
            if (bytes != strlen(str)) {
                exit(-999);
            }
            job->state = ST;
        }
    }
    return;
}

/*
* sigint_handler - The kernel sends a SIGINT to the shell whenver the
*    user types ctrl-c at the keyboard.  Catch it and send it along
*    to the foreground job.
*/
// Yige driving
void sigint_handler(int sig)
{
    if (fgpid(jobs) == 0) { //no foreground job, do nothing
        return;
    }
    // send the signal to the process group containing the foreground job
    if (kill(-fgpid(jobs), sig) < 0) {
        unix_error("sigint_handler: kill error");
    }
    // kill(-fgpid(jobs), sig);
    return;
}

/*
* sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
*     the user types ctrl-z at the keyboard. Catch it and suspend the
*     foreground job by sending it a SIGTSTP.
*/
void sigtstp_handler(int sig)
{
    if (fgpid(jobs) == 0) { //no foreground job, do nothing
        return;
    }
    // send the signal to the process group containing the foreground job
    if (kill(-fgpid(jobs), sig) < 0) {
        unix_error("sigtstp_handler: kill error");
    }
    // kill(-fgpid(jobs), sig);
    return;
}

/*********************
 * End signal handlers
 *********************/



/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void)
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig)
{
    ssize_t bytes;
    const int STDOUT = 1;
    bytes = write(STDOUT, "Terminating after receipt of SIGQUIT signal\n", 45);
    if (bytes != 45)
       exit(-999);
    exit(1);
}
