#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include <hash.h>
#include "../devices/timer.h"
#include "vm/frame.h"


/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */


// /* The information associated with each file descriptor. */
  struct fd_info {
      int size;
      void *start_addr;
      //bool slot_is_empty;
      struct file *file;
      // size, name, begin addr, offset, etc. look at wherever the FD is stored later
    };

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    struct list_elem allelem;           /* List element for all threads list. */
    int64_t wake_up_time;

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

    struct list donated_priorities;       /* The list of donated priorities. (int's) */

    uint32_t *pagedir;                  /* Page directory. */
    
    struct fd_info * fd_array[18];       /* The array of file descriptors, indexed by the file descriptor number.
                                          There are only 16 files in Pintos, but 0 and 1 are reserved fd values. */
    struct thread * parent;

    struct list children;

    struct hash sup_page_table;        /* The Supplemental Page Table associated with this thread if it is a user process. */

<<<<<<< HEAD
=======
    struct list * mmap_table[16];

    int mapid_counter;              /* Used to generate unique mapid's for mem_mapping. */

>>>>>>> 08a0520e871e49599028e3daa1e42071c6ec7cf7
    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };

struct child_process {
  int pid;
  int exit_status;
  bool wait_called;
  struct list_elem process_element;
  bool has_exited;
};


// Exec
/*
  Called by A
  Spawn new process B
  Create new child_process with pid = B->pid, exit_status = -1, wait_called = false, has_exited = false;
  Add child_process->process_element to A->children;
  B->parent = A;
  return B->pid;

*/

// Wait

/*
  A calls wait(B)
  Do checks:
    1. If B is not a member of A->children: return -1;
    2. Find list element in A->children where elem->pid == B->pid -- if elem->wait_called == true: return -1;
  
  If we get to this point...
  cp = child_process where cp->pid == B->pid
  cp->wait_called = true;
  while (!has_exited){
    
  }
  return cp->exit_status;

*/


// Exit

  /*

  A calls exit

  p = A->parent->elem (where elem->pid == A->pid)
  p->exit_status = (-1 or passed exit_status)
  p->has_exited = true;
  thread_exit();

  */



/*

List element for priorities.

*/

struct prio {
  struct list_elem prio_elem;
  int priority;
  struct thread * donator;
};


/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void set_child_of_thread(int pid);
struct child_process * get_child_of_thread(struct thread * parent, int pid);
void set_exit_status_of_child(struct thread * parent, int pid, int status);

bool thread_exists(struct thread * t);

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int get_priority_of_thread (struct thread * t);


void thread_donate_priority (struct thread * t, struct thread * donator);
void thread_revoke_priority (struct thread * t, struct thread * revoker);

bool thread_has_highest_priority (struct thread * t);


int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void wake_up_sleeping_threads (void);

void reset_all_accessed_bits (void);

#endif /* threads/thread.h */