// Trying to create the eviction set based on my understanding
// This is NOT giving entirely correct result, but this implementatino is NEAREST
// as to creation of eviction set from the userspace.
// THIS IMPLEMENTATION IS STILL ACTIVE

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

void maccess(void* p)
{
  asm volatile ("mov (%0), %%rax\n"
    :
    : "c" (p)
    : "rax");
}

uint64_t rdtsc() {
  uint64_t a, d;
  asm volatile ("mfence");
  asm volatile ("rdtsc" : "=a" (a), "=d" (d));
  a = (d<<32) | a;
  asm volatile ("mfence");
  return a;
}

void flush(void* p) {
    asm volatile ("clflush 0(%0)\n"
      :
      : "c" (p)
      : "rax");
}

void populate_rand_addr_buff(datatype *buff_1,volatile uint64_t** eviction_random,unsigned int num_pages_1);
//void eviction_set(volatile uint64_t** eviction_random,unsigned int num_pages);
void evictSetDeterminationFunc(volatile uint64_t** eviction_random,unsigned int num_pages);

int main(int argc,char *argv[]){

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

  unsigned int num_pages_1 = buff_size_1/KB(4);
  printf("Number of pages: %u\n",num_pages_1);

  /*unsigned int num_offsets = buff_size_1/64;
  printf("Number of offsets: %u\n",num_offsets);*/

  eviction_random = (volatile uint64_t **) malloc((num_pages_1+1)*sizeof(uint64_t *));//num_pages_1
  if(!eviction_random){
    printf("Error in allocating memory eviction buffer\n");
    return EXIT_FAILURE;
  }
 
  eviction_random[0] = (uint64_t *)(buff_2+2*1024);
  printf("Start populating the eviction buffer addresses\n");
  populate_rand_addr_buff(buff_1,eviction_random,num_pages_1);
  printf("End populating\n");
  
  eviction_set_1(eviction_random,num_pages_1);
  
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

  for(int i =1;i<=num_pages;i++){
    //unsigned int pageind = selected_offsets[i];
    eviction_random[i] = (uint64_t *)(buff + selected_pages[i]*1024);

     /*if(i<100)
 	printf("%d) \t eviction: %p \t deref eviction: %d \t buff: %p \t deref buff:%d\n",
	   i,eviction_random[i],*eviction_random[i],buff + selected_pages[i]*1024,*(buff + selected_pages[i]*1024));*/
  }

}

#define EVICT_SET_MISS_RATIO 0.99
#define NUM_RNDS 5000
#define THRESHOLD 400
//This is the eviction set creation after discussing with daimeng
void evictSetDeterminationFunc(volatile uint64_t** eviction_random,unsigned int num_pages);

  size_t time,delta_1,delta_2;

  fprintf(stderr,"Starting the creation of the eviction buffer\n");
  fprintf(stderr,"Flushing the target address\n"); 

  flush((void *)eviction_random[0]);
  
  fprintf(stderr,"Non-cached access time\n");
  time = rdtsc();
  maccess((void *)eviction_random[0]);
  delta_1 = rdtsc() - time;
  
  printf("delta_0: %5zu\n", delta_1);

  fprintf(stderr,"Accessing the target address again\n");  
  time = rdtsc();
  maccess((void *)eviction_random[0]);
  delta_1 = rdtsc() - time;
  
  printf("delta_1: %5zu\n", delta_1);

  int evEndPtr=1;
  float mr;
  int testCtr=0;
  while(evEndPtr<num_pages){
    
     if(!(evEndPtr%5))
      fprintf(stderr,"currernt ctr : %d\t",evEndPtr);

     mr=0;
     for(int i =0 ; i< NUM_RNDS;i++){
 	  
	  for(int i =0 ; i< evEndPtr; i++)  
	   flush((void *)eviction_random[i]);
	  
          //Bringing inside the cache
	  time = rdtsc();
	  maccess((void *)eviction_random[0]);
          delta_1 = rdtsc() - time;
  	
	  if(!testCtr)
	    fprintf(stderr,"test access (non-cached): %5zu\n",delta_1);
	   
          for(int i=1; i<evEndPtr; i++)
	   maccess((void *)eviction_random[i]);  

	  time = rdtsc();
  	  maccess((void *)eviction_random[0]);
	  delta_1 = rdtsc() - time;

	  if(!testCtr){
	   fprintf(stderr,"test access (cached): %5zu\n",delta_1);
            testCtr++;
	  }
	  
	  if(delta_1>THRESHOLD)
	   mr += 1.0/NUM_RNDS;
     }
     
     if(!(evEndPtr%5))
      fprintf(stderr,"mr : %f\n",mr);

     if(mr>EVICT_SET_MISS_RATIO)
      break;
     else
      evEndPtr++;
  } 

  fprintf(stderr,"mr: %f \t Num of pages to reach eviction: %d\n\n",mr,evEndPtr);

   for(int i =0 ; i< evEndPtr; i++)  
	   flush((void *)eviction_random[i]);

   time = rdtsc();
   maccess((void *)eviction_random[0]);
   delta_1 = rdtsc() - time;
  	
   fprintf(stderr,"Acess after flush: %5zu\n",delta_1);

   time = rdtsc();
   maccess((void *)eviction_random[0]);
   delta_1 = rdtsc() - time;
  	
   fprintf(stderr,"Acess again: %5zu\n",delta_1);
	   
   for(int i=1; i<evEndPtr; i++)
     maccess((void *)eviction_random[i]);  

   time = rdtsc();
   maccess((void *)eviction_random[0]);
   delta_1 = rdtsc() - time;

   fprintf(stderr,"Accessing after accessing calculated eviction set: %5zu\n",delta_1);

   fprintf(stderr,"Creating the final eviction set\n");

   volatile uint64_t** tempEvictBuff = (volatile uint64_t **) malloc(evEndPtr*sizeof(uint64_t *));
   volatile uint64_t** finalEvictBuff = (volatile uint64_t **) malloc(evEndPtr*sizeof(uint64_t *));
   int *idxBuff = (int *)malloc(evEndPtr*sizeof(int));

   memcpy(tempEvictBuff,eviction_random+1,evEndPtr*sizeof(uint64_t *));
   memset(finalEvictBuff,NULL,evEndPtr*sizeof(uint64_t *));
   memset(idxBuff,0,evEndPtr*sizeof(uint64_t *)); // this is just to test the indexes of the selected page addresses
   
   for(int rnds = 0;rnds<3;rnds++){

 	  int currPos = 1;
   	  int detectedPages = 0;

	  while(currPos<evEndPtr){
	   
	   mr=0;

	   for(int j=0;j<NUM_RNDS;j++){
		
	    flush((void *)eviction_random[0]);
           
	    for(int k=currPos;k<evEndPtr;k++)
	     flush((void *)tempEvictBuff[k]);
	    
	    maccess((void *)eviction_random[0]);
	    asm volatile ("mfence");

            for(int k=currPos;k<evEndPtr;k++)
	      maccess((void *)tempEvictBuff[k]);  

	    time = rdtsc();
  	    maccess((void *)eviction_random[0]);
	    delta_1 = rdtsc() - time;

	    if(delta_1>THRESHOLD)
	     mr += 1.0/NUM_RNDS;

	  }

	  if(mr<0.01){
	    finalEvictBuff[detectedPages] = tempEvictBuff[currPos-1];
	    idxBuff[detectedPages] = currPos-1;
            detectedPages++;
	  }
	
	  currPos++;
       }

       fprintf(stderr,"detected pages: %d\t",detectedPages);

       flush((void *)eviction_random[0]);
	
       for(int k=0;k<detectedPages;k++)
         flush((void *)finalEvictBuff[k]);

       maccess((void *)eviction_random[0]);
       asm volatile ("mfence");

       for(int k=0;k<detectedPages;k++)
	  maccess((void *)finalEvictBuff[k]);
 	
       time = rdtsc();
       maccess((void *)eviction_random[0]);
       delta_1 = rdtsc() - time;

       fprintf(stderr,"time: %5zu\n",delta_1);
      
   }

}



