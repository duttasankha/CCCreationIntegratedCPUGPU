#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sched.h>
#include <inttypes.h>
#include "CACHEUTILS.h"

size_t array[5*1024];
#define ADDR_COUNT 40
#define PROBE_COUNT 9 
size_t start = 0;

volatile uint64_t* faddrs[ADDR_COUNT];
uint8_t eviction[64*1024*1024];
uint8_t* eviction_ptr;
int g_pagemap_fd = -1;

int get_cache_slice(uint64_t phys_addr, int bad_bit) {
 static const int h0[] = { 6, 10, 12, 14, 16, 17, 18, 20, 22, 24, 25, 26, 27, 28, 30, 32, 33, 35, 36 };
  static const int h1[] = { 7, 11, 13, 15, 17, 19, 20, 21, 22, 23, 24, 26, 28, 29, 31, 33, 34, 35, 37 };

  int count = sizeof(h0) / sizeof(h0[0]);
  int hash = 0;
  for (int i = 0; i < count; i++) {
    hash ^= (phys_addr >> h0[i]) & 1;
  }
  count = sizeof(h1) / sizeof(h1[0]);
  int hash1 = 0;
  for (int i = 0; i < count; i++) {
    hash1 ^= (phys_addr >> h1[i]) & 1;
  }
  return hash1 << 1 | hash;
}

int in_same_cache_set(uint64_t phys1, uint64_t phys2, int bad_bit) {
  // For Sandy Bridge, the bottom 17 bits determine the cache set
  // within the cache slice (or the location within a cache line).
  uint64_t mask = ((uint64_t) 1 << 17) - 1;
  static int ctr =1;

  /*int sl_1 = get_cache_slice(phys1, bad_bit);
  int sl_2 = get_cache_slice(phys2, bad_bit);
  
  if((phys1 & mask) == (phys2 & mask)&&(sl_1==sl_2)){
    printf("%d)\tphys_1: %"PRIx64"\t sl_1: %d \t phys_2: %"PRIx64"\t sl_2: %d\n",ctr,phys1,sl_1,phys2,sl_2);
    ctr++;
  }*/
  return ((phys1 & mask) == (phys2 & mask) &&
          get_cache_slice(phys1, bad_bit) == get_cache_slice(phys2, bad_bit));
}

void init_pagemap() {
  g_pagemap_fd = open("/proc/self/pagemap", O_RDONLY);
  assert(g_pagemap_fd >= 0);
}

uint64_t frame_number_from_pagemap(uint64_t value) {
  return value & ((1ULL << 54) - 1);
}

uint64_t get_physical_addr(uint64_t virtual_addr) {
  uint64_t value;
  off_t offset = (virtual_addr / 4096) * sizeof(value);
  int got = pread(g_pagemap_fd, &value, sizeof(value), offset);
  
  //printf("virtual_addr: %"PRIu64"\t offset: %"PRId64"\tvalue: %"PRIu64"\n",virtual_addr,offset,value);
  assert(got == 8);

  // Check the "page present" flag.
  assert(value & (1ULL << 63));

  uint64_t frame_num = frame_number_from_pagemap(value);

  //printf("virtual_addr: %"PRIx64"\t offset: %"PRId64"\tvalue: %"PRIx64"\tframe_num: %"PRId64"\t",virtual_addr,offset,value,frame_num);

  return (frame_num * 4096) | (virtual_addr & (4095));
}

int found = 1;

void pick(volatile uint64_t** addrs, int step)
{
  uint8_t* buf = (uint8_t*) addrs[0];
  uint64_t phys1 = get_physical_addr((uint64_t)buf);
  //printf("phys: %"PRIx64"\n",phys1);
  //printf("%zx -> %zx\n",(uint64_t) buf, phys1);
  //printf("\n\n========================================================\n\n");
  for (size_t i = 0; i < 64*1024*1024-4096; i += 4096) {
    uint64_t phys2 = get_physical_addr((uint64_t)(eviction_ptr + i));
    //printf("phys: %"PRIx64"\n",phys2);
    if (phys1 != phys2 && in_same_cache_set(phys1, phys2, -1)) {
      addrs[found] = (uint64_t*)(eviction_ptr+i);
      //printf("%zx -> %zx\n",(uint64_t) eviction_ptr+i, phys2);
      //*(addrs[found-1]) = addrs[found];
      found++;
    }
  }
  fflush(stdout);
}


