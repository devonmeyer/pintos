#ifndef VM_SWAP_H_
#define VM_SWAP_H_

#include "devices/block.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/frame.h"
#include "vm/page.h"
#include <bitmap.h>

#define SWAP_FREE 0
#define SWAP_NOT_FREE 1
#define SECTORS_PER_PAGE (PGSIZE/BLOCK_SECTOR_SIZE)
#define BITMAP_START 0

struct list st_list;
struct lock st_lock;
struct block* swap_block;
struct bitmap* st_bitmap;

struct st_entry {
	void* frame_number;
	block_sector_t sector;
	struct thread* thread;
	struct list_elem elem;
};

void init_st(void);
void swap_frame_out_st(void* frame_number);
void swap_frame_in_st(block_sector_t sector);
void free_all_swap_slots(struct thread* thread);

/*

Scenarios for swap table:

1. Eviction, given a frame, should store the frame in a swap slot
	- INPUT: Frame, OUTPUT: void
	- Find free swap slot
	- Store frame in swap slot
	- Update sup page table
	- Return void

2. Page is in swap disk, needs frame
	- INPUT: Sector, OUTPUT: Frame
	- Find swap slot associated with given page
	- Swap the swap slot back into memory, evict if memory is full
	- Free swap slot
	- Return frame in memory

3. Process termination, free all swap slots for that process
	- INPUT: void, OUTPUT: void
	- Access current thread's pages in swap through sup page table
	- Free swap slot associated with each page in swap
	- Return void

*/

#endif