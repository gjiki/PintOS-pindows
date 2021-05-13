Design Document for Project 1: Threads
======================================

#### Group Members:
  * Davit Bezhanishvili
  * Giorgi Baghdavadze
  * Gegi Jikia

--------------------------------------

## **Task 1: Efficient Alarm clock**
### **Data structures and functions:**
#### `thread.h`
* `uint64_t end_tick`
  * new element in structure `thread`
  * represents the tick upon which the sleeping thread should wake up
  * gets set before the thread goes to sleep

#### `timer.c`
* `list sleeping_list`
  * new list in file `timer.c`
  * contains all the threads that are currently sleeping
  * initialised in function `timer_init()`
  * modified when a thread goes to sleep
* `void time_wake_up()`
  * iterates over the `sleeping_list` and wakes up every thread that needs to wake up
* `bool wakes_up_earlyer()`
  * takes 2 threads and aux data as arguments
  * checks if first thread wakes up earlyer than the second


### **Algorithms:**
The solution to this section is quite simple. To bypass busy waiting we created a **list** that remembers which threads are **currently sleeping**, each of these threads contains information about when it should wake up. When `timer_sleep()` function is called a running thread is placed into above mentioned list (**in ordered fashion.** threads that should wake up early are placed before the ones that sould wake up later) and **is blocked**.
On every tick `time_wake_up()` function is called, the function iterates the `sleeping_list` and **unblocks** the threads that need to wake up.


### **Synchronisation:**
This section needed little synchronisation, we **disable interrupts** before adding element into the list and before removing an element from the list to ensure a `timer_interrupt()` function won't interrupt the thread in the middle of modifying the list

### **Rational:**
This solution is very easy to grasp, easy to modify, requires very little additional code and is straightforward which makes it easier to debug. It is also very time efficient cause the **list is ordered** so we don't need any unnecessary checking, we only iterate the threads that need to be unblocked.

## **Task 2: Priority Scheduler**
### **Data structures and functions:**
#### `thread.h` / `thread.c`
* `bool has_higher_priority_than()`
  * compare function for threads
  * used for sorting lists that contain `thread` structures
  * higher priority threads come first
* `bool has_lower_priority_than()`
  * compare function for threads
  * used for finding thread with max priority in lists containing  `thread` structures
  * lower priority threads come first
