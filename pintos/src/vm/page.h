#ifndef PAGE_H_
#define PAGE_H_

#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "lib/kernel/hash.h"
#include "filesys/file.h"
#include "threads/vaddr.h"


struct hash sup_page_table;

struct spt_entry {
	struct hash_elem hash_elem; /* Hash table element. */
	void *page_num; /* The virtual page number, acts as the key for the hash table. */
	struct file * file;
	bool in_swap;
	bool mem_mapped_io;
};

void init_spt(void);

unsigned spt_entry_hash (const struct hash_elem *spte_, void *aux UNUSED);
bool spt_entry_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
void add_entry(void *vaddr, struct file *f, bool in_swap, bool mem_mapped_io);
struct spt_entry* get_entry(const void *vaddr);
bool page_is_in_swap (const void *vaddr);

#endif /* PAGE.H */
