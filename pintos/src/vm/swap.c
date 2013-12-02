#include "vm/swap.h"

void 
init_st(void) 
{
	list_init(&st_list);
	lock_init(&st_lock);

	swap_block = block_get_role(BLOCK_SWAP);
	st_bitmap = bitmap_create(block_size(swap_block));
	bitmap_set_all(st_bitmap, SWAP_FREE);
}

void 
swap_frame_out_st(void* frame_number)
{
	lock_acquire(&st_lock);

	block_sector_t sector = bitmap_scan_and_flip(st_bitmap, BITMAP_START, SECTORS_PER_PAGE, SWAP_FREE);

	struct st_entry *entry = malloc(sizeof(struct st_entry));
	entry->frame_number = frame_number;
	entry->thread = thread_current();
	entry->sector = sector;

	void* page_number = get_entry_ft(frame_number);
   	swap_page_out_spt(page_number, sector);

   	int i;
   	for(i = 0; i < SECTORS_PER_PAGE; i++) {
   		block_write(swap_block, sector, frame_number);
   		frame_number += BLOCK_SECTOR_SIZE;
   		sector++;
   	}

	list_push_back(&st_list, &entry->elem);

	lock_release(&st_lock);
}

void 
swap_frame_in_st(block_sector_t sector)
{
	lock_acquire(&st_lock);

	lock_release(&st_lock);
}

void 
free_all_swap_slots(struct thread* thread)
{
	lock_acquire(&st_lock);

	// struct list* sectors_list = get_all_swapped_out_sector_nums_spt();

	// struct list_elem *e;
	// for (e = list_begin (&sectors_list); e != list_end (&sectors_list); e = list_next (e))
	// {
 //      struct page_num_item = list_entry (e, struct page_num_item, elem);
 //      if(t_exists->tid == t->tid){
 //        return true;
 //      }      
 //    }

	lock_release(&st_lock);
}

