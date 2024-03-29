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
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
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
static bool chdir (const char *dir);
static bool mkdir (const char *dir);
static bool readdir (int fd, char *name);
static bool isdir (int fd);
static int inumber (int fd);

/* Checks stack pointer
* If the user provides an invalid pointer, a pointer into kernel memory,
* or a block partially in one of those regions, terminate the process.
*/
// Yige driving
static void
check_esp(void *esp) {
  if (esp == NULL || !is_user_vaddr(esp) ||
      pagedir_get_page (thread_current()->pagedir, esp) == NULL) {
    printf("%s: exit(%d)\n", thread_name(), -1);
    thread_current()->exit_status = -1;
    thread_exit ();
  }
}

/* Returns the struct file_info containing open file fd
* by checking fd of all open files of current thread.
*/
// Yige driving
static struct file_info *
get_file(int fd) {
  struct list_elem *e = NULL;        /* List elements. */
  struct file_info *f_i = NULL;      /* File Information.*/
  for (e = list_begin (&thread_current()->file_list);
        e!= list_end (&thread_current()->file_list); e = list_next (e)) {
    f_i = list_entry (e, struct file_info, file_elem);
    if (f_i->fd == fd) {
      return f_i;
    }
  }
  return NULL;
}

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

// Pengdi driving
static void
syscall_handler (struct intr_frame *f)
{
  check_esp((void *)(f->esp));    /* Checks the pointer to syscall number. */
  uint32_t *esp = (uint32_t *)f->esp;

  switch (*esp) {
    // Processs Control
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      /* Checks the pointer to argument of syscall. */
      check_esp((void *)(esp + 1));
      exit((int)*(esp + 1));
      break;
    case SYS_EXEC:
      check_esp((void *)(esp + 1));
      /* Checks the argument(pointer) of syscall. */
      check_esp((void *)*(esp + 1));
      /* Stores the returned value to eax. */
      f->eax = exec((const char *)*(esp + 1));
      break;
    case SYS_WAIT:
      check_esp((void *)(esp + 1));
      f->eax = wait((pid_t)*(esp + 1));
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
      f->eax = read((int)*(esp + 1), (void *)*(esp + 2),
                    (unsigned)*(esp + 3));
      break;
    case SYS_WRITE:
      check_esp((void *)(esp + 1));
      check_esp((void *)(esp + 2));
      check_esp((void *)*(esp + 2));
      check_esp((void *)(esp + 3));
      f->eax = write((int)*(esp + 1), (const void *)*(esp + 2),
                     (unsigned)*(esp + 3));
      break;
    case SYS_SEEK:
      check_esp((void *)(esp + 1));
      check_esp((void *)(esp + 2));
      seek((int)*(esp + 1), (unsigned)*(esp + 2));
      break;
    case SYS_TELL:
      check_esp((void *)(esp + 1));
      f->eax = tell((int)*(esp + 1));
      break;
    case SYS_CLOSE:
      check_esp((void *)(esp + 1));
      close((int)*(esp + 1));
      break;
    // Directory
    case SYS_CHDIR:
      check_esp((void *)(esp + 1));
      check_esp((void *)*(esp + 1));
      f->eax = chdir((const char *)*(esp + 1));
      break;
    case SYS_MKDIR:
      check_esp((void *)(esp + 1));
      check_esp((void *)*(esp + 1));
      f->eax = mkdir((const char *)*(esp + 1));
      break;
    case SYS_READDIR:
      check_esp((void *)(esp + 1));
      check_esp((void *)(esp + 2));
      check_esp((void *)*(esp + 2));
      f->eax = readdir((int)*(esp + 1), (const void *)*(esp + 2));
      break;
    case SYS_ISDIR:
      check_esp((void *)(esp + 1));
      f->eax = isdir((int)*(esp + 1));
      break;
    case SYS_INUMBER:
      check_esp((void *)(esp + 1));
      f->eax = inumber((int)*(esp + 1));
      break;
    default:
      printf("System Call not implemented.\n");
  }
}

/* Terminates Pintos by calling shutdown_power_off().
 */