size_t rev = 0;
size_t kcount = 0;
size_t kpause = -1ULL;
void flushandreload(void* addr)
{
size_t time,delta;
if (rev == 0)
{
  time = rdtsc();
  for (size_t i = 1; i < PROBE_COUNT; ++i)
  {
    *faddrs[i];
    *faddrs[i+1];
    *faddrs[i+2];
    *faddrs[i];
    *faddrs[i+1];
    *faddrs[i+2];
  }
  delta = rdtsc() - time;
  rev = 1;
}
else
{
  time = rdtsc();
  for (size_t i = PROBE_COUNT-1; i > 0; --i)
  {
    *faddrs[i+2];
    *faddrs[i+1];
    *faddrs[i];
    *faddrs[i+2];
    *faddrs[i+1];
    *faddrs[i];
  }
  delta = rdtsc() - time;
  rev = 0;
}
  if (delta > 230)
  {
    kcount++;
    if (kcount > 3)
    {
      printf("Cache Hit (%zu) after %10lu cycles, t=%10lu us\n", delta, kpause, (time-start)/2600);
    }
    kpause = 0;
  }
  else
  {
    kpause++;
    kcount = 0;
  }
}

int main(int argc, char** argv)
{
 memset(array,-1,5*1024*sizeof(size_t));
 maccess(array + 2*1024);
 faddrs[0] = (uint64_t*) (array + 2*1024);

 for (size_t i = 0; i < 64*1024*1024; ++i)
    eviction[i] = i;

 eviction_ptr = (uint8_t*)(((size_t)eviction & ~0xFFF) | ((size_t)faddrs[0] & 0xFFF));

 printf("faddrs[0]: %p \t eviction: %p\n",faddrs[0],eviction);
 printf("eviction_ptr: %p\n",eviction_ptr);
 init_pagemap();
 
 printf("eviction set\n");
 pick(faddrs,+1);

 printf("\n\nfound: %d\n\n",found);

 /*for(int i=0;i<found;i++){
   printf("%d ==> %p\n",i,faddrs[i]);
 }*/

  printf("\n\n\n START \n");

  size_t time,delta_1,delta_2;

  int count =0;

  while(count <32){
    
    //asm volatile ("CPUID");

    //maccess(array + 2*1024);
  
    //time = rdtsc();
    //maccess(array + 2*1024);
    //delta_1 = rdtsc() - time;
    time = rdtsc();
    *faddrs[0];
    delta_1 = rdtsc() - time;
    printf("delta_0: %5zu\t", delta_1);

    time = rdtsc();
    /*for (size_t i = 1; i < 16; ++i)
    {
      *faddrs[i];
      *faddrs[i+1];
      *faddrs[i+2];
      *faddrs[i];
      *faddrs[i+1];
      *faddrs[i+2];
    }*/
    *faddrs[0];
    delta_1 = rdtsc() - time;

    //time = rdtsc();
    //maccess(array + 2*1024);
    //time = rdtsc();
    for (size_t i = 1; i < (found-2)-1; i++)//(found-2)-1
    {
      *faddrs[i+2];
      *faddrs[i+1];
      *faddrs[i];
      *faddrs[i+2];
      *faddrs[i+1];
      *faddrs[i];
    }
    time = rdtsc();
    *faddrs[0];
    delta_2 = rdtsc() - time;

    printf("delta_1: %5zu delta_2: %5zu \n", delta_1,delta_2);
    
    flush(array + 2*1024);
    for (int i = 0; i < 3000; ++i)
      sched_yield();

    
    count++;
    
  }
/*  start = rdtsc();
  while(1)//for(int k=0;k<100;k++)
  {
    flushandreload(array+2*1024);
    for (int i = 0; i < 3000; ++i)
      sched_yield();
  }*/

 return 0;
}
