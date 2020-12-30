#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <list.h>
void syscall_init (void);
void sys_exit (int status);
struct pcb *find_pcb_by_tid (int tid);
#endif /* userprog/syscall.h */
