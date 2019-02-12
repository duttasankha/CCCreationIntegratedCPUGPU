#define _GNU_SOURCE
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sched.h>
#include <inttypes.h>
#include <stdlib.h>
//#include "CACHEUTILS.h"

#define MB(X) ((X)*1024*1024)
#define KB(X) ((X)*1024)
#define VA_PA_SIZE 8192
#define uniqueSameSlicePerCacheSet 100
#define force_inline __attribute__((always_inline))

typedef struct cacheSetList{
 
 uint64_t cacheSet;
 uint64_t VA[VA_PA_SIZE];
 uint64_t PA[VA_PA_SIZE];
 int sameSetIndex[VA_PA_SIZE];
 int addrInSetCtr;
 
 uint64_t **selectedPA;
 uint64_t **selectedVA;
 int 	  **selectedIndex;
 int 	  sameSliceCtr[uniqueSameSlicePerCacheSet];
 int 	  totalSliceCtr;

 struct cacheSetList *next;

}CSList;

static inline force_inline void maccess(void* p)
{
  asm volatile ("movq (%0), %%rax\n"
    :
    : "r" (p)
    : "rax");
}

//static int load_data = 0;
//#define maccess(ADDR) { load_data = load_data ^ *((int*)ADDR); }

static inline force_inline uint64_t rdtsc() {
  uint64_t a, d;
  asm volatile ("mfence");
  asm volatile ("RDTSCP" : "=a" (a), "=d" (d));
    a = (d<<32) | a;
  asm volatile ("mfence");
  return a;
}

static inline force_inline void flush(void* p) {
    asm volatile ("clflush 0(%0)\n"
      :
      : "c" (p)
      : "rax");
}

typedef int dataType;
int g_pagemap_fd = -1;

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

