#ifndef VM_MMAP_H_
#define VM_MMAP_H_

#include <list.h>
#include "lib/user/syscall.h"
#include "threads/thread.h"
#include "vm/page.h"

void init_mmapt(void);
void add_mmapt_entry(int mapid, struct spt_entry * spt_entry);
struct list * get_mmapt_entries(int mapid);
void remove_mmapt_entry(int mapid);

#endif