/*void eviction_set(volatile uint64_t** eviction_random,unsigned int num_pages){*/

/*  size_t time,delta_1,delta_2;*/
/* */
/*  time = rdtsc();*/
/*  maccess((void *)eviction_random[0]);*/
/*  delta_1 = rdtsc() - time;*/
/*  */
/*  printf("delta_0: %5zu\t", delta_1);*/
/*  */
/*  time = rdtsc();*/
/*  maccess((void *)eviction_random[0]);*/
/*  delta_1 = rdtsc() - time;*/
/*  */
/*  printf("delta_1: %5zu\n", delta_1);*/

/*  int ctr=1;*/
/*  int page_ct_max=0;*/
/*  while(ctr<num_pages){*/

/*    //asm volatile ("CPUID");*/
/*    */
/*    maccess((void *)eviction_random[0]);*/
/*    time = rdtsc();*/
/*    maccess((void *)eviction_random[0]);*/
/*    delta_1 = rdtsc() - time;*/

/*    //for(int j=0;j<50000;j++){ */
/*    //if(ctr%2){*/
/*      for(int k=1;k<ctr;k++){*/
/*      maccess((void *)eviction_random[k]);*/
/*     /* *eviction_random[k+1];*/
/*      *eviction_random[k+2];*/
/*     //}*/
/*     //else{*/
/*      *eviction_random[k+2];*/
/*      *eviction_random[k+1];*/
/*      *eviction_random[k];*/*/
/*     //}*/
/*     }	*/
/*    //}*/

/*    time = rdtsc();*/
/*    maccess((void *)eviction_random[0]);*/
/*    delta_1 = rdtsc() - time;*/
/*  */
/*    if(delta_1>400){*/
/*	//printf("ctr: %d \t delta_1: %5zu\n",ctr, delta_1);*/
/*	page_ct_max++;*/
/*    }	*/

/*    if(page_ct_max==100)*/
/*	break;*/

/*    ctr++;*/
/*  }*/

/*  printf("Final ctr val: %d\n",ctr);*/

/*  #define numIter 50000*/
/*  int ctr_2 = 0;*/
/*  volatile uint64_t** selected_pages = (volatile uint64_t **) malloc((num_pages)*sizeof(uint64_t *));*/
/*  memset(selected_pages,NULL,num_pages*sizeof(uint64_t *));*/

/*  for(int i=1;i<ctr;i++){*/

/*    maccess((void *)eviction_random[0]);*/
/*    time = rdtsc();*/
/*    maccess((void *)eviction_random[0]);*/
/*    delta_1 = rdtsc() - time;*/

/*    printf("page #: %d \t delta_1:%5zu\t",i, delta_1);*/
/*    */
/*   // for(int j=0;j<numIter;j++){*/
/*      for(int k=1;k<ctr;k++){*/
/*	if(i!=k)*/
/*	  maccess((void *)eviction_random[k]);*/
/*      }*/
/*  //  }*/
/*    time = rdtsc();*/
/*    maccess((void *)eviction_random[0]);*/
/*    delta_2 = rdtsc() - time;*/
/*          	*/
/*    printf("delta_2: %5zu\n",delta_2);*/

/*    if(delta_2<400)*/
/*	ctr_2++;*/

/*  }*/
/* */
/*  printf("ctr_2: %d\n",ctr_2);*/
/*  free(selected_pages); */
/*}*/
