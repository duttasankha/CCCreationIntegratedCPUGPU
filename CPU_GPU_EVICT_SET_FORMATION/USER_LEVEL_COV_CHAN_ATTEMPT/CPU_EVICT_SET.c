#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
//#include <CL/cl.h>
//#include <CL/cl_platform.h>
#include <sched.h>
#include <sys/time.h>
#include <time.h>
#include <inttypes.h>

#define MB(C) ((C)*1024*1024)
#define KB(C) ((C)*1024)
#define cacheline 64
#define totalCacheLine (8192*16)

#define RDTSC_DIRTY "%rax", "%rbx", "%rcx", "%rdx"

#define RDTSC_START(cycles)                                \
	do {                                                   \
		register unsigned cyc_high, cyc_low;               \
		asm volatile("CPUID\n\t"                           \
				"RDTSCP\n\t"                           \
				"mov %%edx, %0\n\t"                   \
				"mov %%eax, %1\n\t"                   \
				: "=r" (cyc_high), "=r" (cyc_low)     \
				:: RDTSC_DIRTY);                      \
		(cycles) = ((uint64_t)cyc_high << 32) | cyc_low;   \
	} while (0)

#define RDTSC_STOP(cycles)                                 \
	do {                                                   \
		register unsigned cyc_high, cyc_low;               \
		asm volatile("RDTSCP\n\t"                          \
				"mov %%edx, %0\n\t"                   \
				"mov %%eax, %1\n\t"                   \
				"CPUID\n\t"                           \
				: "=r" (cyc_high), "=r" (cyc_low)     \
				:: RDTSC_DIRTY);                      \
		(cycles) = ((uint64_t)cyc_high << 32) | cyc_low;   \
	} while(0)

typedef int datatype;
typedef unsigned long long ull;

void pageBasedEviction(datatype *buff,datatype *randBuff,size_t buff_size,size_t num_pages,size_t num_el,long int threshold,double *ret_val, unsigned long *);
//void pageBasedEvictionMultSets(datatype *buff,unsigned long **cacheSetList,double *ret_val,int numCacheSets, size_t buff_size,size_t num_pages,size_t num_el);

