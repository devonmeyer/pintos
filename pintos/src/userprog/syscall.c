#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/pte.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"

static void syscall_handler (struct intr_frame *);
static void system_open(struct intr_frame *f);
static bool is_valid_memory_access(const void *vaddr);
static bool get_arguments(struct intr_frame *f, int num_args, int * arguments);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  printf ("system call!\n");
  int * num = f->esp;
  int * arguments;
  switch(*num){
  	case SYS_HALT:
  		print_okay();
  		thread_exit();
  		break;
    case SYS_EXIT:
    	arguments = get_arguments(f, 0);
    	thread_exit();
      // Need to set f->eax to arguments[0]
    	break;
    case SYS_EXEC:
    	break;
    case SYS_WAIT:
    	break;
    case SYS_CREATE:
    	break;
    case SYS_REMOVE:
    	break;
    case SYS_OPEN:
    	system_open(f);
      thread_exit();
    	break;
    case SYS_FILESIZE:
    	break;
    case SYS_READ:
    	break;
    case SYS_WRITE:
    	break;
    case SYS_SEEK:
    	break;
    case SYS_TELL:
    	break;
    case SYS_CLOSE:
    	break;
    default:
    	thread_exit();  
  }
}

void
print_okay(void){
	printf("Got a system call that I expected!\n");
}


/* 
	The method called when the SYS_OPEN is called. Format: open (VIRTUAL_ADDRESS).
*/
static void
system_open(struct intr_frame *f){
	void *vaddr = ((int) (f->esp)) + 1; // The first argument in the interrupt frame

	if (is_valid_memory_access(vaddr)) {
		// User Virtual Address -> Kernel Virtual Address -> Physical Address

    struct thread *t = thread_current ();
		char *kvaddr = pagedir_get_page(t->pagedir,vaddr); 
 
   // char val = *kvaddr; // DEREFERENCING (try & debug)

    ASSERT (t->fd_counter > 1); // FD's of 0 and 1 are RESERVED

    // TO BE IMPLEMENTED LATER:
    //struct fd_info fdi = ... fill in the struct
    //t->fd_array[t->fd_counter] = fdi;

    f->eax = t->fd_counter;
    t->fd_counter++;
    
    // wrong:
    //f->eax--; // "Increment" the stack pointer (We subtract here because the stack grows DOWN)
	} else {
    f->eax = -1; // Returns a file descriptor of -1
    process_exit ();
	}
}
	
/* 
	Returns true if the memory address is valid and can be accessed by the user process.
*/
static bool
is_valid_memory_access(const void *vaddr) {

	ASSERT (vaddr != NULL);
	ASSERT (!is_kernel_vaddr(vaddr));
	ASSERT (pagedir_get_page(thread_current ()->pagedir,vaddr) != NULL);
/*
	// USE ASSERTIONS to handle invalid addr
	if (*vaddr == 0) {
		// Null Pointer
		return false;
	else if (is_kernel_vaddr(vaddr)) { 
		// Pointer to Kernel Virtual Address Space
		return false;
		
	} else if (pagedir_get_page(thread_current()->pagedir,vaddr) == NULL) { 
		// Pointer to Unmapped Virtual Memory
		return false;
	}
*/
	return true;
}

static bool
get_arguments(struct intr_frame * f, int num_args){
  int arguments[num_args];
  int i;
  for (i = 0; i < n; i++){
    int * arg = (int *) f->esp + 1 + i;
    arguments[i] = *arg;
  }
  return arguments;
}







