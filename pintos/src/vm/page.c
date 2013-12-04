#include "vm/page.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "filesys/file.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "vm/frame.h"


// Static method declarations:
static unsigned spt_entry_hash (const struct hash_elem *spte_, void *aux UNUSED);
static bool spt_entry_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
static bool allocate_new_frame(struct spt_entry *spte);


/* Initialize the supplemental page table. */
void
init_spt (struct hash * h) {
	hash_init (h, spt_entry_hash, spt_entry_less, NULL);
}

/* Called by the page fault handler in exception.c. Passes responsibility
   to the Supplemental Page Table to address page faults. */
bool handle_page_fault_spt(struct spt_entry * spte) {
  if (spte->in_swap) {
    // STORED IN SWAP PARTITION
    get_entry_from_swap_spt(spte->page_num);
  } else if (spte->mapid != -1){
    // MEMORY MAPPED FILE
    uint32_t vaddr = ((uint32_t) spte->page_num) << 12; // This is a potential source of error
    uint32_t *kpage = allocate_frame_ft(vaddr);                                           

    if (kpage != NULL) {
      spte->frame_num = pg_no (kpage);      

      // Actually write from the file to the memory frame:      
      file_read_at (spte->file, kpage, PGSIZE, spte->file_offset);       

      add_entry_ft (spte->frame_num, spte->page_num); // Add the frame-to-page mapping to the Frame Table
    } else {
      return false;
    }
    return true;
  }
}

/* Returns a hash value for supplemental page table entry spte. */
static unsigned
spt_entry_hash (const struct hash_elem *el, void *aux UNUSED)
{
  struct spt_entry *entry = hash_entry (el, struct spt_entry, hash_elem);
  return hash_int ((int) entry->page_num);
}


/* Returns true if supplemental page table entry a precedes supplemental page table entry b. */
static bool
spt_entry_less (const struct hash_elem *a, const struct hash_elem *b,
           void *aux UNUSED)
{
  struct spt_entry *my_a = hash_entry (a, struct spt_entry, hash_elem);
  struct spt_entry *my_b = hash_entry (b, struct spt_entry, hash_elem);

  return my_a->page_num < my_b->page_num;
}


/* Add an entry for mem_map to the supplemental page table for use with 
   memory mapping. */
void 
mmap_spt(void *page_num, struct file *f, int file_offset, mapid_t mapid) {
	struct spt_entry *spte = malloc(sizeof(struct spt_entry));
	spte->file = f;
	spte->page_num = page_num;
  spte->mapid = mapid;
  spte->in_swap = false;
  spte->file_offset = file_offset;

	hash_insert (&thread_current()->sup_page_table, &spte->hash_elem);
}

/* Removes all entries with given mapid, for use with memory unmapping. */
void
munmap_spt(mapid_t mapid) {
  // (1) Get the list of spt_entry's from the Mapid Table
  //    (a) Write back the pages for every entry
  //    (b) hash_delete the entry from the Supplemental Page Table

}

/* Create a new entry and add it to the supplemental page table.
   Returns TRUE if operation succeeds, FALSE otherwise. */
bool 
create_entry_spt(void *vaddr) {
  struct spt_entry *spte = malloc(sizeof(struct spt_entry));
  uint32_t *kpage = allocate_frame_ft(vaddr);                                               

  if (kpage != NULL) {
    // (1) Create the Supplemental Page Table entry
    spte->page_num = pg_no (vaddr);
    spte->frame_num = pg_no (kpage);
    spte->in_swap = false;
    spte->mapid = -1;

    // (2) Add the frame-to-page mapping to the Frame Table
    add_entry_ft (spte->frame_num, spte->page_num);
    // hash_insert returns a null pointer if no element equal to element previously existed in it
    return (hash_insert (&thread_current()->sup_page_table, &spte->hash_elem) == NULL); 
  } else {
    free(spte);
    return false;
  }
}

/* Returns the supplemental page table entry containing the given
   PAGE NUM, or a null pointer if no such entry exists. */
struct spt_entry *
get_entry_spt(const void *page_num) { 	
  struct spt_entry spte;
  struct hash_elem *e;

  spte.page_num = page_num;
  e = hash_find (&thread_current()->sup_page_table, &spte.hash_elem);
  return e != NULL ? hash_entry (e, struct spt_entry, hash_elem) : NULL;
}


