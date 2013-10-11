#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* Values used for p.q fixed point arithmetic. 
   Represents P=17 digits before the decimal point,
   and Q=14 digits after the decimal point.*/
#define FIXED_POINT_P 17
#define FIXED_POINT_Q 14

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Lock used for the ready_list. */
static struct lock ready_list_lock;

/* Lock used for the all_list. */
static struct lock all_list_lock;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* A "FIXED POINT" value that estimates the average number of 
   threads ready to run over the past minute. */
static int load_avg_fp;


/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_sleep(int64_t start, int64_t duration);
void thread_schedule_tail (struct thread *prev);
static void wake_up_thread (struct thread * t, void * aux);
static tid_t allocate_tid (void);
static struct thread *get_highest_priority_thread (void);
static bool thread_has_highest_priority (struct thread * t);
void update_load_avg (void);
void recalculate_all_recent_cpu (void);
static void recalculate_recent_cpu (struct thread * t, void * aux);
void recalculate_all_priority (void);
static void recalculate_priority (struct thread * t);
static void recalculate_priority_foreach (struct thread * t, void * aux);

/* Convert integer to fixed point. */
static int int_to_fp (int n) {
  int f = 1 << FIXED_POINT_Q; 
  return n * f;
}

/* Convert fixed point to integer, rounding towards zero. */
static int fp_to_int_r_zero (int fp) {
    int f = 1 << FIXED_POINT_Q; 
    return fp / f;
} 

/* Convert fixed point to integer, rounding to nearest. */
static int fp_to_int_r_nearest (int fp) {
    int f = 1 << FIXED_POINT_Q; 
    if (fp >= 0) {
      return (x + (f/2)) / f;
    } else {
      return (x - (f/2)) / f;
    }
} 

/* Add fixed point and integer. */
static int add_fp_and_int (int fp, int n) {
  int f = 1 << FIXED_POINT_Q; 
  return fp + (n * f);
}

/* Add two fixed point values. */
static int fp_plus_fp (int fp1, int fp2) {
  return fp1 + fp2;
}

/* Subtract two fixed point values. */
static int fp_minus_fp (int fp1, int fp2) {
  return fp1 - fp2;
}

/* Add fixed point and integer. */
static int fp_plus_int (int fp, int n) {
  int f = 1 << FIXED_POINT_Q; 
  return fp + (n * f);
}

/* Subtract integer from a fixed point. */
static int fp_minus_int (int fp, int n) {
  int f = 1 << FIXED_POINT_Q; 
  return fp - (n * f);
}

/* Multiply fixed point by fixed point. */
static int fp_times_fp (int fp1, int fp2) {
  return ((int64_t) fp1) * fp2 / f
}

/* Multiply fixed point by integer. */
static int fp_times_int (int fp, int n) {
  return fp * n;
}

/* Divide fixed point by fixed point. */
static int fp_div_by_fp (int fp1, int fp2) {
  return ((int64_t) fp1) * f / fp2;
}

/* Divide fixed point by integer. */
static int fp_div_by_int (int fp, int n) {
  return fp / n;
}

//static int get_priority_of_thread (struct thread * t);

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  lock_init (&ready_list_lock);
  lock_init (&all_list_lock);

  list_init (&ready_list);
  list_init (&all_list);

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
  
  /* Initialize the system's load_avg. */
  load_avg_fp = 0 * (1 << FIXED_POINT_Q); // convert 0 to fixed point
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Each time a timer interrupt occurs, recent_cpu is incremented by 1 for 
     the running thread only, unless the idle thread is running. */
  if (thread_current () != idle_thread) {
    thread_current ()->recent_cpu_fp 
  }

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE) {
    recalculate_all_priority (); // once every fourth clock tick!
    intr_yield_on_return ();
  }
}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  /* Add to run queue. */
  thread_unblock (t);

  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  // lock_acquire (&ready_list_lock);
  list_push_back (&ready_list, &t->elem);
  // lock_release (&ready_list_lock);
  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING); 

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  //lock_acquire (&all_list_lock);
  list_remove (&thread_current()->allelem);
  //lock_release (&all_list_lock);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  //lock_acquire (&ready_list_lock);
  if (cur != idle_thread) 
    list_push_back (&ready_list, &cur->elem);

  //lock_release (&ready_list_lock);
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
  thread_current ()->priority = new_priority;
  if (!thread_has_highest_priority (thread_current ())) {
    thread_yield ();
  }
}

