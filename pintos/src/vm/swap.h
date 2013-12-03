#ifndef VM_SWAP_H_
#define VM_SWAP_H_

#include <list.h>
#include <bitmap.h>
#include "devices/block.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/frame.h"
#include "vm/page.h"

#define SWAP_FREE 0
#define SWAP_NOT_FREE 1
#define SECTORS_PER_PAGE (PGSIZE/BLOCK_SECTOR_SIZE)
#define BITMAP_START 0

struct lock st_lock;			// Lock to prevent multi-threaded access
struct block * swap_block;		// Block device partitioned for swap
struct bitmap * st_bitmap;		// One to one bitmap of sectors to keep track of swap slots

void init_st(void);
bool swap_frame_out_st(void* frame_number);
bool swap_frame_in_st(block_sector_t sector);
void free_all_swap_slots_for_current_thread_st(void);

/*

SCENARIOS FOR SWAP TABLE:

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