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

#define NBUCKET 13

#define idx(b) (b % NBUCKET)
extern uint ticks; 
struct spinlock lock[NBUCKET];   

struct buf table[NBUCKET];

// static void 
// insert(struct buf* e, struct buf* head)
// {
//   //todo check
//   e->next = head->next; 
//   head->next->prev = e; 

//   e->prev = head;
//   head->next = e;  
// }

static 
void put(int blockno, struct buf* b)
{
  int key = blockno; 
  int i = key % NBUCKET;

  //insert into new block
  b->next = table[i].next;
  table[i].next->prev = b;
  b->prev = &table[i];
  table[i].next = b; 
}


static void
rm(int blockno, struct buf *e)
{
    e->prev = e->next;
    e->next->prev = e->prev;

    e->next = (struct buf*)0;
    e->prev = (struct buf*)0;
}

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
} bcache;

void
binit(void)
{
  struct buf* b; 
  initlock(&bcache.lock, "bcache");
  

  // Initialize buckets
  for(int i = 0; i < NBUCKET; i++) {
      initlock(&lock[i], "bucket");
  
      table[i].next = &table[i];
      table[i].prev = &table[i];
  }
  int bno= 0;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");
    b->blockno = bno;
    put(b->blockno, b);
    bno += 1;
  }
}
// void p(int blockno) {
//   int i = idx(blockno);
//   printf("bno %d, bucket %d", blockno, i);
//   for(struct buf* b = table[i].next; b != &table[i]; b = b->next){
//     printf("(bno %d,%d)->", b->blockno, b->dev);
//   }
//   printf("\n");
// }

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int i = idx(blockno);
  acquire(&lock[i]);
  // acquire(&bcache.lock);
  
  // Is the block already cached?
  for(b = table[i].next; b != &table[i]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
        // found in hash table
        // increase ref count
        b->refcnt++;
        release(&lock[i]);
        // release(&bcache.lock);
        // acquire sleep lock
        acquiresleep(&b->lock);
        return b;
    }
  }
  // try to find a lru in current bucket 
  for(b = table[i].prev; b != &table[i]; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&lock[(i)]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  
  //try to find in other buckets, how to init the buckets?
  for(int i = 0; i < NBUCKET; i++) {
    if (i == idx(blockno)) {
      continue;
    }
    acquire(&lock[i]);
    for(b = table[i].prev; b != &table[i]; b = b->prev){
      if(b->refcnt == 0) {
        rm(b->blockno, b);// remove in current bucket
        release(&lock[i]);

        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        put(b->blockno, b); //put into new bucket

        release(&lock[idx(blockno)]);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&lock[i]);
  }
  release(&lock[idx(blockno)]); 
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

  releasesleep(&b->lock);

  //With this change brelse doesn't need to acquire the bcache lock 
  //why , use the bucket lock 
  acquire(&lock[idx(b->blockno)]);
  // acquire(&bcache.lock);
  int i = idx(b->blockno);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    // b->ts = ticks;
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = table[i].next;
    b->prev = &table[i];
    table[i].next->prev = b;
    table[i].next = b;
  }
  release(&lock[idx(b->blockno)]); 
  // release(&bcache.lock);
}

void
bpin(struct buf *b) {
  acquire(&lock[idx(b->blockno)]);
  // acquire(&bcache.lock);
  b->refcnt++;
  // release(&bcache.lock);
  release(&lock[idx(b->blockno)]);
}

void
bunpin(struct buf *b) {
  acquire(&lock[idx(b->blockno)]);
  // acquire(&bcache.lock);
  b->refcnt--;
  release(&lock[idx(b->blockno)]);
  // release(&bcache.lock);
}


