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
#include "filesys/directory.h"
#include "devices/shutdown.h"
#include "process.h"

static void syscall_handler (struct intr_frame *);
static bool is_valid_memory_access(const void *vaddr);
static void validate_file_descriptor(const int fd);
static bool file_already_exists (const char * file_name);
static void get_arguments(struct intr_frame *f, int num_args, int * arguments);
static void debug (char * debug_msg);

static int system_open(int * arguments);
static int system_read(int * arguments);
static unsigned system_write(int * arguments);
static int system_filesize(int * arguments);
static bool system_create(int * arguments);
static unsigned system_tell(int * arguments);
static void system_close(int * arguments);
static void system_seek(struct intr_frame *f, int * arguments);

static int mem_map(int * arguments);
static void mem_unmap ( int * arguments );
static int system_exec (int * arguments);

static struct lock file_sys_lock;
bool debug_mode;

#define WRITE_CHUNK_SIZE 300

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&file_sys_lock);
  debug_mode = false;
}

static void
syscall_handler (struct intr_frame *f) 
{
  if(is_valid_memory_access((const void *) f->esp)){
    int * num = f->esp;
    int * system_call_arg = num + 1;
    if (is_user_vaddr(system_call_arg) == false) {
      // The argument to the system call is not in user space!!
      system_exit(-1);
    }
    int arguments[3];
    if (debug_mode) {
      printf("Got system call number %d\n", *num);
    }
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
      	break;
      case SYS_TELL:
        get_arguments(f, 1, arguments);
        f->eax = system_tell(arguments);
      	break;
      case SYS_CLOSE:
        get_arguments(f, 1, arguments);
        system_close(arguments);
      	break;
      case SYS_MMAP:
        get_arguments(f, 2, arguments);
        f->eax = mem_map(arguments);
        break;
      case SYS_MUNMAP:
        get_arguments(f, 1, arguments);
        f->eax = mem_unmap(arguments);
        break;
      default:
      	system_exit(-1);  
        break;
    }

    if (debug_mode) {
     printf("System call finished.\n");
    }
  } else {
    system_exit(-1);
  }
}

static void 
system_seek(struct intr_frame *f, int * arguments)
{
  int fd = ((int) arguments[0]);
  int position = ((off_t) arguments[1]);
  struct thread *t = thread_current ();

  validate_file_descriptor(fd);

  if(t->fd_array[fd] != NULL) {
    struct file * open_file = t->fd_array[fd]->file;
    file_seek(open_file, position);
  } else {
        if (debug_mode) {
    printf ("File with fd=%d was not open, exiting...\n",fd);
  }
    system_exit(-1);
  }
}



static void
system_close_all(){
  int i = 0;
  struct thread * holder = thread_current();
  for(i = 0; i < 18; i++){
    if(holder->fd_array[i] != NULL){
      system_close(&i);
    }
  }
}



/*
  System call format:
  void close (int fd)
*/
static void
system_close(int * arguments) 
{
  int fd = ((int) arguments[0]);
  if (fd == 0 || fd == 1) {
        if (debug_mode) {
    printf ("Cannot system_close(%d), exiting...\n",fd);
  }
    system_exit(-1);
  } 

  validate_file_descriptor(fd);
  struct thread *t = thread_current ();
  file_close (t->fd_array[fd]->file);
  t->fd_array[fd] = NULL;
}

 static pid_t 
 system_exec (int * arguments)
 {
  const base_args = ((char*)arguments[0]);
  // pid is mapped exactly to tid, process_execute returns tid
  if(!is_valid_memory_access(base_args)){
    system_exit(-1);
  }
  
  char buffer[100];
  strlcpy(buffer, base_args, (strlen(base_args)+1));
  char *save_ptr; // Address used to keep track of tokenizer's position
  char *file_name = strtok_r(buffer, " ", &save_ptr);

  if(!file_already_exists(file_name)) {
        if (debug_mode) {
    printf("No file named %s exists, exiting...\n", file_name);
  }
    return -1;
  }
  const void * args = pagedir_get_page(thread_current()->pagedir, arguments[0]);
  pid_t pid = ((pid_t) process_execute((char *) args));
  if(pid == TID_ERROR){
    return -1;
  }
  set_child_of_thread((int) pid);
  return pid;
 }



/*
  System call format:
  unsigned tell (int fd)
*/
static unsigned 
system_tell(int * arguments)
{
  int fd = ((int) arguments[0]);
  validate_file_descriptor(fd); 
  
  struct thread *t = thread_current ();
  
  if (t->fd_array[fd] != NULL) {
    unsigned tell = ((unsigned)file_tell (t->fd_array[fd]->file));
    return tell;
  } else {
        if (debug_mode) {
    printf ("File with fd=%d was not open, exiting...\n",fd);
  }
    system_exit(-1);
  }
}



