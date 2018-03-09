#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);
static void halt (void);
static void exit (int status);
static pid_t exec (const char *cmd_line);
static int wait (pid_t pid);
static bool create (const char *file, unsigned initial_size);
static bool remove (const char *file);
static int open (const char *file);
static int filesize (int fd);
static int read (int fd, void *buffer, unsigned size);
static int write (int fd, const void *buffer, unsigned size);
static void seek (int fd, unsigned position);
static unsigned tell (int fd);
static void close (int fd);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f)
{
  printf ("system call!\n");
  unit32_t esptemp = &f->esp;


  // check whether is valid pointer
  // on both esptemp and esptemp++
  // if ()



  switch (esptemp) {
    //the exec and wait
    case SYS_EXEC:
      f->eax = exec((const char*) esptemp+1);
      break;
    case SYS_WAIT:
      break;
    //the the file operating
    case SYS_CREATE:
      break;
    case SYS_REMOVE:
      break;
    case SYS_OPEN:
      break;
    case SYS_FILESIZE:
      break;
    case SYS_READ:
      break;
    case SYS_WRITE:
      break;
    case SYS_SEEK:
      break;
    case SYS_TELL:
      break;
    case SYS_CLOSE:
      break;
    //other sys call
    case SYS_HALT:
      break;
    case SYS_EXIT:
      break;
  }
  thread_exit ();
}

pid_t exec (const char *cmd_line){
  tid_t tid;
  struct thread *cur = thread_current ();
  &cur->child_load=0;
  tid=process_execute(cmd_line);
  lock_acquire(&cur->child_lock);
  while(&cur->child_load==0){
    cond_wait(&cur->childCV);
  }
  if(&cur->child_load==-1){
    tid=-1;
  }
  lock_release(&cur->child_lock);
  return tid;
}
