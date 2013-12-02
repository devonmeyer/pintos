#include "vm/page.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "lib/kernel/hash.h"
#include "filesys/file.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "vm/page.h"
#include "vm/frame.h"


// Static method declarations:
static unsigned spt_entry_hash (const struct hash_elem *spte_, void *aux UNUSED);
static bool spt_entry_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);



/* Initialize the supplemental page table. */
void
init_spt (struct hash *sup_page_table) {
	hash_init (sup_page_table, spt_entry_hash, spt_entry_less, NULL);
}


/* Returns a hash value for supplemental page table entry spte. */
static unsigned
spt_entry_hash (const struct hash_elem *spte_, void *aux UNUSED)
{
  const struct spt_entry *spte = hash_entry (spte_, struct spt_entry, hash_elem);
  return hash_bytes (&spte->page_num, sizeof spte->page_num);
}


/* Returns true if supplemental page table entry a precedes supplemental page table entry b. */
static bool
spt_entry_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED)
{
  const struct spt_entry *a = hash_entry (a_, struct spt_entry, hash_elem);
  const struct spt_entry *b = hash_entry (b_, struct spt_entry, hash_elem);

  return a->page_num < b->page_num;
}


/* Add an entry to the supplemental page table. */
void 
add_entry_spt(struct hash *sup_page_table, void *vaddr, struct file *f, bool in_swap, bool mem_mapped_io) {
	struct spt_entry *spte = malloc(sizeof(struct spt_entry));
	spte->file = f;
	spte->page_num = pg_no(vaddr);
  spte->in_swap = in_swap;
  spte->mem_mapped_io = mem_mapped_io;

	hash_insert (sup_page_table, &spte->hash_elem);
}

/* Create a new entry and add it to the supplemental page table.
   Returns TRUE if operation succeeds, FALSE otherwise. */
bool 
create_entry_spt(struct hash *sup_page_table, void *vaddr) {
  struct spt_entry *spte = malloc(sizeof(struct spt_entry));
  
  uint32_t *kpage;
  bool success = false;

  // (1) Allocate the new frame
  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  
  if (kpage != NULL) {

    // (2) Add the mapping for the new frame to the Hardware Page Table
    success = install_page (vaddr, kpage, true); 
    if (success == false) {
      palloc_free_page (kpage);
    }
  }

  if (success == true) {
    // (3) Create the Supplemental Page Table entry
    spte->page_num = pg_no (vaddr);
    spte->frame_num = pg_no (kpage);
    spte->in_swap = false;
    spte->mem_mapped_io = false;

    // (4) Add the frame-to-page mapping to the Frame Table
    add_entry_ft (spte->frame_num, spte->page_num);

    // hash_insert returns a null pointer if no element equal to element previously existed in it
    return (hash_insert (sup_page_table, &spte->hash_elem) == NULL); 
  } else {
    free(spte);
    return false;
  }
}

/* Returns the supplemental page table entry containing the given
   virtual address, or a null pointer if no such entry exists. */
struct spt_entry *
get_entry_spt(struct hash *sup_page_table, const void *vaddr) { 	
  struct spt_entry spte;
  struct hash_elem *e;

  spte.page_num = pg_no (vaddr);
  e = hash_find (sup_page_table, &spte.hash_elem);
  return e != NULL ? hash_entry (e, struct spt_entry, hash_elem) : NULL;
}


/* Returns TRUE if the page at the given virtual address is stored in
   the swap partition. */
bool 
page_is_in_swap (struct hash *sup_page_table,const void *vaddr) {
  return get_entry_spt (sup_page_table, vaddr)->in_swap;
}


/* Notifies the supplemental page table that the page at virtual address VADDR
   is getting swapped out, that is, stored to the swap partition. Returns TRUE
   if the page lookup was successful (if the page was in the supplemental page
   table) and FALSE otherwise. */
bool
swap_page_out (struct hash *sup_page_table, const void *vaddr) {
  struct spt_entry *spte = get_entry_spt (sup_page_table, vaddr);
  if (spte == NULL) {
    return false;
  } else {
    spte->in_swap = false;
    return true;
  }
}

/* Notifies the supplemental page table that the page at virtual address VADDR
   is being swapped in, that is, stored to memory from the swap partition. 
   Returns TRUE if the page lookup was successful (if the page was in the 
   supplemental page table) and FALSE otherwise. */
bool
swap_page_in (struct hash *sup_page_table, const void *vaddr) {
  struct spt_entry *spte = get_entry_spt (sup_page_table, vaddr);
  if (spte == NULL) {
    return false;
  } else {
    spte->in_swap = true;
    return true;
  }
}


/* Returns a list of all of the page numbers of pages that are swapped out for
   this supplemental page table. */
struct list 
get_all_swapped_out_page_nums (struct hash *sup_page_table) {
  struct list swapped_out_page_nums;
  list_init (&swapped_out_page_nums);

  struct hash_iterator i;

  hash_first (&i, sup_page_table);

  while (hash_next (&i))
  {
    struct spt_entry *spte = hash_entry (hash_cur (&i), struct spt_entry, hash_elem);
    if (spte->in_swap == true) {
      struct page_num_item pni;
      pni.page_num = spte->page_num;
      list_push_back (&swapped_out_page_nums, &pni.elem);
    }
  }

  return swapped_out_page_nums;
}