/* Gets the priority of the current thread. */
int
thread_get_priority (void) 
{
  return get_priority_of_thread (thread_current ());
}

/* Returns the current thread's priority if it has not been given a donated
   priority.  Otherwise, returns the top item from the thread's donated
   priority stack. */
int 
get_priority_of_thread (struct thread * t) {
  if (list_empty (&t->donated_priorities))
  {
    return t->priority;
  } else
  {
    struct prio *pr = list_entry (list_front (&t->donated_priorities), struct prio, prio_elem);
    return pr->priority;
  }
}


/*

  Add p to the thread t's stack of donated priorities.

*/
void
thread_donate_priority (struct thread * t, int p){
  struct prio * pr = malloc(sizeof(struct prio));
  pr->priority = p;
  list_push_front(&t->donated_priorities, pr);
}


/*

  Remove p from the thread's stack of donated priorities

*/
void
thread_revoke_priority (struct thread * t, int p){
  
  ASSERT(!list_empty(&t->donated_priorities));
  //printf("num of donated priorities : %d", list_size (&t->donated_priorities));
  struct list_elem *e;

  for (e = list_begin (&t->donated_priorities); e != list_end (&t->donated_priorities);
       e = list_next (e))
    {
      struct prio *pr = list_entry (e, struct prio, prio_elem);
      if (pr->priority == p){
        list_remove(e);
        return;
      }
    }

}


/* Updates the system's load average. Called by timer.c. */
void
update_load_avg (void)
{
    // formula: load_avg = (59/60)*load_avg + (1/60)*ready_threads,
    // annotated: load_avg = (A * load_avg) + (B * ready_threads)

    int f = 1 << FIXED_POINT_Q; // for conversion to fixed point
    int fifty_nine_fp = 59 * f;
    int one_fp = 1 * f;

    int A_fp = fifty_nine_fp / 60;
    int B_fp = one_fp / 60;

    int num_rt_fp = (list_size (&ready_list)) * f;

    // Now that the variables are set, actually update the load avg value
    load_avg_fp = (A_fp * load_avg_fp) + (B_fp * num_rt_fp);
}


/* Recalculates all of the thread's priorities according the the MLFQS. 
   Called once every 4 timer ticks. */
void 
recalculate_all_priority (void)
{
  thread_foreach (recalculate_priority_foreach, NULL);
}

/* Helper function to be used in the thread_foreach call. */
static void 
recalculate_priority_foreach (struct thread * t, void * aux)
{
  recalculate_priority (t);
}

/* Recalculates the given thread's priority. */
static void 
recalculate_priority (struct thread * t) 
{
  // formula: priority = PRI_MAX - (recent_cpu / 4) - (nice * 2)
  // annotated: priority = pri_max_p fp - A - B

  int f = 1 << FIXED_POINT_Q; // for conversion to fixed point
  int pri_max_fp = PRI_MAX * f;
  int A_fp = t->recent_cpu_fp / 4;
  int B_fp = (t->nice * 2) * f; 

  int priority_fp = pri_max_fp - A_fp - B_fp;
  
  int priority_int = priority_fp >> FIXED_POINT_Q; // Is this a proper truncate?
  if (priority_int < PRI_MIN) {
    priority_int = PRI_MIN;
  } else if (priority_int > PRI_MAX) {
    priority_int = PRI_MAX;
  }

  // And finally set the thread's priority
  t->priority = priority_int;
}


/* Recalculates every thread's respective recent cpu values. Called by timer.c. */
void
recalculate_all_recent_cpu (void)
{
  thread_foreach(recalculate_recent_cpu, NULL);
}

/* Called on every thread in the system. Recalculates the thread's recent cpu value. */
static void
recalculate_recent_cpu (struct thread * t, void * aux)
{
  //formula: recent_cpu_fp = (2*load_avg)/(2*load_avg + 1) * recent_cpu + nice.
  //annotated: recent_cpu_fp = A / B * recent_cpu_fp + nice

  int f = 1 << FIXED_POINT_Q; // for conversion to fixed point
  int A_fp = (2 * load_avg_fp);
  int B_fp = (A_fp + (1 * f));
  int nice_fp = t->nice * f;


  t->recent_cpu_fp = A_fp / B_fp * t->recent_cpu_fp + nice_fp;
}

