#include "vm/page.h"


/* Initialize the supplemental page table. */
void
init_spt (void) {
	hash_init (&sup_page_table, spt_entry_hash, spt_entry_less, NULL);
}


/* Returns a hash value for supplemental page table entry spte. */
unsigned
spt_entry_hash (const struct hash_elem *spte_, void *aux UNUSED)
{
  const struct spt_entry *spte = hash_entry (spte_, struct spt_entry, hash_elem);
  return hash_bytes (&spte->page_num, sizeof spte->page_num);
}


/* Returns true if supplemental page table entry a precedes supplemental page table entry b. */
bool
spt_entry_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED)
{
  const struct spt_entry *a = hash_entry (a_, struct spt_entry, hash_elem);
  const struct spt_entry *b = hash_entry (b_, struct spt_entry, hash_elem);

  return a->page_num < b->page_num;
}


/* Add an entry to the supplemental page table. 
	SWAP_SLOT = -1 if it is memory mapped IO. 
	Otherwise, it is the valid index into the swap table. */
void 
add_entry(void *vaddr, struct file *f, bool in_swap, bool mem_mapped_io) {
	struct spt_entry *spte = malloc(sizeof(struct spt_entry));
	spte->file = f;
	spte->page_num = pg_no(vaddr);
  spte->in_swap = in_swap;
  spte->mem_mapped_io = mem_mapped_io;

	hash_insert (&sup_page_table, &spte->hash_elem);
}


/* Returns the supplemental page table entry containing the given
   virtual address, or a null pointer if no such entry exists. */
struct spt_entry *
get_entry(const void *vaddr) { 	
  struct spt_entry spte;
  struct hash_elem *e;

  spte.page_num = pg_no (vaddr);
  e = hash_find (&sup_page_table, &spte.hash_elem);
  return e != NULL ? hash_entry (e, struct spt_entry, hash_elem) : NULL;
}


/* Returns TRUE if the page at the given virtual address is stored in
   the swap partition. */
bool 
page_is_in_swap (const void *vaddr) {
  return get_entry (vaddr)->in_swap;
}