/* 
  System call format:
  int open (const char *file)
*/
static int 
system_filesize(int * arguments)
{
  int fd = ((int) arguments[0]); 
  validate_file_descriptor(fd);

  struct thread *t = thread_current ();
  
  if (t->fd_array[fd] != NULL) {
    int fl = ((int)file_length (t->fd_array[fd]->file));
    return fl;
  } else {
        if (debug_mode) {
    printf ("File with fd=%d was not open, exiting...\n",fd);
       }
    system_exit(-1);
  }
}



/*
  System call format:
  int open (const char *file)
*/
static int
system_open(int * arguments){
    char *file_name = ((char *) arguments[0]);

if (is_valid_memory_access(file_name) == false) {
          if (debug_mode) {
      printf ("Invalid memory access at %X, exiting...\n",file_name);
    }
      system_exit(-1);
    }

    if (file_name == NULL || strlen(file_name) == 0) {
          if (debug_mode) {
      printf("Empty file name string for system_open(), exiting...\n");
    }
      return -1;
    }
    
    

    struct file * open_file = filesys_open (file_name);
    struct thread *t = thread_current ();
  
    if (open_file != NULL) {
      struct fd_info * fd_information = malloc(sizeof(struct fd_info));
      fd_information->file = open_file;
      int fd;
      int i;
      for (i = 2; i < 18; i++) {
        if (t->fd_array[i] == NULL) {
          fd = i;
          break;
        }
      }

      // Set the file descriptor info at the fd slot in the array
      t->fd_array[fd] = fd_information;
   
      // Return the file descriptor
      return fd;

    } else {
          if (debug_mode) {
      printf ("File named %s could not be opened, exiting...\n",file_name);
    }
      return -1;
    }
}

void
system_exit(int status){
  struct thread * current_thread = thread_current();
  printf("%s: exit(%d)\n", current_thread->name, status);
//<<<<<<< HEAD
  set_exit_status_of_child(current_thread->parent, (int) current_thread->tid, status);
  // NOT SURE IF THE BOTTOM CODE IS CORRECT OR NOT, BUT IT WAS CAUSING INFINITE LOOP
/*=======
  if (thread_exists(current_thread->parent)){
    set_exit_status_of_child(current_thread->parent, (int) current_thread->tid, status);
  }
}
>>>>>>> 294f5e9a46575bf86ff01c771745032cacebd97a*/
  thread_exit();
}


/* 
  System call format:
  int read (int fd, void *buffer, unsigned size)
*/
static int
system_read(int * arguments){
  int fd = ((int) arguments[0]); 
  validate_file_descriptor(fd);

  const void *buffer = ((void *) arguments[1]);
  unsigned size = ((unsigned) arguments[2]);
  
  struct thread *t = thread_current ();
  
  if (is_valid_memory_access (buffer) == false) {
        if (debug_mode) {
    printf ("Invalid memory access at %X, exiting...\n",buffer);
  }
    system_exit(-1);
  }

  buffer = pagedir_get_page(thread_current()->pagedir, buffer);

  if (fd == 0) {
    // READ FROM THE CONSOLE 
    while (size > 0) {
      // Read one byte at a time from the console, for a total of SIZE bytes
      char c = ((char)input_getc());
      void *input = &c;
      memcpy (buffer, input, 1); 
      size--;
    }

  } else if (fd > 1 && fd < 18) {
    // READ FROM A FILE
      if (t->fd_array[fd] != NULL) {      
      return file_read (t->fd_array[fd]->file, buffer, size);
    } else {
          if (debug_mode) {
      printf ("File with fd=%d was not open so could not be read, exiting...\n",fd);
    }
      system_exit(-1);
    }
  } else {
    // INVALID FILE DESCRIPTOR
        if (debug_mode) {
    printf ("Invalid file descriptor of %d, exiting...\n",fd);
  }
    system_exit(-1);
  }
}


/* 
  System call format: 
  int write (int fd, const void *buffer, unsigned size)
*/
static unsigned
system_write(int * arguments){
  // Assumes that items are pushed onto the stack from right to left
  int fd = ((int) arguments[0]); 
  validate_file_descriptor(fd);

  const void *buffer = ((void *) arguments[1]);
  unsigned size = ((unsigned) arguments[2]);
  unsigned size_written = 0;


  if (is_valid_memory_access (buffer) == false) {
    system_exit(-1);
  }

  if (fd == 0) {
        if (debug_mode) {
    printf("File descriptor of 0 is invalid for system_write().\n");
  }
    system_exit(-1);
  }

  buffer = pagedir_get_page(thread_current()->pagedir, buffer);

  if (fd == 1) {
    // WRITE TO THE CONSOLE
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
    // WRITE TO A FILE
    lock_acquire (&file_sys_lock);
    size_written = file_write (thread_current ()->fd_array[fd]->file, buffer, size);
    lock_release (&file_sys_lock);

    return size_written;
  }
}


