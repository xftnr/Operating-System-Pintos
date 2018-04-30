#include <list.h>
#include "filesys/file.h"

#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
void close_file (struct file *);

struct file_info
{
    int fd;                            /* File descriptor. */
    struct file *file_temp;            /* Associated file. */
    struct file *dir_temp;            /* Associated directory. */
    struct list_elem file_elem;        /* List element for file list. */
};

struct lock file_lock;        /* Synchronizea calls to file system. */

#endif /* userprog/syscall.h */

/* Process identifier. */
typedef int pid_t;
