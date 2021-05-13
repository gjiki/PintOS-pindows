Design Document for Project 2: User Programs
============================================

#### Group Members:
  * Davit Bezhanishvili
  * Giorgi Baghdavadze
  * Gegi Jikia

--------------------------------------

#### `process.c`
input : command line string
First step is to tokenize this string into the arguments using the function : void get_argc_argv(char * file_name, struct               argument * arguments);
Using this function we fill data - **struct argument* arguments** and use that data in `tid_t process_execute()`
**process_execute()** initializes struct process_control_block which is the main data in this part. Even when process dies this PCB stays alive and using this we can always find out every childs' exit_status. In process_execute() PCB is being copied on new page.
**The only cases PCB is deleted : parent dies, parent invokes wait.**
execution_error_sema is being used to wait for child thread's load function to finish.

In **start_process()** current_thread() gets PCB data.
**process_wait()** function ensures that parent doesn't continue working untill child exits.
**release_children()** ensures that childs, of the dying parent, are becoming zombie threads. **Zombie Thread** is the thread which no longer has a parent.


#### `syscall.c`
The main function here is **syscall_handler()** which ensures that all codes call right function and some of their executions are atomic. This file uses filesys.c for opening, closing, creating, initializing and removing files.
**struct lock filesys_lock** is being used for the functions which must be atomic : SYS_CREATE, SYS_REMOVE, SYS_OPEN, SYS_FILESIZE...
These functions use data descripted in `syscall.h`. This file contains struct data which stores int fd, struct list_elem file_descriptors, struct file* file_addr.


#### `thread.h`
As we know here all processes are threads, so we decided to update data in thread.h for process to use. They are stored under **USERPROG** part. First PCB about which we already talked. Last but not least **struct list children**. This list contains all the children threads for current parent. Elements in this list are pushed in process_execute() function, and they are poped in release_childre() and process_wait().