/*
  System call format:
  bool create (const char *file, unsigned initial_size)
*/
static bool system_create(int * arguments){
  bool result = false;
  const char *file_name = ((char*) arguments[0]);
  const unsigned initial_size = ((unsigned) arguments[1]);
  
  if(!is_valid_memory_access(file_name)) {
        if (debug_mode) {
    printf("Invalid system_create pointer, exiting...\n");
  }
    system_exit(-1);
  }

  if (strlen(file_name) != 0) {
    //const char * created_file = (char *) pagedir_get_page(thread_current()->pagedir, (void *) arguments[0]);
    lock_acquire(&file_sys_lock);
    result = filesys_create(file_name, initial_size);
    lock_release(&file_sys_lock);
  }
  
  if (result == false) {
        if (debug_mode) {
    printf ("system_create returns FALSE\n");
  }
  }

    return result;
}


/* 
  Checks to see if a file named FILE_NAME already exists in the file system.
*/
static bool
file_already_exists (const char * file_name) {
  struct dir *dir = dir_open_root ();
  struct inode *inode = NULL;
  bool exists = false;
  if (dir != NULL) {
    exists = dir_lookup (dir, file_name, &inode);
    dir_close (dir);
  } else {
    exists = false;
  }
  return exists;
}

/* 
  Validates the file descriptor.  Calls system_exit if invalid file descriptor.
*/
static void
validate_file_descriptor(const int fd) {

  if (fd == 0 || fd == 1) {
    // STDIN and STDOUT File Descriptors are valid
    return;
  }

  if (fd < 0 || fd >= 18) {
        if (debug_mode) {
    printf("Invalid fie descriptor of value %d.\n");
  }
    system_exit(-1);
  } else if (thread_current ()->fd_array[fd] == NULL) {
        if (debug_mode) {
    printf("File descriptor %d not found in the fd_array.\n", fd);
  }
    system_exit(-1);
  }
}


/* 
	Returns true if the memory address is valid and can be accessed by the user process.
*/
static bool
is_valid_memory_access(const void *vaddr) {

	if (vaddr == NULL) {
		// Null Pointer
        if (debug_mode) {
    printf ("--- addr is null pointer ---\n");
  }
		return false;
	} else if (is_kernel_vaddr(vaddr)) { 
		// Pointer to Kernel Virtual Address Space
        if (debug_mode) {
    printf ("--- is kernel virtual address ---\n");
  }
		return false;
	} else if (pagedir_get_page(thread_current ()->pagedir, vaddr) == NULL) { 
		// Pointer to Unmapped Virtual Memory
        if (debug_mode) {
    printf ("--- Unmapped ---\n");
  }
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

static void
debug (char * debug_msg) {
  if (debug_mode == true) {
    printf("%s",debug_msg);
  }
}

/*
  System Call: 
  mapid_t mmap (int fd, void *addr)
*/
static int
mem_map ( int * arguments ){
  int fd = ((int) arguments[0]); 
  void *addr = ((void*) arguments[1]);
  struct thread *t = thread_current ();

  /* ERROR CHECKING: */

  // Cannot mem_map the console in or console out
  if (fd == 0 || fd == 1) {
    return -1;
  }

  // Cannot mem_map an invalid fd
  if (t->fd_array[fd] == NULL) {
    return -1;
  }

  // Cannot mem_map a file with a length of zero bytes
  int fl = ((int)file_length (t->fd_array[fd]->file));
  if (fl == 0) {
    return -1;
  }

  // Cannot mem_map a file with an address of 0 because 
  // some Pintos code assumes virtual page 0 is not mapped
  if (addr == 0) {
    return -1;
  }

  // Cannot mem_map a file whose address is not page-aligned
  if (pg_round_down(addr) != addr) {
    return -1;
  }

  //The range of pages mapped overlaps any exisitng set of mapped pages
  /* TODO: ERROR CHECKING HERE. */


  /* Allocate the full number of entries in the Supplemental Page Table, 
     but don't allocate the frames from the Frame Table, thus it is LAZY. */
  int num_mmap_pages = (fl / PGSIZE) + 1; // +1 to cover any "tail" sticking out beyond 
  int i;
  int page_num = ((int)pg_no(addr));
  int mapid = t->mapid_counter;

  for (i = 1; i <= num_mmap_pages; i++) {
    mmap_spt(((void*)page_num), t->fd_array[fd]->file, i*PGSIZE, t->mapid_counter);
    page_num += PGSIZE;
  }

  t->mapid_counter++;

  return mapid;
}


/*
  System Call: 
  void munmap (mapid_t mapping)
*/
static void
mem_unmap ( int * arguments ){
  mapid_t mapid = ((mapid_t) arguments[0]); 

}










