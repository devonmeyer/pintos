#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/pte.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "process.h"

static void syscall_handler (struct intr_frame *);
static bool is_valid_memory_access(const void *vaddr);
static void get_arguments(struct intr_frame *f, int num_args, int * arguments);

static pid_t system_exec (const char *cmd_line);
static void system_open(struct intr_frame *f);
static void system_write(struct intr_frame *f, int * arguments);

#define WRITE_CHUNK_SIZE 300

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
  int arguments[3];
  printf ("System call is %d\n", *num);
  switch(*num){
  	case SYS_HALT:
  		print_okay();
      shutdown_power_off();
  		thread_exit();
  		break;
    case SYS_EXIT:
      print_okay();
    	get_arguments(f, 1, arguments);
    	thread_exit();
      // Need to set f->eax to arguments[0]
    	break;
    case SYS_EXEC:
      print_okay();
      get_arguments(f, 1, arguments);
      f->eax = system_exec((const char*)arguments[i]);
    	break;
    case SYS_WAIT:
    	break;
    case SYS_CREATE:
    	break;
    case SYS_REMOVE:
    	break;
    case SYS_OPEN:
      print_okay();
    	system_open(f);
      thread_exit();
    	break;
    case SYS_FILESIZE:
    	break;
    case SYS_READ:
    	break;
    case SYS_WRITE:
      print_okay();
      get_arguments(f, 3, arguments);
      system_write(f, arguments);
      thread_exit();
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

static pid_t 
system_exec (const char *cmd_line)
{
  // pid is mapped exactly to tid, process_execute returns tid
  pid_t pid = (pid_t)process_execute(cmd_line);
}

void
print_okay(void){
	printf("Got a system call that I expected!\n");
}


/* 
	The method called when the SYS_OPEN is called. 
  System call format:
  int open (const char *file)
*/
static void
system_open(struct intr_frame *f){
	void *vaddr = ((int) (f->esp)) + 1; // The first argument in the interrupt frame

	if (is_valid_memory_access(vaddr)) {
		// User Virtual Address -> Kernel Virtual Address -> Physical Address

    struct thread *t = thread_current ();
		char *kvaddr = pagedir_get_page(t->pagedir,vaddr); 
 
   // char val = *kvaddr; // DEREFERENCING (try & debug)

    if (t->fd_counter <= 1) {
      f->eax = -1; // Returns a file descriptor of -1
      process_exit ();
    }


    // TO BE IMPLEMENTED LATER:
    struct fd_info fdi;
    fdi.start_addr = kvaddr;
    t->fd_array[t->fd_counter] = fdi;

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
    f->eax = -1; // Returns a file descriptor of -1 ->>>>> Is this correct???
    process_exit ();
}
  buffer = pagedir_get_page(thread_current()->pagedir, buffer);
  printf("new buffer: %d\n", buffer);
  printf("pagedir %d", thread_current()->pagedir);
  printf("1\n");
  // putbuf (const char *buffer, size_t n), defined in console.c
  ASSERT (fd != 0); // FD of 0 is reserved for STDIN_FILENO, the standard input
  printf("2\n");
  if (fd == 1) {
    // Writing to the console (like a print statement)
    if (size <= WRITE_CHUNK_SIZE) {
      printf("3\n");
      putbuf (buffer, size);
      printf("4\n");
    } else {
      // Break the write into smaller chunks
      printf("5\n");
      while (size > WRITE_CHUNK_SIZE) {
        printf("6\n");
        putbuf (buffer, size);
        size -= WRITE_CHUNK_SIZE;
      }
    }
  } else {
    // Writing to a file with a file descriptor fd
    printf("7\n");
    //ASSERT (thread_current ()->fd_array[fd]); // Must be an open file
    
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
		return false;
	} else if (is_kernel_vaddr(vaddr)) { 
		// Pointer to Kernel Virtual Address Space
		return false;
	} else if (pagedir_get_page(thread_current ()->pagedir, vaddr) == NULL) { 
		// Pointer to Unmapped Virtual Memory
		return false;
	}

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
