// Trying to create the eviction set based on my understanding
// This is NOT giving entirely correct result, but this implementatino is NEAREST
// as to creation of eviction set from the userspace.
// THIS IMPLEMENTATION IS STILL ACTIVE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sched.h>
#include <sys/time.h>
#include <time.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#define MB(X) ((X)*1024*1024)
#define KB(X) ((X)*1024)
typedef int datatype;
#define force_inline __attribute__((always_inline))

static inline force_inline void maccess(void* p)
{
  asm volatile ("mov (%0), %%rax\n"
    :
    : "r" (p)
    : "rax");
}

//static int load_data = 0;
//#define maccess(ADDR) { load_data = load_data ^ *((datatype*)ADDR); }

static inline force_inline uint64_t rdtsc() {
  uint64_t a, d;
  asm volatile ("mfence");
  asm volatile ("RDTSCP" : "=a" (a), "=d" (d));
    a = (d<<32) | a;
  asm volatile ("mfence");
  return a;
}

/*inline __attribute__((always_inline)) uint64_t rdtsc_bgn() {*/
/*  uint64_t a, d;*/
/*  asm volatile ("mfence\n\t"*/
/*    "RDTSCP\n\t"*/
/*    "mov %%rdx, %0\n\t"*/
/*    "mov %%rax, %1\n\t"*/
/*    "xor %%rax, %%rax\n\t"*/
/*    "CPUID\n\t"*/
/*    : "=r" (d), "=r" (a)*/
/*    :*/
/*    : "%rax", "%rbx", "%rcx", "%rdx");*/
/*  uint64_t r = d;*/
/*  r = (r<<32) | (uint64_t)a;*/
/*  return r;*/
/*}*/

/*inline __attribute__((always_inline)) uint64_t rdtsc_end() {*/
/*  uint64_t a, d;*/
/*  asm volatile(*/
/*    "xor %%rax, %%rax\n\t"*/
/*    "CPUID\n\t"*/
/*    "RDTSCP\n\t"*/
/*    "mov %%rdx, %0\n\t"*/
/*    "mov %%rax, %1\n\t"*/
/*    "mfence\n\t"*/
/*    : "=r" (d), "=r" (a)*/
/*    :*/
/*    : "%rax", "%rbx", "%rcx", "%rdx");*/
/*   uint64_t r = d;*/
/*   r = (r<<32) | (uint64_t)a;*/
/*   return r;*/
/*}*/



static inline force_inline void flush(void* p) {
    asm volatile ("clflush 0(%0)\n"
      :
      : "c" (p)
      : "rax");
}

void set_affinity(int coreid) {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(coreid, &mask);
  if(sched_setaffinity( 0, sizeof(mask), &mask ) == -1 ) {
    perror("WARNING: Could not set CPU Affinity, continuing...\n");
  }
}

void populate_rand_addr_buff(datatype *buff_1,volatile uint64_t** eviction_random,unsigned int num_pages_1);
//void eviction_set(volatile uint64_t** eviction_random,unsigned int num_pages);
void evictSetDeterminationFunc(volatile uint64_t** eviction_random,volatile uint64_t* target_addr,unsigned int num_pages);

int main(int argc,char *argv[]){

  set_affinity(1);
  datatype *buff_1,*buff_2;
  size_t buff_size_1,buff_size_2;
  
  buff_size_1 = MB(64);  //allocating buffer size 

  int err = posix_memalign((void **)&buff_1,4096,buff_size_1); //allocating page aligned main buffer

  //buff_1 = (datatype *)malloc(buff_size_1);
  if(err!=0){
	printf("Err: %d\n",err);
	return EXIT_FAILURE;
  }

  buff_size_2 = MB(8);  //allocating buffer size 

  err = posix_memalign((void **)&buff_2,4096,buff_size_2); //allocating page aligned main buffer
  if(err!=0){
	printf("Err: %d\n",err);
	return EXIT_FAILURE;
  }

  int num_el_1 = buff_size_1/sizeof(datatype);
  int num_el_2 = buff_size_2/sizeof(datatype);

  for(int i=0;i<num_el_1;i++)
      buff_1[i] = i;

  for(int i=0;i<num_el_2;i++)
      buff_2[i] = num_el_2 - i;
  
  volatile uint64_t** eviction_random;
  volatile uint64_t* target_addr;

  unsigned int num_pages_1 = buff_size_1/KB(4);
  printf("Number of pages: %u\n",num_pages_1);

  /*unsigned int num_offsets = buff_size_1/64;
  printf("Number of offsets: %u\n",num_offsets);*/

  eviction_random = (volatile uint64_t **) malloc((num_pages_1)*sizeof(uint64_t *));//num_pages_1
  if(!eviction_random){
    printf("Error in allocating memory eviction buffer\n");
    return EXIT_FAILURE;
  }
 
  target_addr = (uint64_t *)(buff_2+2*1024);
  printf("Start populating the eviction buffer addresses\n");
  populate_rand_addr_buff(buff_1,eviction_random,num_pages_1);
  printf("End populating\n");
  
  evictSetDeterminationFunc(eviction_random,target_addr,num_pages_1);
  
  free(buff_1);
  free(buff_2);
  free(eviction_random);
}