// Peijie driving
void
halt (void){
  shutdown_power_off();
}

/* Updates exit_status so its parent can know its exit status,
*  print process's exit code then terminates the process.
*/
void
exit (int status) {
  printf("%s: exit(%d)\n", thread_name(), status);
  thread_current()->exit_status = status;
  thread_exit();
}

/* Runs the executable whose name is given in cmd_line,
* passing any given arguments, and returns the new process's program id.
*/
// Pengdi driving
pid_t
exec (const char *cmd_line){
  tid_t tid;
  tid = process_execute(cmd_line);
  return tid;
}

/* Waits for a child process pid and retrieves the child's exit status.
*/
// Peijie driving
int
wait (pid_t pid) {
  return process_wait(pid);
}

/* Writes size bytes from buffer to the open file fd.
* For fd 1, writes to standard output.
* Returns the number of bytes actually written,
* which may be less than size if some bytes could not be written.
*/
// Wei Po driving
int
write (int fd, const void *buffer, unsigned size) {
      if (fd == 1) {
    /* Writes to standard output. */
    putbuf (buffer, size);
        return size;
  } else if (fd != 0){
    /* Writes to open file fd. */
    struct file_info *cur_info = get_file(fd);
    if (cur_info == NULL) {
      /* No such open file fd for current process. */
            exit(-1);
    }
    struct file *cur = cur_info->file_temp;
    /* Cannot write to directory. */
    if (inode_isdir(file_get_inode(cur))) {
            return -1;
    }
    int result = file_write (cur, buffer, size);
        return result;
  } else {  /* Never writes to standard input. */
    exit(-1);
  }
}

/* Creates a new file called file initially initial_size bytes in size.
* Returns true if successful, false otherwise.
*/
// Yige driving
bool
create (const char *file, unsigned initial_size) {
      /* Create an inode with isdir as false. */
  bool result = filesys_create(file, initial_size, false);
    return result;
}

/* Deletes the file called file.
* Returns true if successful, false otherwise.
*/
bool
remove (const char *file) {
      bool result = filesys_remove(file);
    return result;
}

/* Opens the file called file.
* Returns a nonnegative integer handle called a "file descriptor" (fd)
* or -1 if the file could not be opened.
*/
int
open (const char *file){
  if (strlen(file) == 0) {
    return -1;
  }
      struct file *cur = filesys_open(file);
  if (cur == NULL) {
    /* No file named NAME exists, or an internal memory allocation fails. */
        return -1;
  }
  struct file_info *cur_info = malloc(sizeof(struct file_info));
  if (cur_info == NULL) {
    /* Malloc failed. */
        return -1;
  }
  cur_info->file_temp = cur;

  /* Add directory to file info of this file. */
  struct inode *inode = file_get_inode(cur);
  if (inode != NULL && inode_isdir(inode)) {
    cur_info->dir_temp = dir_open(inode_reopen(inode));
  } else {
    cur_info->dir_temp = NULL;
  }

  cur_info->fd = thread_current()->fd;
  thread_current()->fd++;      /* Updates next fd for open file. */
  /* Adds file to current thread's list of open files. */
  list_push_back (&thread_current()->file_list, &cur_info->file_elem);
    return cur_info->fd;
}

/* Returns the size, in bytes, of the file open as fd.
*/
// Wei Po driving
int
filesize (int fd) {
     struct file_info *cur_info = get_file(fd);
  if (cur_info == NULL) {
    /* No such open file fd for current process. */
        exit(-1);
  }

  struct file *cur = cur_info->file_temp;
  int result = file_length(cur);
    return result;
}

/* Reads size bytes from the file open as fd into buffer.
* Returns the number of bytes actually read (0 at end of file), or -1
* if the file could not be read (due to a condition other than end of file).
*/
int
read (int fd, void *buffer, unsigned size) {
     struct file_info *cur_info = get_file(fd);
  if (cur_info == NULL) {
    /* No such open file fd for current process. */
        exit(-1);
  }

  struct file *cur = cur_info->file_temp;
  int result = file_read (cur, buffer, size);
    return result;
}

