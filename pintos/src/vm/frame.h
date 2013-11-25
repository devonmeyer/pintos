#include "threads/thread.h"
#include "threads/synch.h"

struct list ft_list;

struct ft_entry {
	void * frame_number;
	void * page_number;
	struct thread * t;
	struct list_elem elem;
};

struct lock ft_lock;

void initialize_ft (void);
void insert_entry_ft (void * frame, void * page);