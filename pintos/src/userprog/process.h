#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
typedef struct argument{
  char * argv[500];
  int argc;
};


typedef struct process_control_block{
    tid_t pid;
    int exit_status;
    int trying_to_exit;
    int zombie;
    int waited;
    struct argument* args;
    struct list_elem children_elem;
    struct thread* parent;
    struct semaphore wait_for_child_sema;
    struct semaphore execution_error_sema;
};

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
void get_argc_argv(char * file_name, struct argument * arguments);
void cpy_args_to_stack(struct argument* args, void** esp);
void set_up_pcb(struct process_control_block* pcb, struct argument* arguments);
struct process_control_block* get_child(tid_t tid);
void release_children(void);
#endif /* userprog/process.h */