void populate_rand_addr_buff(datatype *buff,volatile uint64_t** eviction_random,unsigned int num_pages){

  unsigned int selected_pages[num_pages];

  memset(selected_pages,0,num_pages*sizeof(unsigned int));

  int ctr =0;
  bool matched_pages = false;
  
  srand(time(NULL));
  for(unsigned int i=0;i<num_pages;i++){
      
    unsigned int pageNum = rand()%(num_pages + 1);
       
    for(unsigned int j=0;j<ctr;j++){

      if(selected_pages[j]==pageNum){
	matched_pages = true;
        break;
      }
    }
    
    if(matched_pages){
        i--;
        matched_pages = false;
        continue;
    }
    else{
      // printf("%u\n",pageNum);
       selected_pages[i] = pageNum;
       ctr++;
    }

  }

  for(int i =0;i<num_pages;i++){
    //unsigned int pageind = selected_offsets[i];
    eviction_random[i] = (uint64_t *)(buff + selected_pages[i]*1024);

     /*if(i<100)
 	printf("%d) \t eviction: %p \t deref eviction: %d \t buff: %p \t deref buff:%d\n",
	   i,eviction_random[i],*eviction_random[i],buff + selected_pages[i]*1024,*(buff + selected_pages[i]*1024));*/
  }

}

#define EVICT_SET_RATIO 0.97

