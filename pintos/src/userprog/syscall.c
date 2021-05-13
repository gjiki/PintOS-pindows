#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "threads/malloc.h"
#include <string.h>

static void syscall_handler (struct intr_frame *);




void
syscall_init (void)
{
  lock_init(&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  uint32_t* args = ((uint32_t*) f->esp);
  //printf("System call number: %d\n", args[0]);
  if (!valid_ptr(args + 1)) 
  {
    f->eax = -1; 
    syscall_exit(-1);
    return;
  }

  if (args[0] == SYS_EXIT) 
  {
    f->eax = args[1];
    syscall_exit((int)args[1]);
    return;
  }

  /*
    syscall exec
  */
  if (args[0] == SYS_EXEC)
  {
    f -> eax = syscall_exec((const char *) args[1]);
    return;
  }

  /*
    syscall practice
  */
  if (args[0] == SYS_PRACTICE)
  {
    f-> eax = syscall_practice((int) args[1]);
    return;
  }
  
  /*
    syscall wait
  */
  if (args[0] == SYS_WAIT)
  {
    f->eax = syscall_wait((int)args[1]);
    return;
  }

  /*
    syscall halt
  */
  if (args[0] == SYS_HALT)
  {
    syscall_halt();
    return;
  }

  /*  
  syscall write
  */
  if (args[0] == SYS_WRITE)
  {
    f->eax = syscall_write((int) args[1], (const char*) args[2],(size_t) args[3]);
    return;
  }

  /*
  syscall create
  */
  if (args[0] == SYS_CREATE)
  {
    f->eax = syscall_create((const char*)args[1], (size_t)args[2]);
    return;
  }

  /*
  syscall remove
  */
  if (args[0] == SYS_REMOVE)
  {
    f->eax = syscall_remove((const char*)args[1]);
    return;
  }

  /*
  syscall open
  */
  if (args[0] == SYS_OPEN)
  {
    f->eax = syscall_open((const char*)args[1]);
    return;
  }
  
  /*
  syscall filesize
  */
  if (args[0] == SYS_FILESIZE)
  {
    f->eax = syscall_filesize((int)args[1]);
    return;
  }

  /*
  syscall read
  */
  if (args[0] == SYS_READ)
  {
    f->eax = syscall_read(args[1],args[2],args[3]);
    return;
  }

  /*
  syscall seek
  */
  if (args[0] == SYS_SEEK)
  {
    syscall_seek(args[1],args[2]);
    return;
  }

  /*
  syscall tell
  */
  if (args[0] == SYS_TELL)
  {
    f->eax = syscall_tell(args[1]);
    return;
  }

  /*
  syscall close
  */
  if (args[0] == SYS_CLOSE)
  {
    syscall_close(args[1]);
    return;
  }
  /*
  syscall change directory
  */
  if (args[0] == SYS_CHDIR)
  {
    f->eax = syscall_chdir(args[1]);
    return;
  }
  /*
  syscall make directory
  */
  if (args[0] == SYS_MKDIR)
  {
    f->eax = syscall_mkdir(args[1]);
    return;
  }
  /*
  syscall read directory
  */
  if (args[0] == SYS_READDIR)
  {
    
    f->eax = syscall_readdir(args[1], args[2]);
    return;
  }
  /*
  syscall is directory
  */
  if (args[0] == SYS_ISDIR)
  {
    f->eax = syscall_isdir(args[1]);
    return;
  }

  /*
  syscall inumber
  */
  if (args[0] == SYS_INUMBER)
  {
    f->eax = syscall_inumber(args[1]);
    return;
  }
}

void
syscall_exit(int status)
{
  thread_current()->pcb->exit_status = status;
  printf("%s: exit(%d)\n", &thread_current ()->name, status);
  thread_exit();
}

tid_t 
syscall_exec(const char * cmdline)
{
  if (!valid_ptr(cmdline)) syscall_exit(-1);
  return process_execute(cmdline);
}

void 
syscall_halt()
{
  shutdown_power_off();
}

int 
syscall_practice(int i)
{
  return i + 1;
}

int 
syscall_wait(tid_t pid)
{
  return process_wait(pid);  
}

int 
valid_ptr(void* ptr)
{
  if (ptr == NULL) return 0;
  if (!is_user_vaddr(ptr)) return 0;
  if (pagedir_get_page(thread_current()->pagedir, ptr) == NULL) return 0;
  return 1;
}

size_t 
syscall_write(int fd, const char* buff, size_t size)
{
  if(!valid_ptr(buff) || fd == 0)  
    syscall_exit(-1);
  int bytes_written = -1;

  lock_acquire(&filesys_lock);
  if(fd == 1)
  {
    putbuf(buff, size); 
    bytes_written = size;
  }else
  {
    struct fd_table_entry* file = search_file(fd);
   
      
    if(file != NULL)
    {
      if(inode_is_dir(file_get_inode(file->address))){
        lock_release(&filesys_lock);
        return -1;
      }
      bytes_written = file_write(file->address, buff, size);
    }
  }
  lock_release(&filesys_lock);
  return bytes_written;
}

bool 
syscall_create(const char* file, size_t size)
{
  if(!valid_ptr(file))  
    syscall_exit(-1);

  lock_acquire(&filesys_lock);
  bool status = filesys_create(file, (off_t)size, false);
  lock_release(&filesys_lock);
  return status;
}

bool 
syscall_remove(const char* file)
{
  if(!valid_ptr(file))
    syscall_exit(-1);
  lock_acquire(&filesys_lock);
  bool status = filesys_remove(file);
  lock_release(&filesys_lock);
  return status;
}

int 
syscall_open(const char* file)
{
  if(!valid_ptr(file))
    syscall_exit(-1);
  if( strlen(file)==0)
    return -1;
  lock_acquire(&filesys_lock);

  struct file* new_file = filesys_open(file);
  if(new_file == NULL)
  {
    lock_release(&filesys_lock);
    return -1;
  }
  struct fd_table_entry* entry = malloc(sizeof(struct fd_table_entry));
  if(entry==NULL)
    return -1;

  entry->address = new_file;
  entry->fd = ++thread_current()->fd_count;
  list_push_back(&thread_current()->open_fds, &entry->elem);
  lock_release(&filesys_lock);
  return entry->fd;
}

int 
syscall_filesize(int fd)
{
  lock_acquire(&filesys_lock);
  int lentgh = -1;
  struct fd_table_entry* file = search_file(fd);
  if(file != NULL)
  {
    lentgh = file_length(file->address);
  }

  lock_release(&filesys_lock);
  return lentgh;
}

int 
syscall_read(int fd, void *buffer, unsigned size)
{
  if(!valid_ptr(buffer))
    syscall_exit(-1);
  if(size <= 0)
    return size;
  uint8_t* buff = (uint8_t*)buffer;
  int bytes_read = -1;
  lock_acquire(&filesys_lock);
  if(fd == 0)
  {
    bytes_read = 0;
    uint8_t input = input_getc();
    while(input != 0 && bytes_read < size)
    {
      buff[bytes_read++] = input;
      input = input_getc();
    }
  } 
  else
  {
    struct fd_table_entry* file = search_file(fd);
    if(file != NULL)
    {
      bytes_read = file_read(file->address, buffer, size);
    }
  }
  lock_release(&filesys_lock);
  return bytes_read;
}

void 
syscall_seek(int fd, unsigned position)
{
  lock_acquire(&filesys_lock);
  struct fd_table_entry* file = search_file(fd);
  if(file != NULL)
  {
    file_seek(file->address,position);
  }
  lock_release(&filesys_lock);
}

unsigned 
syscall_tell(int fd)
{
  lock_acquire(&filesys_lock);
  struct fd_table_entry* file = search_file(fd);
  int cursor = 0;
  if(file != NULL)
  {
    cursor = file_tell(file->address);
  }

  lock_release(&filesys_lock);
  return cursor;
}

void 
syscall_close(int fd)
{
  lock_acquire(&filesys_lock);
  struct fd_table_entry* file = search_file(fd);
  if(file != NULL)
  {
    list_remove(&file->elem);
    file_close(file->address);
    free(file);
    thread_current()->fd_count--;
  }
  lock_release(&filesys_lock);
}

bool 
syscall_chdir(const char* dir_name)
{
  if(!valid_ptr(dir_name))
    syscall_exit(-1);  

  lock_acquire(&filesys_lock);
  bool status = directory_chdir(dir_name);
  lock_release (&filesys_lock);

  return status;
}

bool 
syscall_mkdir(const char* dir_name)
{
  if(!valid_ptr(dir_name))
    syscall_exit(-1);

  lock_acquire(&filesys_lock);
  bool status = filesys_create(dir_name, 0, true);
  lock_release(&filesys_lock);
  return status;
}

bool 
syscall_readdir(int fd, const char* dir_name)
{
  if(!valid_ptr(dir_name))
    syscall_exit(-1);

  lock_acquire(&filesys_lock);
  
  struct fd_table_entry* file_entry = search_file(fd);
  struct dir * directory = dir_open(file_get_inode(file_entry->address));
  bool status = dir_readdir(directory, dir_name);
  lock_release(&filesys_lock);
  return status;

  return false;
}


bool 
syscall_isdir(int fd)
{
  struct fd_table_entry* file_entry = search_file(fd);
  return inode_is_dir(file_get_inode(file_entry->address));
}

block_sector_t
syscall_inumber(int fd)
{
  struct fd_table_entry* file_entry = search_file(fd);
  return inode_get_inumber(file_get_inode(file_entry->address));
}

struct fd_table_entry* 
search_file(int fd)
{
  struct list* open_fds = &thread_current()->open_fds;
  struct list_elem* e = list_begin(open_fds);
  while(e!=list_end(open_fds))
  {
    struct fd_table_entry* entry = list_entry(e,struct fd_table_entry,elem);
    if(entry->fd == fd) return entry;
    e = list_next(e);
  }
  return NULL;
}