/* Changes the next byte to be read or written in open file fd to position,
* expressed in bytes from the beginning of the file.
*/
void
seek (int fd, unsigned position) {
     struct file_info *cur_info = get_file(fd);
  if (cur_info == NULL) {
    /* No such open file fd for current process. */
        exit(-1);
  }

  struct file *cur = cur_info->file_temp;
  file_seek (cur, position);
  }

/* Returns the position of the next byte to be read or written
* in open file fd, expressed in bytes from the beginning of the file.
*/
unsigned
tell (int fd) {
     struct file_info *cur_info = get_file(fd);
  if (cur_info == NULL) {
    /* No such open file fd for current process. */
        exit(-1);
  }

  struct file *cur = cur_info->file_temp;
  unsigned result = file_tell (cur);
    return result;
}

/* Closes file descriptor fd. Remove file from file_list and frees resources.
*/
// Yige driving
void
close (int fd) {
     struct file_info *cur_info = get_file(fd);
  if (cur_info == NULL) {
    /* No such open file fd for current process. */
        exit(-1);
  }

  /* No longer a open file of current process. */
  list_remove(&cur_info->file_elem);

  file_close (cur_info->file_temp);

  /* Close directory if inode is a directory. */
  if (cur_info->dir_temp != NULL) {
    dir_close(cur_info->dir_temp);
  }

  free(cur_info);     /* Frees memory allocated. */
  }

/* Closes open file file.
* Used when exiting or terminating a process implicitly.
*/
void
close_file (struct file *file) {
      file_close (file);
  }

/* Changes the current working directory of the process to dir,
which may be relative or absolute.
Returns true if successful, false on failure.
*/
// Peijie Driving
bool
chdir (const char *dir) {
      bool result = filesys_chdir(dir);
    return result;
}

/* Creates the directory named dir, which may be relative or absolute.
Returns true if successful, false on failure.
Fails if dir already exists or if any directory name in dir,
besides the last, does not already exist.
*/
// Wei Po Driving
bool
mkdir (const char *dir) {
      bool result = filesys_create(dir, 0, true);
    return result;
}

/* Reads a directory entry from file descriptor fd,
which must represent a directory. If successful,
stores the null-terminated file name in name,
which must have room for READDIR_MAX_LEN + 1 bytes, and returns true.
If no entries are left in the directory, returns false.
*/
// Yige Driving
bool
readdir (int fd, char *name) {
     struct file_info *cur_info = get_file(fd);
  if (cur_info == NULL) {
    /* No such open file fd for current process. */
        return false;
  }

  struct file *cur = cur_info->file_temp;
  struct inode *inode = file_get_inode(cur);

  if(inode == NULL) {
        return false;
  }

  /* Must read a directory. */
  if (!inode_isdir(inode)) {
        return false;
  }

  /* Skip "." and ".." */
  bool result = dir_readdir(cur_info->dir_temp, name);
  if (strcmp(name, ".") == 0) {
    result = dir_readdir(cur_info->dir_temp, name);
    result = dir_readdir(cur_info->dir_temp, name);
  } else if (strcmp(name, "..") == 0) {
    result = dir_readdir(cur_info->dir_temp, name);
  }

  return result;
}

/* Returns true if fd represents a directory,
false if it represents an ordinary file.
*/
// Pengdi Driving
bool
isdir (int fd) {
  struct file_info *cur_info = get_file(fd);
  if (cur_info == NULL) {
    /* No such open file fd for current process. */
    exit(-1);
  }

  struct file *cur = cur_info->file_temp;
  struct inode *inode = file_get_inode(cur);
  bool result = inode_isdir(inode);

  return result;
}

/* Returns the inode number of the inode associated with fd,
which may represent an ordinary file or a directory.
*/
// Pengdi Driving
int
inumber (int fd) {
     struct file_info *cur_info = get_file(fd);
  if (cur_info == NULL) {
    /* No such open file fd for current process. */
        exit(-1);
  }

  struct file *cur = cur_info->file_temp;
  struct inode *inode = file_get_inode(cur);
  int result = (int)inode_get_inumber(inode);

  return result;
}
