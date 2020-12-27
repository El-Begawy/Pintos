#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <list.h>
void syscall_init (void);

struct file_descriptor {
    int fd;
    struct list_elem file_element;
    struct file* file;
};
#endif /* userprog/syscall.h */
