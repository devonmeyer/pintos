#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"

struct list ft_list;

struct ft_entry {
	void * frame_number;
	void * page_number;
	struct thread * t;
	struct list_elem elem;
};

struct lock ft_lock;

void initialize_ft (void);
void add_entry_ft (void * frame, void * page);
void remove_entry_ft (void * frame);
void * get_frame_to_evict (void);
void reclaim_frames(void);