int get_cache_slice(uint64_t phys_addr, int bad_bit) {
  //static const int h0[] = { 6, 10, 12, 14, 16, 17, 18, 20, 22, 24, 25, 26, 27, 28, 30, 32, 33, 35, 36 };
  //static const int h1[] = { 7, 11, 13, 15, 17, 19, 20, 21, 22, 23, 24, 26, 28, 29, 31, 33, 34, 35, 37 };
  static const int h0[] = { 17, 18, 20, 22, 24, 25, 26, 27, 28, 30, 32, 33};
  static const int h1[] = { 18, 19, 21, 23, 25, 27, 29, 30, 31, 32, 34 };

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

void set_affinity(int coreid) {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(coreid, &mask);
  if(sched_setaffinity( 0, sizeof(mask), &mask ) == -1 ) {
    perror("WARNING: Could not set CPU Affinity, continuing...\n");
  }
}

#define buffInMBSize 24
void multiEvictSetDetFunc(CSList *);

int main(int argc,char *argv[]){

	set_affinity(0);
	dataType *buff;

	size_t buffSize = MB(buffInMBSize);

	int err = posix_memalign((void **)&buff,4096,buffSize); //allocating page aligned main buffer
	if(err!=0){
		printf("Err: %d\n",err);
		return EXIT_FAILURE;
	}

	int numEl = buffSize/sizeof(dataType);
	int numPages = buffSize/KB(4);

	printf("Number of elements: %d\n",numEl);
	printf("Number of pages: %d\n",numPages);
	printf("Buff start addr: %p\n",buff);

	for(int i=0;i<numEl;i++)
		buff[i] = i;

	init_pagemap();
  
	size_t pageOffset = (KB(4)/sizeof(dataType));

	CSList *head,*temp;//*list[2048],

	head = NULL;//list[0];
	int totalUniqueSet = 0;

	for(int i=0;i<numPages;i++){

     		uint64_t VA= (uint64_t)(buffHost+ numElsPerPage*i);
     		uint64_t PA = get_physical_addr(VA);
     		uint64_t CacheSet = (uint64_t)((PA>>6)&0x7ff);//(PA&((uint64_t) 1 << 17) - 1);//
		int sameSetIndex = numElsPerPage*i;

     		//printf("index: %d \t VA: %"PRIx64" \t PA: %"PRIx64"\t Cache Set: %"PRIx64"\n",i,VA,PA,CacheSet);

     		if(head==NULL){
       		  head = (CSList *)malloc(sizeof(CSList));
       		  //head = list[0];
      		  head->next = NULL;
       		  head->addrInSetCtr =1;
       		  head->VA[head->addrInSetCtr - 1] = VA;
       		  head->PA[head->addrInSetCtr - 1] = PA;
		  head->sameSetIndex[head->addrInSetCtr - 1] = sameSetIndex;
       		  head->cacheSet = CacheSet;
       		  totalUniqueSet++;
     		}
     		else{

       		  temp = head;
       		  while(temp){
	  	   if(temp->cacheSet==CacheSet){
	    		temp->addrInSetCtr+=1;
	    		temp->VA[temp->addrInSetCtr - 1] = VA;
	    		temp->PA[temp->addrInSetCtr - 1] = PA;
			temp->sameSetIndex[temp->addrInSetCtr - 1] = sameSetIndex;
  	    		break;
          	  }
	  	  else{
	      		temp=temp->next;
	      		continue;  
          	 }           
       		}

       		if(temp==NULL){
			temp = head;
			while(temp->next!=NULL)
				temp = temp->next;
	 
			CSList *temp2 = (CSList *)malloc(sizeof(CSList));
			temp2->next = NULL;
			temp2->addrInSetCtr =1;
			temp2->VA[temp2->addrInSetCtr-1] = VA;
			temp2->PA[temp2->addrInSetCtr-1] = PA;
			temp2->sameSetIndex[temp2->addrInSetCtr - 1] = sameSetIndex;
			temp2->cacheSet = CacheSet;
			temp->next = temp2;
			totalUniqueSet++;
               }

            }
    
        }
	
	temp = head;
	printf("///////////////////////////////////////////////////////////////////////////\n"); 
	int samectr=0;
	while(temp){
		printf("Detected set number: %d\n",samectr);
		multiEvictSetDetFunc(temp);
		samectr++;
		printf("///////////////////////////////////////////////////////////////////////////\n");
		temp = temp->next;
	}


	temp = head;
	
	while(temp){
		for(int i =0 i< temp->totalSliceCtr;i++){
		
			for(int j =0 ;j<temp->sameSliceCtr[i];j++){
			
				long long int dummyVal = 0;
				int tempInd = selectedIndex[i][j];
				*buff[tempInd];
				while(1){
					dummyVal+= buff[tempInd]; 
				}
			}
		}

		temp = temp->next;
	}
 
	free(buff);
}


void multiEvictSetDetFunc(CSList *temp){

  int breakCtr = temp->addrInSetCtr;
  
  //printf("breakCtr: %d\n\n",breakCtr);

  /////////////////////////////////////////////////////////////////////////////////

  temp->selectedPA = (uint64_t **)malloc(uniqueSameSlicePerCacheSet*sizeof(uint64_t *)); 
  //memset(temp->selectedPA,0,breakCtr*sizeof(uint64_t)); 
 
  temp->selectedVA = (uint64_t **)malloc(uniqueSameSlicePerCacheSet*sizeof(uint64_t *));   
  //memset(temp->selectedVA,0,breakCtr*sizeof(uint64_t)); 
 
  temp->selectedIndex = (int **)malloc(uniqueSameSlicePerCacheSet*sizeof(int *));   
  //memset(temp->selectedIndex,0,breakCtr*sizeof(int));  

  int **sameIndex = (int **)malloc(uniqueSameSlicePerCacheSet*sizeof(int *));   

  temp->totalSliceCtr = 0;

  uint64_t localPA[breakCtr],localVA[breakCtr];
  int localInd[breakCtr];

  memcpy(localPA,temp->PA,breakCtr*sizeof(uint64_t));
  memcpy(localVA,temp->VA,breakCtr*sizeof(uint64_t));
  memcpy(localInd,temp->sameSetIndex,breakCtr*sizeof(int));

  //temp->selectedNumAddr =0;
  memset(temp->sameSliceCtr,0,uniqueSameSlicePerCacheSet*sizeof(int));    

  int initTargetIndex;// = 0;
  int allAddrCtr = breakCtr;
  int count = 0;
  
  //for(int i =0;i<breakCtr;i++)
    //  printf("temp->PA: %"PRIx64"\tPA: %"PRIx64" \t VA: %"PRIx64"\n",temp->PA[i],localPA[i],localVA[i]);
  
  while(allAddrCtr>10){
     
    //printf("Count: %d\n");
    //add the unique target address selection code here
    //if(count!=0){
	for(int n=0;n<breakCtr;n++){
          if(localPA[n]!=0){  
	   initTargetIndex = n;
           break;
	  }
	}
    //}
   
  
   if(count!=0){

	for(int i=0;i<breakCtr;i++){

		int found = 0;
		for(int j = 0;j<count;j++){
			for(int k = 0;k<temp->sameSliceCtr[j];k++)
				//if(temp->selectedIndex[j][k] == i)
				if(sameIndex[j][k] == i)
		  			found++;	
		}
		
		if((found == 0)&&(i!=initTargetIndex))
			if(get_cache_slice(localPA[initTargetIndex],1)==get_cache_slice(localPA[i],1)) //&& (localPA[i] != 0)) )
        	  		temp->sameSliceCtr[count]++;	   
    	} 
   }
   else{
		for(int i=0;i<breakCtr;i++) // Look into the counter 
			if(i!=initTargetIndex)
				if(get_cache_slice(localPA[initTargetIndex],1)==get_cache_slice(localPA[i],1)) //&& (localPA[i] != 0)) )
					temp->sameSliceCtr[count]++;
   }

   
/*    printf("allAddrCtr(b): %d \t Count: %d \t sameSliceCtr: %d\tinitTargetIndex: %d\tlocalPA[initTargetIndex]: %"PRIx64"\t",*/
/*			allAddrCtr, count,temp->sameSliceCtr[count],initTargetIndex,localPA[initTargetIndex]);*/

    temp->selectedPA[count] = (uint64_t *)malloc(temp->sameSliceCtr[count]*sizeof(uint64_t)); 
    temp->selectedVA[count] = (uint64_t *)malloc(temp->sameSliceCtr[count]*sizeof(uint64_t));   
    temp->selectedIndex[count] = (int *)malloc(temp->sameSliceCtr[count]*sizeof(int));   
    sameIndex[count] = (int *)malloc(temp->sameSliceCtr[count]*sizeof(int));

    int p = 0;
    if(count!=0){

	for(int i=0;i<breakCtr;i++){

		int found = 0;
		for(int j = 0;j<count;j++){
			for(int k = 0;k<temp->sameSliceCtr[j];k++)
				//if(temp->selectedIndex[j][k] == i)
				if(sameIndex[j][k] == i)
		  			found++;	
		}
		
		if((found == 0)&&(i!=initTargetIndex)){
			if(get_cache_slice(localPA[initTargetIndex],1)==get_cache_slice(localPA[i],1)){//&& (localPA[i] != 0)) ){
	
				temp->selectedPA[count][p]    = localPA[i];
				temp->selectedVA[count][p]    = localVA[i];  
				temp->selectedIndex[count][p] = localInd[i];   
				sameIndex[count][p] = i;   
				localPA[i] = 0;
				localVA[i] = 0;
         
				p++;
			}
	       }  
    	} 

	
   }
   else{

	for(int i=0;i<breakCtr;i++){
		if(i!=initTargetIndex){
			if(get_cache_slice(localPA[initTargetIndex],1)==get_cache_slice(localPA[i],1)){//&& (localPA[i] != 0)) ){
	
				temp->selectedPA[count][p]    = localPA[i];
				temp->selectedVA[count][p]    = localVA[i];  
				temp->selectedIndex[count][p] = localInd[i];   
		  		sameIndex[count][p] = i;
				localPA[i] = 0;
				localVA[i] = 0;
         
	 	  		p++;
		}
      	   }
    	}

   }

    localPA[initTargetIndex] = 0;
    //allAddrCons++; 
    allAddrCtr=allAddrCtr - temp->sameSliceCtr[count];
    count++;
    //printf("p: %d \n",p);
    //printf("p: %d \t allAddrCtr(a): %d\n",p,allAddrCtr);

  }
 
  temp->totalSliceCtr = count;

/*	for(int i=0;i<uniqueSameSlicePerCacheSet;i++)	  */
/*		free(sameIndex[i]);*/

/*	free(sameIndex);*/
}


