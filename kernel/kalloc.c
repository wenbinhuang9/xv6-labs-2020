// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

//A good way to do this is to keep, 
//for each physical page, a “reference count” of the number of user page tables that refer to that page.
//give the array a number of elements 
//equal to highest physical address of any page placed on the free list by kinit() in kalloc.c.
int pgcnt[(uint64)PHYSTOP/PGSIZE]; 

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.
struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}



// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree_dec(void *pa, int dec)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  
  decpgcnt((uint64)pa);
  
  int idx = ((uint64)pa)/PGSIZE;
  if (pgcnt[idx] > 0) {
    //kfree() should only place a page back on the free list if its reference count is zero
    return;
  }
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

void kfree(void *pa) {
  kfree_dec(pa, 1);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    //Set a page’s reference count to one when kalloc() allocates it.
    int idx = ((uint64)r)/PGSIZE; 
    pgcnt[idx] = 1;
  }
  release(&kmem.lock);

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
  }
  return (void*)r;
}


void incpgcnt(uint64 pa) {
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("incpgcnt"); 
  int idx = pa/PGSIZE; 

  acquire(&kmem.lock);
  pgcnt[idx] += 1;
  release(&kmem.lock);
}

void decpgcnt(uint64 pa) {
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("decpgcnt"); 
  int idx = pa/PGSIZE; 

  acquire(&kmem.lock);
  pgcnt[idx] -= 1;
  release(&kmem.lock);
}