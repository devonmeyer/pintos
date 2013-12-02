#include "vm/swap.h"

/* Initializes the swap table by obtaining the swap block on disk
   and creating the bitmap 
 */ 
void 
init_st(void) 
{
	lock_init(&st_lock);

	swap_block = block_get_role(BLOCK_SWAP);
	if(!swap_block) {
		PANIC("Error obtaining swap block in swap table");
	}
	st_bitmap = bitmap_create(block_size(swap_block));
	if(!st_bitmap) {
		PANIC("Error creating bitmap in swap table");
	}
	bitmap_set_all(st_bitmap, SWAP_FREE);
}

/* SUMMARY: Swaps a given frame in memory into swap disk
   INPUT: Frame number to be swapped out and stored in swap disk.

   Checks if swap partition is not full, otherwise PANIC.
   Finds an empty swap slot and updates the bitmap to indicate
   it is now in use.
   Finds the page associated with given frame and notifies
   corresponding supplementary page table where the page is
   being stored in swap disk.
   Writes the frame to the swap disk.	
 */
void 
swap_frame_out_st(void* frame_number)
{
	lock_acquire(&st_lock);

	if(!swap_block || !st_bitmap) {
		PANIC("Error, swap table was not properly initalized");
	}

	block_sector_t sector = bitmap_scan_and_flip(st_bitmap, BITMAP_START, SECTORS_PER_PAGE, SWAP_FREE);

	if(sector == BITMAP_ERROR) {
		PANIC("Error, swap partition is full");
	}

	void* page_number = get_entry_ft(frame_number);
   	swap_page_out_spt(page_number, sector);

   	int i;
   	for(i = 0; i < SECTORS_PER_PAGE; i++) {
   		block_write(swap_block, sector, frame_number);
   		frame_number += BLOCK_SECTOR_SIZE;
   		sector++;
   	}

	lock_release(&st_lock);
}

/* SUMMARY: Swaps a frame in swap disk into memory
   INPUT: Initial sector where the frame to be swapped in is stored.

   Checks if designated swap slot contains valid data, otherwise PANIC.
   Updates the bitmap to indicate the swap slot located at the given
   sector is now free.
   Reads the frame from the swap disk.
 */
void
swap_frame_in_st(block_sector_t sector)
{
	lock_acquire(&st_lock);

	if(!swap_block || !st_bitmap) {
		PANIC("Error, swap table was not properly initalized");
	}

	if(bitmap_test(st_bitmap, sector) == SWAP_FREE) {
		PANIC("Error trying to swap in a free block");
	}

	bitmap_scan_and_flip(st_bitmap, sector, SECTORS_PER_PAGE, SWAP_NOT_FREE);

	void* frame_number; // NEED TO ALLOCATE THIS FRAME

	int i;
	for(i = 0; i < SECTORS_PER_PAGE; i++) {
		block_read(swap_block, sector, frame_number);
		frame_number += BLOCK_SECTOR_SIZE;
		sector++;
	}

	// swap_page_in_spt(page_number);

	lock_release(&st_lock);
}

/* SUMMARY: Frees all frames stored in swap disk for the current thread
   
   Fetches the start sectors for each page associated with the current
   thread from the thread's supplementary page table.
   FOREACH swap slot, checks if swap slot is in use, otherwise PANIC;
   	THEN free swap slot by updating the bitmap to indicate free status
   	for corresponding sectors.
 */
void 
free_all_swap_slots_for_current_thread_st(void)
{
	lock_acquire(&st_lock);

	if(!swap_block || !st_bitmap) {
		PANIC("Error, swap table was not properly initalized");
	}

	struct list* sectors_list = get_all_swapped_out_sector_nums_spt();

	struct list_elem *e;
	for (e = list_begin (sectors_list); e != list_end (sectors_list); e = list_next (e)){
		struct sector_item* entry = list_entry (e, struct sector_item, elem);

		block_sector_t sector = entry->sector_num;
		if(bitmap_test(st_bitmap, sector) == SWAP_FREE) {
			PANIC("Error trying to swap in a free sector");
		}

		int i;
		for(i = 0; i < SECTORS_PER_PAGE; i++) {
			bitmap_flip(st_bitmap, sector);
			sector++;
		}     
    }

	lock_release(&st_lock);
}