#define NUM_RNDS 10000
#define NON_CACHED_THRESHOLD 400
#define CACHED_THESHOLD 200
#define EVICTSET_NUM_PAGES 100
//This is the eviction set creation after discussing with daimeng
void evictSetDeterminationFunc(volatile uint64_t** eviction_random,volatile uint64_t* target_addr,unsigned int num_pages){

  uint64_t time,delta_1,delta_2;

  fprintf(stderr,"Starting the creation of the eviction buffer\n");
  fprintf(stderr,"Flushing the target address\n"); 
  flush((void *)target_addr);
  
  fprintf(stderr,"Non-cached access time\n");
  time = rdtsc();
  maccess((void *)target_addr);
  delta_1 = rdtsc() - time;
  
  printf("delta_0: %"PRId64"\n", delta_1);

  fprintf(stderr,"Accessing the target address again\n");  
  time = rdtsc();
  maccess((void *)target_addr);
  delta_1 = rdtsc() - time;
  
  printf("delta_1: %"PRId64"\n", delta_1);

  int evEndPtr=1;
  float mr;
  int testCtr=0;
  while(evEndPtr<num_pages){
    
     if(!(evEndPtr%5))
      fprintf(stderr,"currernt ctr : %d\t",evEndPtr);

     mr=0;
     for(int i =0 ; i< NUM_RNDS;i++){
 	  
	  flush((void *)target_addr);
	  for(int j =0 ; j< evEndPtr; j++)  
	   flush((void *)eviction_random[j]);
	  
   	  maccess((void *)target_addr);
          asm volatile ("mfence");

	  for(int j=0; j<evEndPtr; j++){
	   //for(int k=0; k<5; k++)
	   maccess((void *)eviction_random[j]);  
	  }
	  asm volatile ("mfence");

	  time = rdtsc();
  	  maccess((void *)target_addr);
	  delta_1 = rdtsc() - time;

	  if(delta_1>NON_CACHED_THRESHOLD)
	   mr += 1.0/NUM_RNDS;
     }
     
     if(!(evEndPtr%5))
      fprintf(stderr,"mr : %f\n",mr);

     if(mr>EVICT_SET_RATIO)
      break;
     else
      evEndPtr++;
  } 

  fprintf(stderr,"mr: %f \t Num of page addresses to reach eviction: %d\n\n",mr,evEndPtr);

   flush((void *)target_addr);
   for(int i =0 ; i< evEndPtr; i++)  
     flush((void *)eviction_random[i]);

   time = rdtsc();
   maccess((void *)target_addr);
   delta_1 = rdtsc() - time;
  	
   fprintf(stderr,"Acess after flush: %"PRId64"\n",delta_1);

   time = rdtsc();
   maccess((void *)target_addr);
   delta_1 = rdtsc() - time;
  	
   fprintf(stderr,"Acess again: %"PRId64"\n",delta_1);
	   
   for(int i=0; i<evEndPtr; i++)
     maccess((void *)eviction_random[i]);  

   time = rdtsc();
   maccess((void *)target_addr);
   delta_1 = rdtsc() - time;

   fprintf(stderr,"Accessing after accessing calculated eviction set: %"PRId64"\n",delta_1);

   fprintf(stderr,"Creating the final eviction set\n");

   //volatile uint64_t** tempEvictBuff = (volatile uint64_t **) malloc(evEndPtr*sizeof(uint64_t *));
   volatile uint64_t** finalEvictBuff = (volatile uint64_t **) malloc(EVICTSET_NUM_PAGES*sizeof(uint64_t *));
   int idxBuff[EVICTSET_NUM_PAGES];// = (int *)malloc(evEndPtr*sizeof(int));

   //memcpy(tempEvictBuff,eviction_random+1,evEndPtr*sizeof(uint64_t *));
   memset(finalEvictBuff,NULL,EVICTSET_NUM_PAGES*sizeof(uint64_t *));
   memset(idxBuff,0,EVICTSET_NUM_PAGES*sizeof(int)); // this is just to test the indexes of the selected page addresses
   
   for(int rnds = 0;rnds<10;rnds++){

 	  int currPos =1;
   	  int detectedPages = 0;

	  while(currPos<evEndPtr){
	   
	   mr=0;

	   for(int j=0;j<NUM_RNDS;j++){
		
	    flush((void *)target_addr);
           
	    for(int k=currPos;k<evEndPtr;k++)
	     flush((void *)eviction_random[k]);
	   
/*	    if(detectedPages)*/
/*	      for(int k=0;k<detectedPages;k++)*/
/*	       flush((void *)finalEvictBuff[k]);*/

	    maccess((void *)target_addr);
	    asm volatile ("mfence");
	
/*	    if(detectedPages)*/
/*	      for(int k=0;k<detectedPages;k++)*/
/*		//for(int i=0;i<5;i++)*/
/*	          maccess((void *)finalEvictBuff[k]);*/

             for(int k=currPos;k<evEndPtr;k++)
  		//for(int i=0;i<5;i++)
	         maccess((void *)eviction_random[k]);  
	    
	    time = rdtsc();
  	    maccess((void *)target_addr);
	    delta_1 = rdtsc() - time;

	    if(delta_1<CACHED_THESHOLD)
	     mr += 1.0/NUM_RNDS;

	  }

/*          if(rnds==0){*/
/*	   if(mr<=EVICT_SET_MISS_RATIO)*/
/*             printf("currPos: %d \t mr: %f\n",currPos,mr);*/
/*	   else*/
/*             printf("currPos: %d \t mr: %f\t ==>>",currPos,mr);*/
/*	  }*/

	  if(mr>EVICT_SET_RATIO){
//	   if(rnds==0)
//	    printf("TAKEN\n");
	   finalEvictBuff[detectedPages] = eviction_random[currPos-1];
	   idxBuff[detectedPages] = currPos-1;
           detectedPages++;
	  }
	
	  if(detectedPages==EVICTSET_NUM_PAGES)
             break;

	  currPos++;
       }

       fprintf(stderr,"detected pages: %d\t",detectedPages);

       flush((void *)target_addr);
	
       for(int k=0;k<detectedPages;k++)
         flush((void *)finalEvictBuff[k]);

       maccess((void *)target_addr);
       asm volatile ("mfence");

       for(int k=0;k<detectedPages;k++)
           for(int j=0;j<10;j++)
	      maccess((void *)finalEvictBuff[k]);
 	
       time = rdtsc();
       maccess((void *)target_addr);
       delta_1 = rdtsc() - time;

       fprintf(stderr,"time: %"PRId64"\n",delta_1);

/*       if(rnds==0)*/
/*	  for(int i=0;i<detectedPages;i++)*/
/*	   fprintf(stderr,"page index: %d \t address: %p\n",idxBuff[i],finalEvictBuff[i]);*/
             
   }

}


