#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#define DEBUG_STACK 0

tid_t process_execute (const char *argv);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
struct pcb {
    tid_t pid;
    struct list_elem child_elem;
    int used;
    int exit_code;
    struct semaphore parent_waiting_sema;
};

#endif /* userprog/process.h */
