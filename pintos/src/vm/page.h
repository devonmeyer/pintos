#ifndef VM_PAGE_H_
#define VM_PAGE_H_

struct spt_entry {
	struct hash_elem hash_elem; /* Hash table element. */
	void *page_num; /* The virtual page number, acts as the key for the hash table. */
	void *frame_num; /* The physical frame number. */
	int sector_num; /* The sector number representing the beginning of the swap slot. */
	struct file * file;
	bool in_swap;
	bool mem_mapped_io;
};

struct page_num_item {
	struct list_elem elem; /* List element. */
	int sector_num; /* The sector number. */
};

void init_spt (struct hash *sup_page_table);
bool create_entry_spt(struct hash *sup_page_table, void *vaddr);
void add_entry_spt(struct hash *sup_page_table, void *vaddr, struct file *f, bool in_swap, bool mem_mapped_io);
struct spt_entry* get_entry_spt(struct hash *sup_page_table, const void *vaddr);
bool page_is_in_swap_spt (struct hash *sup_page_table, const void *vaddr);
bool swap_page_out_spt (struct hash *sup_page_table, const void *vaddr, int sector_num);
bool swap_page_in_spt (struct hash *sup_page_table, const void *vaddr);
struct list get_all_swapped_out_sector_nums_spt (struct hash *sup_page_table);

#endif /* PAGE.H */
