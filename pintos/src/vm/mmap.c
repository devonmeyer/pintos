#include "vm/mmap.h"

void
init_mmapt(void)
{
	int i;
	for(i = 0; i < 16; i++) {
		list_init(thread_current()->mmap_table[i]);
	}
}

void 
add_entry_mmapt(mapid_t mapid, struct spt_entry * spt_entry)
{
	list_push_back (thread_current()->mmap_table[mapid], &(spt_entry->list_elem));
}

struct list * 
get_entries_mmapt(mapid_t mapid)
{
	return thread_current()->mmap_table[mapid];
}

void 
remove_entry_mmapt(mapid_t mapid)
{
	struct list * list = thread_current()->mmap_table[mapid];
	struct list_elem * e;
	for (e = list_begin (list); e != list_end (list); e = list_next (e)){
		list_remove(e);
    }
}
