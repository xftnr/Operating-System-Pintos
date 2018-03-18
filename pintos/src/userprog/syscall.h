#include <list.h>


#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);


struct file_info
{
    int fd;                            /* File descriptor. */
    struct file *file_temp;                  /* Associated file. */
    struct list_elem file_elem;        /* List element for file list. */
};

#endif /* userprog/syscall.h */

/* Process identifier. */
typedef int pid_t;
