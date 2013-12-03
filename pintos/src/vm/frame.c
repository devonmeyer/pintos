#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/page.h"

void
initialize_ft (void){
	lock_init(&ft_lock);
	list_init(&ft_list);
}

void *
allocate_frame_ft (void * vaddr) {

  // (1) Allocate the new frame
  void * new_frame = palloc_get_page (PAL_USER | PAL_ZERO);
  while ( new_frame == NULL ){
  	// Must perform an eviction
  	void * evict = get_frame_to_evict();
  	if (evict == NULL){
  		// Could not get a frame to evict.
  		return NULL;
   	}

  	if ( swap_frame_out_st ( evict ) ) {

  		// Swap succeeded, continue

  		remove_entry_ft ( evict );
  		palloc_free_page ( evict );

  	} else {

  		// Swap didn't succeed.
  		// Due to a kernel panic, we should never reach this.
  		// But just in case, I fail gracefully! :D
  		return NULL;

  	}


  }
  add_entry_ft (new_frame, pg_no(vaddr));
  if (!install_page (pg_round_down(vaddr), new_frame, true)) {

    palloc_free_page (new_frame);
   	return NULL;

  }
  return new_frame;

}

void
add_entry_ft (void * frame, void * page){
	struct ft_entry *entry = malloc(sizeof(struct ft_entry));
	lock_acquire(&ft_lock);
	entry->frame_number = frame;
	entry->page_number = page;
	entry->t = thread_current();
	list_push_back(&ft_list, &entry->elem);
	lock_release(&ft_lock);
}

void
remove_entry_ft (void * frame){
	struct list_elem *e;
	struct ft_entry *entry;
	lock_acquire(&ft_lock);

	for (e = list_begin (&ft_list); e != list_end (&ft_list); e = list_next (e)) {
	  	entry = list_entry (e, struct ft_entry, elem);
	  	if(entry->frame_number == frame){
	  		list_remove(&entry->elem); //used to be (&ft_list->elem)
	  		break;
	  	}
  	}
  	lock_release(&ft_lock);
  	free(entry);
}

void *
get_entry_ft (void * frame){
	struct list_elem *e;
	struct ft_entry *entry;
	lock_acquire(&ft_lock);

	for (e = list_begin (&ft_list); e != list_end (&ft_list); e = list_next (e)) {
	  	entry = list_entry (e, struct ft_entry, elem);
	  	if(entry->frame_number == frame){
	  		lock_release(&ft_lock);
	  		return entry->page_number;
	  	}
  	}
  	lock_release(&ft_lock);
  	return NULL;
}


void *
get_frame_to_evict (void){
	struct list_elem *e;

	lock_acquire(&ft_lock);

	struct ft_entry * entries[4];
	int i;
	for (i = 0; i < 4; i++){
		entries[i] = NULL;
	}

	for (e = list_begin (&ft_list); e != list_end (&ft_list); e = list_next (e)) {
	  	struct ft_entry *entry = list_entry (e, struct ft_entry, elem);
	  	bool accessed = pagedir_is_accessed(entry->t->pagedir, entry->page_number);
	  	bool dirty = pagedir_is_dirty(entry->t->pagedir, entry->page_number);
	  	if(accessed && dirty){
	  		entries[3] = entry;
	  	}
	  	if(accessed && !dirty){
	  		entries[2] = entry;
	  	}
	  	if(!accessed && dirty){
	  		entries[1] = entry;
	  	}
	  	if(!accessed && !dirty){
	  		entries[0] = entry;
	  	}

  	}

  	for (i = 0; i < 4; i++){
  		if(entries[i] != NULL){
  			lock_release(&ft_lock);
  			return entries[i]->frame_number;
  		}
	}
  	lock_release(&ft_lock);
  	return NULL;


}

void
reclaim_frames(void){
	struct list_elem *e;
	lock_acquire(&ft_lock);
	struct thread * ct = thread_current();
	for (e = list_begin (&ft_list); e != list_end (&ft_list); e = list_next (e)) {
	  	struct ft_entry *entry = list_entry (e, struct ft_entry, elem);
	  	if(entry->t == ct){
	  		list_remove(&entry->elem);
  		  	free(entry);
	  	}
  	}
  	lock_release(&ft_lock);
}