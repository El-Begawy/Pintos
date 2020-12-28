#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#define DEBUG_STACK 0

tid_t process_execute (const char *argv);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

struct child_process {
    int pid;
    struct list_elem child_elem;
};

#endif /* userprog/process.h */
