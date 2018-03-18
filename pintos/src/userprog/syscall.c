#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <list.h>
#include "filesys/file.h"
#include "filesys/filesys.h"


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

struct file_info
{
    int fd;                            /* File descriptor. */
    struct file *file_temp;                  /* Associated file. */
    struct list_elem file_elem;        /* List element for file list. */
};


static void
check_esp(void *esp) {
  if (esp == NULL || !is_user_vaddr(esp) ||
      pagedir_get_page (thread_current()->pagedir, esp) == NULL) {
    printf("%s: exit(%d)\n", thread_name(), -1);
    thread_current()->exit_status = -1;
    thread_exit ();
  }
}

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
  check_esp(f->esp);
  uint32_t *esp = (uint32_t *)f->esp;

  switch (*esp) {
    // Processs Control
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      check_esp(esp + 1);
      exit(*(esp + 1));
      break;
    case SYS_EXEC:
      check_esp(esp + 1);
      check_esp(*(esp + 1));                          // may need to cast
      f->eax = exec(*(esp + 1));
      break;
    case SYS_WAIT:
      check_esp(esp + 1);
      f->eax = wait (*(esp + 1));
      break;
    // File System
    case SYS_CREATE:
      check_esp(esp + 1);
      check_esp(*(esp + 1));
      check_esp(esp + 2);
      f->eax = create(*(esp + 1), *(esp + 2));
      break;
    case SYS_REMOVE:
      check_esp(esp + 1);
      check_esp(*(esp + 1));
      f->eax = remove(*(esp + 1));
      break;
    case SYS_OPEN:
      check_esp(esp + 1);
      check_esp(*(esp + 1));
      f->eax = open(*(esp + 1));
      break;
    case SYS_FILESIZE:
      check_esp(esp + 1);
      f->eax = filesize(*(esp + 1));
      break;
    case SYS_READ:
      check_esp(esp + 1);
      check_esp(esp + 2);
      check_esp(*(esp + 2));
      check_esp(esp + 3);
      f->eax = read (*(esp + 1), *(esp + 2), *(esp + 3));
      break;
    case SYS_WRITE:
      check_esp(esp + 1);
      check_esp(esp + 2);
      check_esp(*(esp + 2));
      check_esp(esp + 3);
      f->eax = write (*(esp + 1), *(esp + 2), *(esp + 3));
      break;
    case SYS_SEEK:
      check_esp(esp + 1);
      check_esp(esp + 2);
      seek (*(esp + 1), *(esp + 2));
      break;
    case SYS_TELL:
      check_esp(esp + 1);
      f->eax = tell(*(esp + 1));
      break;
    case SYS_CLOSE:
      check_esp(esp + 1);
      close(*(esp + 1));
      break;
    default:
      printf("System Call not implemented.\n");
  }
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

int
write (int fd, const void *buffer, unsigned size) {
  if (fd == 1) {
    putbuf (buffer, size);
    return size;
  } else if (fd != 0) {
    lock_acquire(&file_lock);

    struct file_info *cur_info = get_file(fd);
    if (cur_info == NULL) {
      exit(-1);
    }

    struct file *cur = cur_info->file_temp;

    int result = file_write (cur, buffer, size);
    lock_release(&file_lock);

    return result;
  }
  exit(-1);
}

bool
create (const char *file, unsigned initial_size) {
  lock_acquire(&file_lock);
  bool result = filesys_create(file, initial_size);
  lock_release(&file_lock);
  return result;
}

bool
remove (const char *file) {
  lock_acquire(&file_lock);
  bool result = filesys_remove(file);
  lock_release(&file_lock);
  return result;
}

int
open (const char *file){
  lock_acquire(&file_lock);
  struct file_info *f = malloc(sizeof(struct file_info));
  f->file_temp = filesys_open(file);
  if (f->file_temp == NULL) {
    free(f);
    return -1;
  }

  f->fd = thread_current()->fd;
  thread_current()->fd++;

  list_push_back (&thread_current()->file_list, &f->file_elem);
  lock_release(&file_lock);
  return f->fd;
}

int
filesize (int fd) {

  lock_acquire(&file_lock);

  struct file_info *cur_info = get_file(fd);
  if (cur_info == NULL) {
    exit(-1);
  }
  struct file *cur = cur_info->file_temp;

  int result = file_length(cur);
  lock_release(&file_lock);
  return result;
}

int
read (int fd, void *buffer, unsigned size) {
  lock_acquire(&file_lock);

  struct file_info *cur_info = get_file(fd);
  if (cur_info == NULL) {
    exit(-1);
  }
  struct file *cur = cur_info->file_temp;

  int result = file_read (cur, buffer, size);
  lock_release(&file_lock);
  return result;
}

void
seek (int fd, unsigned position) {
  lock_acquire(&file_lock);

  struct file_info *cur_info = get_file(fd);
  if (cur_info == NULL) {
    exit(-1);
  }
  struct file *cur = cur_info->file_temp;

  file_seek (cur, position);
  lock_release(&file_lock);
}

unsigned
tell (int fd) {
  lock_acquire(&file_lock);

  struct file_info *cur_info = get_file(fd);
  if (cur_info == NULL) {
    exit(-1);
  }
  struct file *cur = cur_info->file_temp;

  unsigned result = file_tell (cur);
  lock_release(&file_lock);
  return result;
}

void
close (int fd) {
  lock_acquire(&file_lock);

  struct file_info *cur_info = get_file(fd);
  if (cur_info == NULL) {
    exit(-1);
  }
  struct file *cur = cur_info->file_temp;
  list_remove(&cur_info->file_elem);
  free(cur_info);

  file_close (cur);
  lock_release(&file_lock);
}
