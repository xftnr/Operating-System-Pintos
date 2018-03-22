/*
* Authors: Yige Wang, Pengdi Xia, Peijie Yang, Wei Po Chen
* Date: 03/21/2018
* Description: Whenever a user process wants to access some
* kernel functionality, it invokes a system call.
*/
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "devices/shutdown.h"

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

struct lock file_lock;

/* Check stak pointer
* If the user provides an invalid pointer, a pointer into kernel memory,
* or a block partially in one of those regions, terminate the process.
*/
static void
check_esp(void *esp) {
  if (esp == NULL || !is_user_vaddr(esp) ||
      pagedir_get_page (thread_current()->pagedir, esp) == NULL) {
    printf("%s: exit(%d)\n", thread_name(), -1);
    thread_current()->exit_status = -1;
    thread_exit ();
  }
}

/* Get the struct file_info with the file descriptor */
static struct file_info *
get_file(int fd) {
  struct list_elem *e = NULL;
  struct file_info *cur = NULL;
  for (e = list_begin (&thread_current()->file_list);
        e!= list_end (&thread_current()->file_list); e = list_next (e)) {
    cur = list_entry (e, struct file_info, file_elem);
    if (cur->fd == fd) {
      return cur;
    }
  }
  return NULL;
}

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&file_lock);
}

static void
syscall_handler (struct intr_frame *f)
{
  check_esp((void *)(f->esp));
  uint32_t *esp = (uint32_t *)f->esp;

  switch (*esp) {
    // Processs Control
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      check_esp((void *)(esp + 1));
      exit((int)*(esp + 1));
      break;
    case SYS_EXEC:
      check_esp((void *)(esp + 1));
      check_esp((void *)*(esp + 1));
      f->eax = exec((const char *)*(esp + 1));
      break;
    case SYS_WAIT:
      check_esp((void *)(esp + 1));
      f->eax = wait ((pid_t)*(esp + 1));
      break;
    // File System
    case SYS_CREATE:
      check_esp((void *)(esp + 1));
      check_esp((void *)*(esp + 1));
      check_esp((void *)(esp + 2));
      f->eax = create((const char *)*(esp + 1), (unsigned)*(esp + 2));
      break;
    case SYS_REMOVE:
      check_esp((void *)(esp + 1));
      check_esp((void *)*(esp + 1));
      f->eax = remove((const char *)*(esp + 1));
      break;
    case SYS_OPEN:
      check_esp((void *)(esp + 1));
      check_esp((void *)*(esp + 1));
      f->eax = open((const char *)*(esp + 1));
      break;
    case SYS_FILESIZE:
      check_esp((void *)(esp + 1));
      f->eax = filesize((int)*(esp + 1));
      break;
    case SYS_READ:
      check_esp((void *)(esp + 1));
      check_esp((void *)(esp + 2));
      check_esp((void *)*(esp + 2));
      check_esp((void *)(esp + 3));
      f->eax = read ((int)*(esp + 1), (void *)*(esp + 2), (unsigned)*(esp + 3));
      break;
    case SYS_WRITE:
      check_esp((void *)(esp + 1));
      check_esp((void *)(esp + 2));
      check_esp((void *)*(esp + 2));
      check_esp((void *)(esp + 3));
      f->eax = write ((int)*(esp + 1), (const void *)*(esp + 2), (unsigned)*(esp + 3));
      break;
    case SYS_SEEK:
      check_esp((void *)(esp + 1));
      check_esp((void *)(esp + 2));
      seek ((int)*(esp + 1), (unsigned)*(esp + 2));
      break;
    case SYS_TELL:
      check_esp((void *)(esp + 1));
      f->eax = tell((int)*(esp + 1));
      break;
    case SYS_CLOSE:
      check_esp((void *)(esp + 1));
      close((int)*(esp + 1));
      break;
    default:
      printf("System Call not implemented.\n");
  }
}

/*Terminates Pintos by calling shutdown_power_off()*/
void
halt (void){
  shutdown_power_off();
}


/*erminates the current user program, returning status to the kernel*/
void
exit (int status) {
  printf("%s: exit(%d)\n", thread_name(), status);
  thread_current()->exit_status = status;
  thread_exit();
}

/*Runs the executable whose name is given in cmd_line,
 *passing any given arguments, and returns the new process's program id (pid)*/
pid_t
exec (const char *cmd_line){
  lock_acquire(&file_lock);

  tid_t tid;
  tid = process_execute(cmd_line);
  lock_release(&file_lock);

  return tid;
}

