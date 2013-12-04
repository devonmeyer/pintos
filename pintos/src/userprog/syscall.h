#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
void print_okay (void);
void system_exit(int status);

struct lock file_sys_lock;

#endif /* userprog/syscall.h */