int main(int argc,char *argv[]){

	datatype *buff,*randBuff;
	//ull **addrOfBuff[totalCacheLine];
	unsigned long long cycle_1,cycle_2;
	long int cycle_diff_1,cycle_diff_2;

	size_t buff_size = MB(8);  //allocating buffer size 
	size_t num_pages = buff_size/getpagesize(); //getting number of pages

	int err = posix_memalign((void **)&buff,4096,buff_size); //allocating page aligned main buffer
	if(err!=0){
		printf("Err: %d\n",err);
		return EXIT_FAILURE;
	}

	err = posix_memalign((void **)&randBuff,4096,sizeof(datatype)); // allcoating a page aligned random address
	if(err!=0){
		printf("Err: %d\n",err);
		return EXIT_FAILURE;
	}

	size_t num_el = buff_size/sizeof(datatype); //Calculating the number of elements in the main buffer

	//printing details
	printf("Main buffer address: %p \t Random buffer address: %p \n size: %ld\t number of pages: %ld\t",buff,randBuff,buff_size,num_pages);
	printf("number of elements: %ld\n",num_el);

	//Populating the buffer
	asm volatile ("CPUID\n\t":::);
	srand(time(NULL));
	randBuff[0] = (int)rand();

	for(int i=0;i<num_el;i++)
		buff[i] = (int)rand();
	asm volatile ("CPUID\n\t":::);

#ifdef PAGE_EVICT
	double val = 1; //dummy var
	unsigned long pind[16]; //variable to hold the eviciton buff page indices
	
	/**Calculating the threshold**/
	//Flushing address from all cache levels
	asm volatile ("CPUID\n\t":::);
	int *p = randBuff;
	asm volatile ("clflush (%0)"::"r"(p));
	asm volatile ("CPUID\n\t":::);

	//Accessing data after cache flush; gathering data from RAM
        datatype setTest =0; //dummy variable
        RDTSC_START(cycle_1);
	setTest += randBuff[0];
	RDTSC_STOP(cycle_2);
	
	//Measuring the access time
	cycle_diff_1 = cycle_2 - cycle_1;
	printf("cycle_diff_1: %ld\n",cycle_diff_1);

	//Accessing data from cache
	RDTSC_START(cycle_1);
	setTest += randBuff[0];
	RDTSC_STOP(cycle_2);

	//Measuring the access time
	cycle_diff_2 = cycle_2 - cycle_1;

	printf("cycle_diff_2: %ld\n",cycle_diff_2);

	long int threshold = cycle_diff_1 - cycle_diff_2;

	printf("threshold: %ld\n ignore: %d\n",threshold,setTest);

	/**Threshold calculation over**/

	//Calling eviction buffer formation function
	pageBasedEviction(buff,randBuff,buff_size,num_pages,num_el,threshold,&val,pind);

	printf("val: %lf\n",val);

	for(int i=0;i<16;i++)
		printf("%lu \n",pind[i]);

	int dataPageOffset = KB(4)/sizeof(datatype);

	asm volatile ("CPUID\n\t":::);
	asm volatile ("clflush (%0)"::"r"(randBuff));
	/*for(int i=0;i<16;i++){
		datatype *p = &buff[pind[i]];
		asm volatile ("clflush (%0)"::"r"(p));
	}*/
	asm volatile ("CPUID\n\t":::);

	setTest += randBuff[0];
        RDTSC_START(cycle_1);
	setTest += randBuff[0];
	RDTSC_STOP(cycle_2);

	cycle_diff_1 = cycle_2 - cycle_1;        
	
	for(int i=0;i<16;i++)
	  setTest+=buff[pind[i]*dataPageOffset];

	RDTSC_START(cycle_1);
	setTest += randBuff[0];
	RDTSC_STOP(cycle_2);

	cycle_diff_2 = cycle_2 - cycle_1;
	printf("first: %ld \t second: %ld\n",cycle_diff_1,cycle_diff_2);
        printf("ignore: %d\n",setTest);

#endif

/*#ifdef ELEMENT_EVICT
	//data offset needed to be set tpo 64B per cacheline
	size_t data_offset = cacheline/sizeof(datatype);

	register unsigned long int k=0;
	register unsigned long int temp =0;

	asm volatile ("CPUID\n\t":::);

	for(int i=0;i<num_el;i++){
		int *p = buff+i;
		asm volatile ("clflush (%0)"::"r"(p));
	}
	asm volatile ("CPUID\n\t":::);

	//for(int idx=0;idx<num_el;idx++){

	unsigned long int ctr=0;
	//  for(;ctr<16;ctr++)
	//  k+=buff[ctr*data_offset];

	RDTSC_START(cycle_1);
	temp+ = buff[0];
	RDTSC_STOP(cycle_2);

	cycle_diff = cycle_2 - cycle_1;

	ctr=1;
	while(1){

		k+=buff[ctr*data_offset];

		RDTSC_START(cycle_1);
		temp += buff[0];
		RDTSC_STOP(cycle_2);

		if((cycle_2-cycle_1)>cycle_diff)
			break;
		else{
			ctr++;
		}

	}

	printf("Ctr: %lu \t temp: %lu\n",ctr,temp);          
	//}
#endif*/

	printf("End of operation\n");

	free(buff);
	return 0;
}