/*Waits for a child process pid and retrieves the child's exit status.*/
int
wait (pid_t pid) {
  return process_wait(pid);
}

/*Writes size bytes from buffer to the open file fd.
*Returns the number of bytes actually written,
*which may be less than size if some bytes could not be written.*/
int
write (int fd, const void *buffer, unsigned size) {
  lock_acquire(&file_lock);
  if (fd == 1) {
    putbuf (buffer, size);
    lock_release(&file_lock);

    return size;
  } else if (fd != 0) {

    struct file_info *cur_info = get_file(fd);
    if (cur_info == NULL) {
      lock_release(&file_lock);
      exit(-1);
    }

    struct file *cur = cur_info->file_temp;

    int result = file_write (cur, buffer, size);
    lock_release(&file_lock);

    return result;
  }
  exit(-1);
}

/*Creates a new file called file initially initial_size bytes in size.
*Returns true if successful, false otherwise. */
bool
create (const char *file, unsigned initial_size) {
  lock_acquire(&file_lock);
  bool result = filesys_create(file, initial_size);
  lock_release(&file_lock);
  return result;
}

/*Deletes the file called file. Returns true if successful, false otherwise.*/
bool
remove (const char *file) {
  lock_acquire(&file_lock);
  bool result = filesys_remove(file);
  lock_release(&file_lock);
  return result;
}

/*Opens the file called file.
*Returns a nonnegative integer handle called a "file descriptor" (fd) or -1
*if the file could not be opened.*/
int
open (const char *file){
  lock_acquire(&file_lock);
  struct file *f = filesys_open(file);

  if (f == NULL) {
    lock_release(&file_lock);
    return -1;
  }
  struct file_info *f_i = (struct file_info*)malloc(sizeof(struct file_info));
  f_i->file_temp = f;
  f_i->fd = thread_current()->fd;
  thread_current()->fd++;

  list_push_back (&thread_current()->file_list, &f_i->file_elem);
  lock_release(&file_lock);
  return f_i->fd;
}

/*Returns the size, in bytes, of the file open as fd.*/
int
filesize (int fd) {
  lock_acquire(&file_lock);

  struct file_info *cur_info = get_file(fd);
  if (cur_info == NULL) {
    lock_release(&file_lock);
    exit(-1);
  }
  struct file *cur = cur_info->file_temp;

  int result = file_length(cur);
  lock_release(&file_lock);
  return result;
}

/*Reads size bytes from the file open as fd into buffer.
*Returns the number of bytes actually read (0 at end of file), or -1
*if the file could not be read (due to a condition other than end of file).*/
int
read (int fd, void *buffer, unsigned size) {
  lock_acquire(&file_lock);

  struct file_info *cur_info = get_file(fd);
  if (cur_info == NULL) {
    lock_release(&file_lock);
    exit(-1);
  }
  struct file *cur = cur_info->file_temp;

  int result = file_read (cur, buffer, size);
  lock_release(&file_lock);
  return result;
}

/*Changes the next byte to be read or written in open file fd to position,
*expressed in bytes from the beginning of the file.*/
void
seek (int fd, unsigned position) {
  lock_acquire(&file_lock);

  struct file_info *cur_info = get_file(fd);
  if (cur_info == NULL) {
    lock_release(&file_lock);
    exit(-1);
  }
  struct file *cur = cur_info->file_temp;

  file_seek (cur, position);
  lock_release(&file_lock);
}

/*Returns the position of the next byte to be read or written in open file fd,
*expressed in bytes from the beginning of the file.*/
unsigned
tell (int fd) {
  lock_acquire(&file_lock);

  struct file_info *cur_info = get_file(fd);
  if (cur_info == NULL) {
    lock_release(&file_lock);
    exit(-1);
  }
  struct file *cur = cur_info->file_temp;

  unsigned result = file_tell (cur);
  lock_release(&file_lock);
  return result;
}

/*Closes file descriptor fd.
*Exiting or terminating a process implicitly closes all its open file
*descriptors, as if by calling this function for each one.*/
void
close (int fd) {
  lock_acquire(&file_lock);

  struct file_info *cur_info = get_file(fd);
  if (cur_info == NULL) {
    lock_release(&file_lock);
    exit(-1);
  }
  struct file *cur = cur_info->file_temp;
  list_remove(&cur_info->file_elem);

  file_close (cur);
  free(cur_info);

  lock_release(&file_lock);
}

//what is this?????????????
void
close_file (struct file *file) {
  lock_acquire(&file_lock);

  file_close (file);

  lock_release(&file_lock);
}
