// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  int ref_counts[PHYSTOP >> PGSHIFT];
} kmem;


int
kref_read(uint64 pa)
{
  int ref;
  acquire(&kmem.lock);
  ref = kmem.ref_counts[pa >> PGSHIFT];
  release(&kmem.lock);
  return ref;
}

void
kref_get(uint64 pa)
{
  acquire(&kmem.lock);
  kmem.ref_counts[pa >> PGSHIFT] += 1;
  release(&kmem.lock);
}


void
kinit()
{
  initlock(&kmem.lock, "kmem");
  memset(kmem.ref_counts, 0, sizeof(kmem.ref_counts));
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

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  r = (struct run*)pa;

  // Fill with junk to catch dangling refs.
  // memset(pa, 1, PGSIZE);

  acquire(&kmem.lock);
  if(kmem.ref_counts[(uint64)pa >> PGSHIFT] > 1){
    kmem.ref_counts[(uint64)pa >> PGSHIFT] -= 1;
    release(&kmem.lock);
    return;
  }
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
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
  if(r){
    kmem.freelist = r->next;
    kmem.ref_counts[(uint64)r >> PGSHIFT] = 1;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