//Creating eviction buffer
void pageBasedEviction(datatype *buff,datatype *randBuff,size_t buff_size,size_t num_pages,size_t num_el,long int threshold,double *ret_val,unsigned long *pind){

	unsigned long int randPageInd; 
	unsigned long pind1,pind2,pind3,pind4,pind5,pind6,pind7,pind8;
	unsigned long pind9,pind10,pind11,pind12,pind13,pind14,pind15,pind16;

	int dataPageOffset = KB(4)/sizeof(datatype); 

	unsigned long long cycle_1,cycle_2;
	long int cycle_diff_1,cycle_diff_2,cycle_diff_final;
	register unsigned long long int temp_1 =0,temp_2 =0;

	/*asm volatile ("CPUID\n\t":::);
	for(int i=0;i<num_pages;i++){
		int *p = buff+i*dataPageOffset;
		asm volatile ("clflush (%0)"::"r"(p));
	}*/

	printf("threshold (2): %ld\n",threshold);

	int ctr=0;
	int loopCtr =0;

	srand(time(NULL));
	while(1){
		
		loopCtr++;
		/*asm volatile ("CPUID\n\t":::);
		asm volatile ("clflush (%0)"::"r"(randBuff));
		for(int i=0;i<num_pages;i++){
			datatype *p = buff+(i*dataPageOffset);
			asm volatile ("clflush (%0)"::"r"(p));
		}
		asm volatile ("CPUID\n\t":::);*/

		for(int i=0;i<num_pages;i++)
			temp_1+= buff[i*dataPageOffset];

		RDTSC_START(cycle_1);
		temp_2 += randBuff[0];
		RDTSC_STOP(cycle_2);

		cycle_diff_1 = cycle_2 - cycle_1;

		randPageInd = rand() % (num_pages + 1);  
		
		for(int i=0;i<num_pages;i++){
			if(randPageInd !=i)
				temp_1+= buff[i*dataPageOffset];
		}

		RDTSC_START(cycle_1);
		temp_2 += randBuff[0];
		RDTSC_STOP(cycle_2);

		cycle_diff_2 = cycle_2 - cycle_1;       
	
		cycle_diff_final = cycle_diff_1 - cycle_diff_2;

		printf("loopCtr: %d \t cycle_diff_2: %ld \t cycle_diff_1: %ld \t cycle_diff_final: %ld \t randPageInd: %ld\n",
			loopCtr,cycle_diff_2,cycle_diff_1,cycle_diff_final,randPageInd);

		if(cycle_diff_final>500){
			ctr++;
			
			switch(ctr){

				case 1:
					pind1 = randPageInd;
					break;
				case 2:
					pind2 = randPageInd;
					break;
				case 3:
					pind3 = randPageInd;
					break;
				case 4:
					pind4 = randPageInd;
					break;
				case 5:
					pind5 = randPageInd;
					break;
				case 6:
					pind6 = randPageInd;
					break;
				case 7:
					pind7 = randPageInd;
					break;
				case 8:
					pind8 = randPageInd;
					break;
				case 9:
					pind9 = randPageInd;
					break;
				case 10:
					pind10 = randPageInd;
					break;
				case 11:
					pind11 = randPageInd;
					break;
				case 12:
					pind12 = randPageInd;
					break;
				case 13:
					pind13 = randPageInd;
					break;
				case 14:
					pind14 = randPageInd;
					break;
				case 15:
					pind15 = randPageInd;
					break;
				case 16:
					pind16 = randPageInd;
					break;

			}

			*ret_val+=(double)(abs(temp_2-temp_1)/(*ret_val));
			temp_2 = 0;
			temp_1 = 0;
		}

		if(ctr==16)
			break;
	}

	printf("Total number of tries (loopCtr): %d\n",loopCtr);
	/*printf("%lu \t %lu \t %lu \t %lu \t %lu \t %lu \t %lu \t %lu \t"
	  "%lu \t %lu \t %lu \t %lu \t %lu \t %lu \t %lu \t %lu \n",
	  pind1,pind2,pind3,pind4,pind5,pind6,pind7,pind8,pind9,pind10,pind11,pind12,pind13,pind14,pind15,pind16);*/

	pind[0] = pind1; pind[1] = pind2; pind[2] = pind3; pind[3] = pind4; 
	pind[4] = pind5; pind[5] = pind6; pind[6] = pind7; pind[7] = pind8;
	pind[8] = pind9; pind[9] = pind10; pind[10] = pind11; pind[11] = pind12;
	pind[12] = pind13; pind[13] = pind14; pind[14] = pind15; pind[15] = pind16;
}

