#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/pte.h"

static void syscall_handler (struct intr_frame *);
static void system_open(void);
static bool is_valid_memory_access(int *vaddr);

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
  switch(*num){
  	case SYS_HALT:
  		print_okay();
  		thread_exit();
  		break;
    case SYS_EXIT:
    	print_okay();
    	thread_exit();
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
    	//system_open();
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
system_open(void){
	int vaddr = &(f->esp) - (1 * (size_of(int))); // The first argument in the interupt frame

	if (is_valid_memory_access(vaddr)) {
		// Dereference the pointer
		f->eax = *vaddr;
	}	
}

/* 
	Returns true if the memory address is valid and can be accessed by the user process.
*/
static bool
is_valid_memory_access(int *vaddr) {
	if (&vaddr == 0) {
		// Null Pointer
		return false;
	else if (is_kernel_vaddr(vaddr)) { 
		// Pointer to Kernel Virtual Address Space
		return false;
		
	} else if (pagedir_get_page(pd_no(vaddr),vaddr) == NULL) { 
		// Pointer to Unmapped Virtual Memory
		return false;
	}

	return true;
}






