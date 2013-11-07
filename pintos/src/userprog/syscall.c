#include "userprog/syscall.h"
#include "lib/user/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "threads/pte.h"
#include "devices/input.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/shutdown.h"
#include "process.h"

static void syscall_handler (struct intr_frame *);
static bool is_valid_memory_access(const void *vaddr);
static void get_arguments(struct intr_frame *f, int num_args, int * arguments);

static int system_open(int * arguments);
static int system_read(int * arguments);
static unsigned system_write(int * arguments);
static void system_exit(int s);
static int system_filesize(int * arguments);
static bool system_create(int * arguments);
static unsigned system_tell(int * arguments);
static void system_close(int * arguments);
static void system_seek(struct intr_frame *f, int * arguments);

static int system_exec (int * arguments);

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
  int * num = f->esp;
  int arguments[3];
  switch(*num){
  	case SYS_HALT:
      shutdown_power_off();
  		thread_exit();
  		break;
    case SYS_EXIT:
    	get_arguments(f, 1, arguments);
      f->eax = (int) arguments[0];
      system_exit(arguments[0]);
    	break;
    case SYS_EXEC:
      get_arguments(f, 1, arguments);
      f->eax = system_exec(arguments);
    	break;
    case SYS_WAIT:
      get_arguments(f, 1, arguments);
      f->eax = process_wait((tid_t) arguments[0]);
    	break;
    case SYS_CREATE:
      get_arguments(f, 2, arguments);
      f->eax = system_create(arguments);
    	break;
    case SYS_REMOVE:
      get_arguments(f, 1, arguments);
    	break;
    case SYS_OPEN:
      get_arguments(f, 1, arguments);
    	f->eax = system_open(arguments);
    	break;
    case SYS_FILESIZE:
      get_arguments(f, 1, arguments);
      f->eax = system_filesize(arguments);
    	break;
    case SYS_READ:
      get_arguments(f, 3, arguments);
      f->eax = system_read(arguments);
    	break;
    case SYS_WRITE:
      get_arguments(f, 3, arguments);
      f->eax = system_write(arguments);
    	break;
    case SYS_SEEK:
      get_arguments(f, 2, arguments);
      system_seek(f, arguments);
    	break;
    case SYS_TELL:
      get_arguments(f, 1, arguments);
      f->eax = system_tell(arguments);
    	break;
    case SYS_CLOSE:
      get_arguments(f, 1, arguments);
      system_close(arguments);
    	break;
    default:
    	thread_exit();  
      break;
  }
}

static void 
system_seek(struct intr_frame *f, int * arguments)
{
  int fd = ((int) arguments[0]);
  int position = ((off_t) arguments[1]);
  struct thread *t = thread_current ();

  if(t->fd_array[fd].slot_is_empty == false) {
    struct file * open_file = t->fd_array[fd].file;
    file_seek(open_file, position);
  } else {
    printf ("File with fd=%d was not open, exiting...\n",fd);
    system_exit(-1);
  }
}

static void
system_close_all(){
  int i = 0;
  struct thread * holder = thread_current();
  for(i = 0; i < 18; i++){
    if(!holder->fd_array[i].slot_is_empty){
      system_close(&i);
    }
  }
}


static void
system_close(int * arguments) 
{
  int fd = ((int) arguments[0]); 
  struct thread *t = thread_current ();
  
  if (t->fd_array[fd].slot_is_empty == false) {
    file_close (t->fd_array[fd].file);
  } else {
    printf ("File with fd=%d was not open, exiting...\n",fd);
    system_exit(-1);
  }
}

 static pid_t 
 system_exec (int * arguments)
 {
   // pid is mapped exactly to tid, process_execute returns tid
  if(!is_valid_memory_access(arguments[0])){
    system_exit(-1);
  }
  const void * args = pagedir_get_page(thread_current()->pagedir, arguments[0]);
  pid_t pid = ((pid_t) process_execute((char *) args));
  set_child_of_thread((int) pid);
  return pid;
 }

static unsigned 
system_tell(int * arguments)
{
  int fd = ((int) arguments[0]); 
  struct thread *t = thread_current ();
  
  if (t->fd_array[fd].slot_is_empty == false) {
    unsigned tell = ((unsigned)file_tell (t->fd_array[fd].file));
    return tell;
  } else {
    printf ("File with fd=%d was not open, exiting...\n",fd);
    system_exit(-1);
  }
}

