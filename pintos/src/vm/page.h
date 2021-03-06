#ifndef VM_PAGE_H_
#define VM_PAGE_H_

#include <hash.h>
#include <list.h>
#include "lib/user/syscall.h"
#include "threads/thread.h"

struct spt_entry {
	struct hash_elem hash_elem; /* Hash table element. */
	struct list_elem list_elem; /* To be used in the Mapid Table. */
	void *page_num; /* The virtual page number, acts as the key for the hash table. */
	void *frame_num; /* The physical frame number. */
	int sector_num; /* The sector number representing the beginning of the swap slot. */
	int file_offset; /* Where in the file this is mem_mapping begins. */
	struct file * file;
	mapid_t mapid;
	bool in_swap;
};

struct sector_item {
	struct list_elem elem; /* List element. */
	int sector_num; /* The sector number. */
};

void init_spt (struct hash * h);
bool handle_page_fault_spt(struct spt_entry * spte);
bool create_entry_spt(void *vaddr);
void mmap_spt(void *page_num, struct file *f, int file_offset, mapid_t mapid);
void munmap_spt(mapid_t mapid);
struct spt_entry* get_entry_spt(const void *page_num);
bool page_is_in_swap_spt (const void *page_num);
bool swap_page_out_spt (const void *page_num, int sector_num);
bool swap_page_in_spt (const void *page_num);
struct list *get_all_swapped_out_sector_nums_spt (void);
struct spt_entry *get_entry_from_vaddr_spt(const void *vaddr);
bool get_entry_from_swap_spt (const void *page_num);

#endif /* PAGE.H */
