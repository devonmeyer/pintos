#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "threads/pte.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "devices/shutdown.h"
#include "process.h"

static void syscall_handler (struct intr_frame *);
static bool is_valid_memory_access(const void *vaddr);
static void get_arguments(struct intr_frame *f, int num_args, int * arguments);

static void system_open(struct intr_frame *f, int * arguments);
static void system_write(struct intr_frame *f, int * arguments);
static void system_exit(int s);

static bool system_create(struct intr_frame *f, int * arguments);

static struct lock file_sys_lock;

#define WRITE_CHUNK_SIZE 300

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&file_sys_lock);
}

static void
syscall_handler (struct intr_frame *f) 
{
  printf ("system call!\n");
  int * num = f->esp;
  int arguments[3];
  printf ("System call is %d\n", *num);
  switch(*num){
  	case SYS_HALT:
      shutdown_power_off();
  		thread_exit();
  		break;
    case SYS_EXIT:
    	get_arguments(f, 1, arguments);
      f->eax = (int) arguments[0];
      system_exit(arguments[0]);
            // Need to set f->eax to arguments[0]
    	break;
    case SYS_EXEC:
      get_arguments(f, 1, arguments);
      // f->eax = system_exec((const char*)arguments[i]);
    	break;
    case SYS_WAIT:
    	break;
    case SYS_CREATE:
      get_arguments(f, 2, arguments);
      f->eax = system_create(f, arguments);
    	break;
    case SYS_REMOVE:
      get_arguments(f, 1, arguments);

    	break;
    case SYS_OPEN:
      get_arguments(f, 1, arguments);
    	system_open(f, arguments);
    	break;
    case SYS_FILESIZE:
    	break;
    case SYS_READ:
    	break;
    case SYS_WRITE:
      get_arguments(f, 3, arguments);
      system_write(f, arguments);
    	break;
    case SYS_SEEK:
    	break;
    case SYS_TELL:
    	break;
    case SYS_CLOSE:
    	break;
    default:
    	thread_exit();  
      break;
  }
}

// static pid_t 
// system_exec (const char *cmd_line)
// {
//   // pid is mapped exactly to tid, process_execute returns tid
//   pid_t pid = (pid_t)process_execute(cmd_line);
// }

/* 
	The method called when the SYS_OPEN is called. 
  System call format:
  int open (const char *file)
*/
static void
system_open(struct intr_frame *f, int * arguments){
	//void *vaddr = ((char *) (f->esp)) + 1; // The first argument in the interrupt frame
    char *vaddr = ((char *) arguments[0]);
    
    printf ("vaddr: %X\n",vaddr);

	if (is_valid_memory_access(vaddr) == true) {
		// User Virtual Address -> Kernel Virtual Address -> Physical Address

    struct thread *t = thread_current ();
		char *kvaddr = pagedir_get_page(t->pagedir,vaddr); 
 
    // char val = *kvaddr; // DEREFERENCING (try & debug)

    // TO BE IMPLEMENTED LATER:
    struct fd_info fdi;
    fdi.start_addr = kvaddr;
    int fd;

    // Choose an available file descriptor
    int i;
    for (i = 2; i < 18; i++) {
      if (t->fd_array[i].valid == 0) {
        fd = i;
      }
    }

    // Set the file descriptor info at the fd slot in the array
    t->fd_array[fd] = fdi;
   
    // Return the file descriptor number
    f->eax = fd;

	} else {
    printf ("invalid open call, will exit soon\n");
    system_exit(-1);
	}
}

static void system_exit(int status){
  printf("%s: exit(%d)\n", thread_current()->name, status);
  thread_exit();
}


/* 
  The method called when the SYS_WRITE is called. 
  System call format: 
  int write (int fd, const void *buffer, unsigned size)
*/
static void
system_write(struct intr_frame *f, int * arguments){
  // Assumes that items are pushed onto the stack from right to left
  int fd = ((int) arguments[0]); 
  const void *buffer = ((void *) arguments[1]);
  unsigned size = ((unsigned) arguments[2]);


if (is_valid_memory_access (buffer) == false) {
    system_exit(-1);
}
  buffer = pagedir_get_page(thread_current()->pagedir, buffer);

  // putbuf (const char *buffer, size_t n), defined in console.c
  ASSERT (fd != 0); // FD of 0 is reserved for STDIN_FILENO, the standard input
  if (fd == 1) {
    // Writing to the console (like a print statement)
    if (size <= WRITE_CHUNK_SIZE) {
      putbuf (buffer, size);
    } else {
      // Break the write into smaller chunks
      while (size > WRITE_CHUNK_SIZE) {
        putbuf (buffer, size);
        size -= WRITE_CHUNK_SIZE;
      }
    }
  } else {
    // Writing to a file with a file descriptor fd

    lock_acquire (&file_sys_lock);
    //file_write ();
    lock_release (&file_sys_lock);    
    //ASSERT (thread_current ()->fd_array[fd]); // Must be an open file
    
  }

}

static bool system_create(struct intr_frame *f, int * arguments){
  bool result = false;
  if(is_valid_memory_access((void*) arguments[0])){
    const char * created_file = (char *) pagedir_get_page(thread_current()->pagedir, (void *) arguments[0]);
    const unsigned s = (unsigned) arguments[1];
    lock_acquire(&file_sys_lock);
    result = filesys_create(created_file, s);
    lock_release(&file_sys_lock);
    return result;
  } else {
    system_exit(-1);
  }
}



/* 
	Returns true if the memory address is valid and can be accessed by the user process.
*/
static bool
is_valid_memory_access(const void *vaddr) {

  // Use ASSERT statements for debugging, then remove before publishing code:
	//ASSERT (vaddr != NULL);
	//ASSERT (!is_kernel_vaddr(vaddr));
	//ASSERT (pagedir_get_page(thread_current ()->pagedir,vaddr) != NULL);


	if (vaddr == NULL) {
		// Null Pointer
    printf ("--- addr is null pointer ---\n");
		return false;
	} else if (is_kernel_vaddr(vaddr)) { 
		// Pointer to Kernel Virtual Address Space
    printf ("--- is kernel virtual address ---\n");
		return false;
	} else if (pagedir_get_page(thread_current ()->pagedir, vaddr) == NULL) { 
		// Pointer to Unmapped Virtual Memory
    printf ("--- Unmapped ---\n");
		return false;
	}

  printf ("--- valid_memory_access ---\n");


	return true;
}

static void
get_arguments(struct intr_frame * f, int num_args, int * arguments){
  int i;
  for (i = 0; i < num_args; i++){
    int * arg = (int *) f->esp + 1 + i;
    arguments[i] = *arg;
  }
}
