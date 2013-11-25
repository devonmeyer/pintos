#include "vm/frame.h"

void
initialize_ft (void){
	lock_init(&ft_lock);
	list_init(&ft_list);
}


void
insert_entry_ft (void * frame, void * page){
	struct ft_entry *entry = malloc(sizeof(struct ft_entry));
	lock_acquire(&ft_lock);
	ft_entry->frame_number = frame;
	ft_entry->page_number = page;
	ft_entry->t = thread_current();
	list_push_back(&ft_list, &ft_entry->elem);
	lock_release(&ft_lock);
}