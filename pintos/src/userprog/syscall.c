#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

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

static void
check_esp(void *esp) {
  if (esp == NULL || !is_user_vaddr(esp) ||
      pagedir_get_page (thread_current()->pagedir, esp) == NULL) {
    printf("%s: exit(%d)\n", thread_name(), -1);
    thread_current()->exit_status = -1;
    thread_exit ();
  }
}

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f)
{
  printf ("system call!\n");
  check_esp(f->esp);
  uint32_t *esp = (uint32_t *)f->esp;

  switch (*esp) {
    // Processs Control
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      check_esp(esp + 1);
      exit (*(esp + 1))
      break;
    case SYS_EXEC:
      check_esp(esp + 1);
      check_esp(*(esp + 1));                          // may need to cast
      f->eax = exec(*(esp + 1));
      break;
    case SYS_WAIT:
      check_esp(esp + 1);
      f->eax = wait (*(esp + 1))
      break;
    // File System
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
  }
  thread_exit ();
}


void
halt (void){
  shutdown_power_off();
}

void
exit (int status) {
  printf("%s: exit(%d)\n", thread_name(), status);
  thread_current()->exit_status = status;
  thread_exit();
}

pid_t
exec (const char *cmd_line){
  tid_t tid;

  tid = process_execute(cmd_line);

  return tid;
}

int
wait (pid_t pid) {
  return process_wait(pid);
}