/* Sets the current thread's nice value to NEW_NICE. */
void
thread_set_nice (int new_nice) 
{
  // Clamp the new_nice value to the bounds
  if (new_nice > 20) {
    new_nice = 20;
  } else if (new_nice < -20) {
    new_nice = -20;
  }

  thread_current ()->nice = new_nice;
  recalculate_priority (thread_current ());

  if (!thread_has_highest_priority (thread_current ())) {
    thread_yield ();
  }
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  return thread_current ()->nice;
}

/* Returns 100 times the current system load average, rounded to the nearest integer. */
int
thread_get_load_avg (void) 
{
  int f = 1 << FIXED_POINT_Q; // for conversion to fixed point
  int load_avg_int; // rounded to the NEAREST integer

  // Perform rounding to the nearest integer
  if (load_avg_fp >= 0) {
    load_avg_int = (load_avg_fp + (f/2)) / f;
  } else {
    load_avg_int = (load_avg_fp - (f/2)) / f;
  }

  return 100 * load_avg_int;
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  /* Not yet implemented. */
  return 0;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  enum intr_level old_level;

  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;
  t->magic = THREAD_MAGIC;
  t->wake_up_time = 0;
  list_init (&t->donated_priorities);

  
  if (list_empty (&all_list)) {
    t->recent_cpu_fp = 0 * (1 << FIXED_POINT_Q); // "The initial value of recent_cpu is 0 in the
                                                                        // first thread created"
    t->nice = 0; // Initial thread starts with a nice value of 0
  } else {
    t->recent_cpu_fp = thread_current ()->recent_cpu_fp; // The PARENT THREAD's recent cpu value
    t->nice = thread_current ()->nice; // The PARENT THREAD's nice value
  }

  old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  intr_set_level (old_level);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return get_highest_priority_thread ();
    //return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

static struct thread *
get_highest_priority_thread (void)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);
  
  // This assertion added by us
  ASSERT (!list_empty (&ready_list));

  // Get the priority of the first thread in the list
  struct thread * highest = list_entry (list_begin (&ready_list), struct thread, elem);

  for (e = list_begin (&ready_list); e != list_end (&ready_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, elem);
      if (get_priority_of_thread(t) > get_priority_of_thread(highest)) {
        highest = t;
      }
    }

  list_remove(&highest->elem); // remove the highest priority thread from the ready list (we used to "pop" it off)

  return highest;
}

static bool
thread_has_highest_priority (struct thread * t)
{
  struct list_elem *e;

  // Compare the priority of the current thread with the threads in the ready list
  for (e = list_begin (&ready_list); e != list_end (&ready_list);
       e = list_next (e))
    {
      struct thread *ready_thread = list_entry (e, struct thread, elem);
      if (get_priority_of_thread(ready_thread) > get_priority_of_thread (t)) {
        return false;
      }
    }

  return true;
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}


/* Puts a thread to sleep by blocking the thread, updating the status,
   and adding the thread to the list of blocked threads. 
   
   This function is imitating thread_block but is specifically preparing
   it for sleep. */

void
thread_sleep(int64_t start, int64_t duration) {
  enum intr_level old_level = intr_disable ();
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  struct thread *cur = thread_current ();
  lock_acquire (&ready_list_lock);
  list_remove(&cur->elem); // remove the thread from the ready list
  lock_release (&ready_list_lock);
  cur->status = THREAD_BLOCKED;
  cur->wake_up_time = (start + duration);

  // printf ("THREAD_SLEEP: %s, %d \n",cur->name,cur->wake_up_time);

  // list_push_back (&blocked_list, &cur->elem);
  schedule ();
  intr_set_level (old_level);
}

void
wake_up_sleeping_threads (void)
{
  thread_foreach(wake_up_thread, NULL);
}

static void
wake_up_thread (struct thread * t, void * aux){
  // A sleeping thread will have a NONZERO wake up time
  int64_t current_tick = timer_ticks ();  
  if (t->wake_up_time != 0 && current_tick >= t->wake_up_time) 
  {
      t->wake_up_time = 0; // reset the wake up time to ZERO
      thread_unblock(t);
  }

}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));
  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);
