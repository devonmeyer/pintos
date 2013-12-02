#ifndef VM_FRAME_H_
#define VM_FRAME_H_

#include "threads/synch.h"

struct list ft_list;

struct ft_entry {
	void * frame_number;
	void * page_number;
	struct thread * t;
	struct list_elem elem;
};

struct lock ft_lock;

void initialize_ft ( void );
void add_entry_ft ( void * frame, void * page );
void remove_entry_ft ( void * frame );
void * get_entry_ft ( void * frame );
void * get_frame_to_evict ( void );
void reclaim_frames ( void );

#endif