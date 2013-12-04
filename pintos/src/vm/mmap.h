#ifndef VM_MMAP_H_
#define VM_MMAP_H_

#include <list.h>
#include "lib/user/syscall.h"
#include "threads/thread.h"
#include "vm/page.h"

void init_mmapt(void);
void add_entry_mmapt(mapid_t mapid, struct spt_entry * spt_entry);
struct list * get_entries_mmapt(mapid_t mapid);
void remove_entry_mmapt(mapid_t mapid);

#endif