#ifndef PAGE_H_
#define PAGE_H_


struct spt_entry {
	struct hash_elem hash_elem; /* Hash table element. */
	void *page_num; /* The virtual page number, acts as the key for the hash table. */
	void *frame_num; /* The physical frame number. */
	struct file * file;
	bool in_swap;
	bool mem_mapped_io;
};

struct page_num_item {
	struct list_elem elem; /* List element. */
	void *page_num; /* The virtual page number. */
};

void init_sup_page_table (struct hash *sup_page_table);
unsigned spt_entry_hash (const struct hash_elem *spte_, void *aux UNUSED);
bool spt_entry_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
bool create_entry_spt(struct hash *sup_page_table, void *vaddr);
void add_entry_spt(struct hash *sup_page_table, void *vaddr, struct file *f, bool in_swap, bool mem_mapped_io);
struct spt_entry* get_entry_spt(struct hash *sup_page_table, const void *vaddr);
bool page_is_in_swap (struct hash *sup_page_table, const void *vaddr);
bool swap_page_out (struct hash *sup_page_table, const void *vaddr);
bool swap_page_in (struct hash *sup_page_table, const void *vaddr);
struct list get_all_swapped_out_page_nums (struct hash *sup_page_table);

#endif /* PAGE.H */