///////////////////////////////////////////////CREATING MULTIPLE SETS///////////////////////////////////////////////
/*
void pageBasedEvictionMultSets(datatype *buff,unsigned long **cacheSetList,double *ret_val,int numCacheSets, size_t buff_size,size_t num_pages,size_t num_el){

	unsigned long int randPageInd; 
	unsigned long pind1,pind2,pind3,pind4,pind5,pind6,pind7,pind8;
	unsigned long pind9,pind10,pind11,pind12,pind13,pind14,pind15,pind16;

	unsigned long long cycle_1,cycle_2,cycle_diff_1,cycle_diff_2;
	register unsigned long long int temp_1 =0,temp_2 =0;

	int dataPageOffset  = KB(4)/sizeof(datatype);
	int currNumPages    = num_pages;
	int numCacheSetsLocal = numCacheSets;

	datatype **pagePointer = (datatype **)malloc(currNumPages*sizeof(datatype *));
	if(!pagePointer){
		printf("Error in allocating page pointer buffer\n");
		return;
	}

	datatype **tempPagePointer = (datatype **)malloc((currNumPages-17)*sizeof(datatype *));
	if(!tempBuffPointer){
		printf("Error in allocating temp page pointer buffer\n");
		return;
	}

	for(int i=1;i<currNumPages;i++)
		*(pagePointer+i) = &buff[i*dataPageOffset];

	*ret_val = 1.0;

	while(numCacheSetsLocal){

	  	asm volatile ("CPUID\n\t":::);
		for(int i=0;i<currNumPages;i++){
		   int *p = *(pagePointer+i);
		   asm volatile ("clflush (%0)"::"r"(p));
		}
		asm volatile ("CPUID\n\t":::);

	        ///// Discovering 1 set ///
		int ctr=0;
		srand(time(NULL));
		while(1){

			for(int i=1;i<currNumPages;i++)
			   temp_1+= **(pagePointer+i);

			RDTSC_START(cycle_1);
			temp_2 += **(pagePointer);
			RDTSC_STOP(cycle_2); 

			cycle_diff_1 = cycle_2 - cycle_1;

			randPageInd = rand() % (currNumPages + 1);  

			for(int i=1;i<num_pages;i++){
			   if(randPageInd !=i)
			    temp_1+= **(pagePointer+i);
			}

			RDTSC_START(cycle_1);
			temp_2 += **(pagePointer);
			RDTSC_STOP(cycle_2);

			cycle_diff_2 = cycle_2 - cycle_1;     

			if(cycle_diff_1>cycle_diff_2){
				ctr++;

				switch(ctr){

				   case 1:
					pind1 = randPageInd;
					break;
				   case 2:
					pind2 = randPageInd;
					break;
				   case 3:
					pind3 = randPageInd;
					break;
				   case 4:
					pind4 = randPageInd;
					break;
				   case 5:
					pind5 = randPageInd;
					break;
				   case 6:
					pind6 = randPageInd;
					break;
				   case 7:
					pind7 = randPageInd;
					break;
				   case 8:
					pind8 = randPageInd;
					break;
				   case 9:
					pind9 = randPageInd;
					break;
				   case 10:
					pind10 = randPageInd;
					break;
				   case 11:
					pind11 = randPageInd;
					break;
				   case 12:
					pind12 = randPageInd;
					break;
				   case 13:
					pind13 = randPageInd;
					break;
				   case 14:
					pind14 = randPageInd;
					break;
				   case 15:
					pind15 = randPageInd;
					break;
				   case 16:
					pind16 = randPageInd;
					break;

			  	}
				*ret_val+=(double)((temp_2-temp_1)/ret_val);
				temp_2 = 0;
				temp_1 = 0;
			}

			if(ctr==16)
			  break;  
		}

		*(cacheSetList + (numCacheSets-numCacheSetsLocal)*17) = *(pagePointer); 
		*(cacheSetList + (numCacheSets-numCacheSetsLocal)*17 + 1) = *(pagePointer + pind1); 
		*(cacheSetList + (numCacheSets-numCacheSetsLocal)*17 + 2) = *(pagePointer + pind2); 
		*(cacheSetList + (numCacheSets-numCacheSetsLocal)*17 + 3) = *(pagePointer + pind3); 
		*(cacheSetList + (numCacheSets-numCacheSetsLocal)*17 + 4) = *(pagePointer + pind4); 
		*(cacheSetList + (numCacheSets-numCacheSetsLocal)*17 + 5) = *(pagePointer + pind5); 
		*(cacheSetList + (numCacheSets-numCacheSetsLocal)*17 + 6) = *(pagePointer + pind6); 	
		*(cacheSetList + (numCacheSets-numCacheSetsLocal)*17 + 7) = *(pagePointer + pind7); 
		*(cacheSetList + (numCacheSets-numCacheSetsLocal)*17 + 8) = *(pagePointer + pind8); 
		*(cacheSetList + (numCacheSets-numCacheSetsLocal)*17 + 9) = *(pagePointer + pind9); 
		*(cacheSetList + (numCacheSets-numCacheSetsLocal)*17 + 10) = *(pagePointer + pind10); 
		*(cacheSetList + (numCacheSets-numCacheSetsLocal)*17 + 11) = *(pagePointer + pind11); 
		*(cacheSetList + (numCacheSets-numCacheSetsLocal)*17 + 12) = *(pagePointer + pind12); 
		*(cacheSetList + (numCacheSets-numCacheSetsLocal)*17 + 13) = *(pagePointer + pind13); 
		*(cacheSetList + (numCacheSets-numCacheSetsLocal)*17 + 14) = *(pagePointer + pind14); 
		*(cacheSetList + (numCacheSets-numCacheSetsLocal)*17 + 15) = *(pagePointer + pind15); 
		*(cacheSetList + (numCacheSets-numCacheSetsLocal)*17 + 16) = *(pagePointer + pind16); 

		*(pagePointer) = NULL;	
		*(pagePointer + pind1) = NULL;	*(pagePointer + pind2) = NULL; *(pagePointer + pind3) = NULL; *(pagePointer + pind4) = NULL;		
		*(pagePointer + pind5) = NULL;	*(pagePointer + pind6) = NULL; *(pagePointer + pind7) = NULL; *(pagePointer + pind8) = NULL;		
		*(pagePointer + pind9) = NULL;	*(pagePointer + pind10) = NULL; *(pagePointer + pind11) = NULL; *(pagePointer + pind12) = NULL;		
		*(pagePointer + pind13) = NULL;	*(pagePointer + pind14) = NULL; *(pagePointer + pind15) = NULL; *(pagePointer + pind16) = NULL;		

		int temp_ctr =0;
		for(int i=0;i<currNumPages;i++){
		  if(*(pagePointer+i)){
			*(tempPagePointer+temp_ctr) = *(pagePointer+i);
			temp_ctr++;
		  }
		}

		currNumPages -=17;
		
		pagePointer = (datatype **)realloc(pagePointer,currNumPages*sizeof(datatype *));

		for(int i=0;i<currNumPages;i++){
			*(pagePointer+i) = *(tempPagePointer + i);
		
		tempPagePointer = (datatype **)realloc(tempPagePointer,(currNumPages-17)*sizeof(datatype *));

		numCacheSetsLocal--;
	}
}

*/


/* RDTSC_START(cycle_1);
   temp_1 += buff[0];
   RDTSC_STOP(cycle_2);

   cycle_diff = cycle_2 - cycle_1;

   unsigned long int ctr=0;

   srand(time(NULL));
   while(1){

   randPageInd = rand() % ((num_pages + 1 -0) + 0);  

//if(ctr<50)
//printf("%p\n",(buff+randPageInd*dataPageOffset));

temp_2+=buff[randPageInd*dataPageOffset];

//temp_2+=buff[ctr*dataPageOffset];
RDTSC_START(cycle_1);
temp_1 += buff[0];
RDTSC_STOP(cycle_2);

if((cycle_2-cycle_1)>cycle_diff)
break;
else{
ctr++;
}  
}*/

// printf("Ctr: %lu \n",ctr);          
//printf("Ctr: %lu \t temp_1: %lu \t temp_2: %lu\n",ctr,temp_1,temp_2);          

//*ret_val = abs(temp_2-temp_1);


