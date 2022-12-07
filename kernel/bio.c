// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf heads[NBUCKET];
  struct spinlock locks[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;
  int i, bucket_id;

  initlock(&bcache.lock, "bcache");

  for (i = 0; i < NBUCKET; i++) {
    bcache.heads[i].prev = &bcache.heads[i];
    bcache.heads[i].next = &bcache.heads[i];
    initlock(&bcache.locks[i], "bcache");
  }

  i = 0;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    bucket_id = i % NBUCKET;
    b->next = bcache.heads[bucket_id].next;
    b->prev = &bcache.heads[bucket_id];
    initsleeplock(&b->lock, "buffer");
    bcache.heads[bucket_id].next->prev = b;
    bcache.heads[bucket_id].next = b;
    i++;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int bucket_id, old_bucket_id;

  bucket_id = blockno % NBUCKET;

  acquire(&bcache.locks[bucket_id]);

  // Is the block already cached?
  for(b = bcache.heads[bucket_id].next; b != &bcache.heads[bucket_id]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.locks[bucket_id]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // acquire(&bcache.lock);
  acquire(&bcache.lock);
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    if(b->refcnt == 0) {
      old_bucket_id = b->blockno % NBUCKET;
      if(old_bucket_id != bucket_id) {
        acquire(&bcache.locks[old_bucket_id]);
        b->next->prev = b->prev;
        b->prev->next = b->next;
        b->next = bcache.heads[bucket_id].next;
        b->prev = &bcache.heads[bucket_id];
        bcache.heads[bucket_id].next->prev = b;
        bcache.heads[bucket_id].next = b;
      }
      release(&bcache.lock);
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.locks[bucket_id]);
      if(old_bucket_id != bucket_id)
        release(&bcache.locks[old_bucket_id]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  b->refcnt--;
  releasesleep(&b->lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache.locks[b->blockno%NBUCKET]);
  b->refcnt++;
  release(&bcache.locks[b->blockno%NBUCKET]);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.locks[b->blockno%NBUCKET]);
  b->refcnt--;
  release(&bcache.locks[b->blockno%NBUCKET]);
}