/* 
	The method called when the SYS_OPEN is called. 
  System call format:
  int open (const char *file)
*/

static int 
system_filesize(int * arguments)
{
  int fd = ((int) arguments[0]); 
  struct thread *t = thread_current ();
  
  if (t->fd_array[fd].slot_is_empty == false) {
    int fl = ((int)file_length (t->fd_array[fd].file));
    return fl;
  } else {
    printf ("File with fd=%d was not open, exiting...\n",fd);
    system_exit(-1);
  }
}

static int
system_open(int * arguments){

    char *file_name = ((char *) arguments[0]);
    
    if (is_valid_memory_access(file_name) == false) {
      printf ("Invalid memory access at %X, exiting...\n",file_name);
      system_exit(-1);
    }

    struct file * open_file = filesys_open (file_name);
    struct thread *t = thread_current ();
  
    if (open_file != NULL) {
      struct fd_info fd_information;
      fd_information.file = open_file;
      fd_information.slot_is_empty = false;
      int fd;
      int i;
      for (i = 2; i < 18; i++) {
        if (t->fd_array[i].slot_is_empty == true) {
          fd = i;
          //printf ("chose fd=%d\n",fd);
          break;
        }
      }

      // Set the file descriptor info at the fd slot in the array
      t->fd_array[fd] = fd_information;
   
      // Return the file descriptor
      return fd;

    } else {
      printf ("File named %s could not be opened, exiting...\n",file_name);
      system_exit(-1);
    }
}

static void system_exit(int status){
  struct thread * current_thread = thread_current();
  printf("%s: exit(%d)\n", current_thread->name, status);
  set_exit_status_of_child(current_thread->parent, (int) current_thread->tid, status);
  thread_exit();

}


/* 
  The method called when SYS_READ is called.
  System call format:
  int read (int fd, void *buffer, unsigned size)
*/
static int
system_read(int * arguments){
  int fd = ((int) arguments[0]); 
  const void *buffer = ((void *) arguments[1]);
  unsigned size = ((unsigned) arguments[2]);
  
  struct thread *t = thread_current ();
  
  if (is_valid_memory_access (buffer) == false) {
    printf ("Invalid memory access at %X, exiting...\n",buffer);
    system_exit(-1);
  }

  buffer = pagedir_get_page(thread_current()->pagedir, buffer);

  if (fd == 0) {
    // READ FROM CONSOLE
    // uint8_t input_getc (void) 
    buffer = input_getc ();
    /* TODO: Implement reading from the console. */

  } else if (fd > 1 && fd < 18) {
    // READ FROM A FILE
      if (t->fd_array[fd].slot_is_empty == false) {
      //off_t file_read (struct file *, void *, off_t);
      
      return file_read (t->fd_array[fd].file, buffer, size);
    } else {
      printf ("File with fd=%d was not open so could not be read, exiting...\n",fd);
      system_exit(-1);
    }
  } else {
    // INVALID FILE DESCRIPTOR
    printf ("Invalid file descriptor of %d, exiting...\n",fd);
    system_exit(-1);
  }
}


/* 
  The method called when the SYS_WRITE is called. 
  System call format: 
  int write (int fd, const void *buffer, unsigned size)
*/
static unsigned
system_write(int * arguments){
  // Assumes that items are pushed onto the stack from right to left
  int fd = ((int) arguments[0]); 
  const void *buffer = ((void *) arguments[1]);
  unsigned size = ((unsigned) arguments[2]);
  unsigned size_written = 0;


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
      return size;
    } else {
      // Break the write into smaller chunks
      while (size > WRITE_CHUNK_SIZE) {
        putbuf (buffer, size);
        size -= WRITE_CHUNK_SIZE;
        size_written += WRITE_CHUNK_SIZE;
      }
      return size_written;
    }
  } else {
    // Writing to a file with a file descriptor fd

    lock_acquire (&file_sys_lock);
    //file_write ();
    lock_release (&file_sys_lock);    
    //ASSERT (thread_current ()->fd_array[fd]); // Must be an open file
    
  }

}

static bool system_create(int * arguments){
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

  //printf ("--- valid_memory_access ---\n");


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