/* Returns the supplemental page table entry containing the given
   virtual address, or a null pointer if no such entry exists. */
struct spt_entry *
get_entry_from_vaddr_spt(const void *vaddr) {   
  struct spt_entry spte;
  struct hash_elem *e;
  spte.page_num = pg_no (vaddr);
  struct thread * t = thread_current();
  struct hash * h = &t->sup_page_table;
  e = hash_find (h, &spte.hash_elem);
  return e != NULL ? hash_entry (e, struct spt_entry, hash_elem) : NULL;
}


/* Returns TRUE if the page at the given PAGE NUM is stored in
   the swap partition. */
bool 
page_is_in_swap_spt (const void *page_num) {
  return get_entry_spt (page_num)->in_swap;
}


/* Notifies the supplemental page table that the page at page num PAGE NUM
   is getting swapped out, that is, stored to the swap partition. The SECTOR_NUM
   marks the beginning of the swap slot. Returns TRUE if the page lookup was successful
  (if the page was in the supplemental page table) and FALSE otherwise. */
bool
swap_page_out_spt (const void *page_num, int sector_num) {
  struct spt_entry *spte = get_entry_spt (page_num);
  if (spte == NULL) {
    return false;
  } else {
    void * page = spte->page_num;             // This bitshifting to create a physical
    uint32_t vaddr = ((uint32_t) page) << 12; // address is a potential source of error
    pagedir_clear_page (thread_current()->pagedir, vaddr); // Clear the frame associated with this page
    spte->in_swap = true;
    spte->sector_num = sector_num;
    return true;
  }
}

/* Notifies the supplemental page table that the page at PAGE NUM
   is being swapped in, that is, stored to memory from the swap partition. 
   Returns TRUE if the page lookup was successful (if the page was in the 
   supplemental page table) and FALSE otherwise. */
bool
swap_page_in_spt (const void *page_num) {
  struct spt_entry *spte = get_entry_spt (page_num);
  if (spte == NULL) {
    return false;
  } else {
    spte->in_swap = false;
    spte->sector_num = -1;
    return true;
  }
}


/* Returns a list of all of the sector numbers of the pages that are swapped out for
   this supplemental page table. */
struct list *
get_all_swapped_out_sector_nums_spt (void) {
  struct list *swapped_out_sectors;
  list_init (swapped_out_sectors);

  struct hash_iterator i;

  hash_first (&i, &thread_current()->sup_page_table);

  while (hash_next (&i))
  {
    struct spt_entry *spte = hash_entry (hash_cur (&i), struct spt_entry, hash_elem);
    if (spte->in_swap == true) {
      struct sector_item si;
      si.sector_num = spte->sector_num;
      list_push_back (swapped_out_sectors, &si.elem);
    }
  }

  return swapped_out_sectors;
}



/* The page fault handler calls this method once it has realized that 
   the target page is stored in the swap partition. The method allocates
   a new frame, then calls into the swap table to swap it into memory. */
bool
get_entry_from_swap_spt (const void *page_num) {
    struct spt_entry *spte = get_entry_spt (page_num);
    ASSERT (spte->in_swap);
    ASSERT (spte != NULL);

    if (allocate_new_frame(spte)) {
      void * frame = spte->frame_num;            // This bitshifting to create a physical
      uint32_t faddr = ((uint32_t) frame) << 12; // address is a potential source of error      
      return swap_frame_in_st(spte->sector_num, faddr);                                          
    }
}


/* Allocates a new frame for the given supplemental page table entry by
   calling into the frame table. Returns TRUE if successful, FALSE otherwise. */
static bool 
allocate_new_frame(struct spt_entry *spte) {  
  void * page = spte->page_num;             // This bitshifting to create a virtual
  uint32_t vaddr = ((uint32_t) page) << 12; // address is a potential source of error
  uint32_t *kpage = allocate_frame_ft(vaddr);                                               

  if (kpage != NULL) {
    spte->frame_num = pg_no (kpage);
    return true;
  } else {
    return false;
  }
}