* `int own_priority`
  * new element in stucture `thread`
  * used to store the unaltered priority of the thread(isn't changed by priority donation)
  * is set on initialization
  * is changed only when `thread_set_priority()` method is called
* `lock* locked_on`
  * new element in structure `thread`
  * used to store the pointer of the lock the thread is locked on
  * gets set when a thread tries to acquire already acquired lock
  * becomes null when thread successfuly acquires a lock
* `list held_locks`
  * new element in structure `thread`
  * used to store all the locks the thread has acquired and not yet released
  * helps to compute **proper priority** value for a thread after it releases a lock

#### `synch.h` / `synch.c`
* `list_elem elem`
  * new element in structure `lock`
  * gives `lock` an option to be stored in a list 
* `semaphore semaphore_for_atomic_donation`
  * new element in structure `lock`
  * ensures that for each lock holder, during priority donation, the priority changes atomicly, so that there is no race condition with 2 paralel donations
* `bool priotiry_sort_func()`
  * compare function for 2 `semaphore_elem` structures
  * checks if the first element has a higher priority
  * helps sort `semaphore_elem` elements in `condition` structures waiting list
* `bool lock_priority_compare()`
  * compare function for 2 `lock` structures
  * compares the priorities of highest priority threads from each lock
  * used to find max priority lock held by some thread, to then find max priority thread waiting for above mentioned thread
* `void recursive_donate()`
  * function for priority donation
  * gets a lock as an argument 
  * if necessary changes the priority of the lock holder to match the priority of a thread waiting for it
  * follows up the lock chain and updates priority of each of the lock holders that has influence over the waiting thread
* `int priority`
  * new element in structure `semaphore_elem`
  * is used to store the priority of a thread waiting on a semaphore of this same structure
  * makes it easier to compare 2 `semaphore_elem` structures



### **Algorithms:**
For every schedule the next thread that is scheduled to run is the max priority thread in the `ready_list`, this is the main part of this scheduler.  
Every time a new thread is added to a `ready_list` the running thread **yields** it's cpu time if this new thread has higher priority, this ensures that at each moment the running thread is the highest priority thread.  
When locked a thread donates it's priority to the lock holder so there is no priority inversion. the donation is performed recursivly so that every thread up the lock chain gets it's priority raised if necessary.  
While waking up a waiting thread, the thread to be woken up is chosen to be of the highest priority.  
When `thread_set_priority()` method is called, only the original priority of the thread is changed. The effective priority is changed only if the original priority becomes higher than the effective priority.


### **Synchronisation:**
As in every other task, the interrupts are turned off for every insert or removal from any of the lists. also `semaphore_for_atomic_donation` is used to erase the race conditions while donating priority recursivly.  
semaphore incrementation during `sema_up` is moved up to happen before unblocking a thread so there is no deadlock if current thread yields it's cpu time to the newly woken up thread.
### **Rational:**
This method of scheduling might not be the most optimal, but is's quite straightforward and easy to implement. There is little room for bugs and if there are some, they are relatively easy to find and fix.

## **Task 3: Multi-level Feedback Queue Scheduler (MLFQS)**
### **Data structures and functions:**

#### `thread.h` / `thread.c`
* `int nice`
  * new element in structure `thread`
  * contains thread's **nice value**
  * is initialized to be the same as creator thread's nice value (or 0 for the initial thread)
* `fixed_point_t recent_cpu`
  * new element in structure `thread`
  * contains **aproximate cpu time recently used by the thread**
  * is initialized to be the same as creator thread's recent_cpu value (or 0 for initial thread)
  * **is updated every second** for every thread
* `static fixed_point_t load_avg`
  * static variable contained in `thread.c`
  * contains the aproximate number of threads ready to run over past minute 
  * is initialized to be 0
  * **is updated every second**
* `void thread_update_load_avg()`
  * updates load average value for the system
  * uses formula:  
  $load\_avg = (59/60)*load\_avg + (1/60)*ready\_threads$
* `void thread_update_recent_cpu()`
  * iterates over all threads in the system and calls `recent_cpu_formula()` for each one
* `void thread_update_priorities()`
  * iterates over all threads in the system and calls `priority_formula()` for each one
* `void recent_cpu_formula()`
  * is used as an argument for the function `thread_foreach`
  * takes a thread as an argument and updates it's recent_cpu value
  * uses formula:
  $recent\_cpu = (2*load\_avg)/(2*load\_avg + 1)*recent\_cpu + nice$
* `void priority_formula()`
  * is used as an argument for the function `thread_foreach`
  * takes a thread as an argument and updates it's priority
  * uses formula:  
  $priority = PRI\_MAX - (recent\_cpu / 4) - (2*nice)$


### **Algorithms:**
This part of the assignment is quite easy to do after Task 2 is ready, there is not much additional synchronisation required.  
Every thread keeps track of it's own **nice** and **recent_cpu** values and the system keeps track of **load_avg** value. every 4 ticks the priority of every thread is recalculated. This solves the problem for thread starvation, threads that have low **recent_cpu** (which means they haven't gotten much cpu time recently) get higher priorities than the threads with hith **recent_cpu**.
every 1 second (or every `TIMER_FREQ` ticks) the values of **load_avg** and every thread's **recent_cpu** is updated

### **Synchronisation:**
There was no additional synchronisation required for this task

### **Rational:**
This type of scheduler is less biased towards high priority threads and solves the problem of starvation


## **Design Document Additional Questions**
### Test
The test described in this question is already present in the Pint-OS code. You can check it [here](https://github.com/FreeUni-OS-2018Spring/pintos-pindows/blob/master/pintos/src/tests/threads/priority-donate-sema.c)
### MLFQS scheduler
timer ticks | R(A) | R(B) | R(C) | P(A) | P(B) | P(C) | thread to run
------------|------|------|------|------|------|------|--------------
 0          |0     |0     |0     |0     |0     |0     |A
 4          |4     |0     |0     |60    |61    |61    |B
 8          |4     |4     |0     |60    |60    |61    |C
12          |4     |4     |4     |60    |60    |60    |A
16          |8     |4     |4     |59    |60    |60    |B
20          |8     |8     |4     |59    |59    |60    |C
24          |8     |8     |8     |59    |59    |59    |A
28          |12    |8     |8     |58    |59    |59    |B
32          |12    |12    |8     |58    |58    |59    |C
36          |12    |12    |12    |58    |58    |58    |A