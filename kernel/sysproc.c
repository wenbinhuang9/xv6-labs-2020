#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "proc.h"
#include "fcntl.h"
#include "fs.h"
#include "file.h"

uint64 sys_mmap(void ) {
  uint64 addr; 
  int length, prot, flags, offset, fd; 

  if(argaddr(0, &addr) < 0 ||  argint(1, &length) < 0 || argint(2, &prot) < 0 || 
    argint(3, &flags) < 0 || argint(4, &fd) < 0 || argint(5, &offset) < 0) {
    return -1; 
  }
  if (addr != 0) {
    printf("addr is not 0 ");
    return -1;
  }
  if (offset != 0 ) {
    printf("offset is not 0");
    return -1;
  }
  struct proc *p = myproc();
  struct file* f = p->ofile[fd]; 
  int pte_flag = PTE_U; 

  //check writable 
  if(prot & PROT_WRITE) {
    if(!(f->writable) && !(flags & MAP_PRIVATE)) {
      printf("not writable\n");
      return -1;
    }
    pte_flag |= PTE_W;
  }
  //check readable 
  if(prot & PROT_READ) {
    if(!(f->readable)) {
      printf("not readable\n");
      return -1; 
    }
    pte_flag |= PTE_R;
  }
  
  // allocat 
  struct vma* pvma;

  if((pvma = alloc_vma()) == 0) {
    return -1;
  }
  pvma->length = length; 
  pvma->offset = offset;
  pvma->flags = flags;
  pvma->prot = prot;
  pvma->file = f; 
  filedup(f);
  pvma->next = 0;
  pvma->pte_flag = pte_flag; 
  if (p->pvma == 0) {
    pvma->start = VMA_START; 
    pvma->end = VMA_START + length; 
    p->pvma = pvma; 
  }else {
    struct  vma *i = p->pvma;  
    while(i->next) i = i->next; // find the last vma  

    pvma ->start = PGROUNDUP(i->end);
    pvma ->end = pvma->start + length; 
    i->next = pvma;
  }

  release(&pvma->lock);
  printf("mmap done, addr: %p, len: %d\n", pvma->start, pvma->length);
  return pvma->start;
}


void
writeback(struct vma* v, uint64 addr, int n) {
  if ((v->flags & MAP_PRIVATE) ||  !(v->pte_flag & PTE_W)) {
    return; 
  }

  if( addr  % PGSIZE != 0) {
    panic("unmap not aligned");
  }

  struct file* f = v->file;

  begin_op();
  ilock(f->ip);
  int r = writei(f->ip, 1, addr, v->offset + addr - v->start, n);
  if (r == 0) {
    printf("write fails n1:%d\n", n);
  }
  iunlock(f->ip);
  end_op();

  printf("writeback done addr:%p, len:%d\n", addr, n);
}

extern struct vma vma[NVMA];
uint64 sys_munmap(void) {
  uint64 addr; 
  int length; 

  if(argaddr(0, &addr) < 0 || argint(1, &length) < 0) {
    printf("argint fails\n");
    return -1; 
  }
  if( addr % PGSIZE != 0 || length == 0) {
    printf("invalid args\n");
    return -1;
  }
  struct vma* pre = 0; 
  struct vma* v = myproc()->pvma; 
  //find the VMA for the address range and unmap the specified pages (hint: use uvmunmap). 
  while(v) {
    if(addr >= v->start && addr < v->end) {
      break;
    }else {
      pre = v; 
    }
  }
  if (v == 0) {
    printf("munmap no vma found\n");
    return -1;
  }
  if(addr != v->start && (addr + length) != v->end) {
    printf("munmap middle of vma\n"); 
    return -1;
  }

  return unmap(v, addr, length, pre);
}

uint64 unmap(struct vma* v, uint64 addr, int length, struct vma* pre) {
  struct proc* p = myproc();
  if (v->start == addr && v->length == length) {
    //release whole
    //If an unmapped page has been modified and the file is mapped MAP_SHARED,
    // write the page back to the file. Look at filewrite for inspiration.
    writeback(v, addr, length);
    uvmunmap(p->pagetable, addr, PGROUNDUP(length) / PGSIZE, 1);
    //If munmap removes all pages of a previous mmap, 
    //it should decrement the reference count of the corresponding struct file. 
    fileclose(v->file);
    
    if(pre == 0) {
      p->pvma = v->next;

    }else {
      pre->next = v->next;   
      v->next = 0; 
    }
    acquire(&v->lock);
    v->length = 0;
    release(&v->lock);
  }else if( v->start == addr && length < v->length) {
    //release head
    writeback(v, addr, length);
    uvmunmap(p->pagetable, addr, length/ PGSIZE, 1); 
    
    v->length -= length;
    // v->start -= length; 
    v->start += length; 
    v->offset += length;
  }else if(v->end == (addr + length) && length < v->length) {
    // release tail
    writeback(v, addr, length);
    uvmunmap(p->pagetable, addr, length/ PGSIZE, 1);
    v->length -= length;
    v->end -= length; 
  }else{
    panic("munmap");
  }

  printf("munmap done, addr:%p, len:%d\n", addr, length);
  return 0; 
}

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
