#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/thread.h"
#include "filesys/filesys.h"
#include <filesys/inode.h>

typedef struct fd_table_entry{
    int fd;
    struct file* address;
    struct list_elem elem;
};

struct lock filesys_lock;

void syscall_init (void);
void syscall_exit(int status);
tid_t syscall_exec(const char * cmdline);
void syscall_halt();
int syscall_practice(int i);
int syscall_wait(tid_t pid);
size_t syscall_write(int fd, const char* buff, size_t size);
int valid_ptr(void* ptr);

bool syscall_create(const char* file, size_t size);
bool syscall_remove(const char* file);
int syscall_open(const char* file);
int syscall_filesize(int fd);
int syscall_read(int fd, void *buffer, unsigned size);
void syscall_seek(int fd, unsigned position);
unsigned syscall_tell(int fd);
void syscall_close(int fd);
bool syscall_chdir(const char* dir_name);
bool syscall_mkdir(const char* dir_name);
bool syscall_readdir(int fd, const char* dir_name);
bool syscall_isdir(int fd);
block_sector_t syscall_inumber(int fd);

struct fd_table_entry* search_file(int fd);
#endif /* userprog/syscall.h */