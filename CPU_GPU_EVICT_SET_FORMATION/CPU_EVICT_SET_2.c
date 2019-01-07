#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
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
				"RDTSCP	\n\t"                           \
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

typedef int datatype;
void createEvictionSetFunction(datatype *buff,datatype *randbuff,double *retVal,long long int *offsetCtr,int num_el_per_cacheline,size_t num_offsets);

int main(int argc,char *argv[]){

	datatype *buff,*randBuff;
	//ull **addrOfBuff[totalCacheLine];
	unsigned long long cycle_1,cycle_2;
	long int cycle_diff_1,cycle_diff_2;

	size_t buff_size = MB(8);  //allocating buffer size 

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

	size_t num_offsets = buff_size/cacheline; //Calculating the number of elements in the main buffer
	int num_el_per_cacheline = cacheline/sizeof(datatype);
	size_t num_el = buff_size/sizeof(datatype); //Calculating the number of elements in the main buffer
	//printing details
	printf("Main buffer address: %p \t Random buffer address: %p \t Size: %ld \t Number of offsets: %ld\n",buff,randBuff,buff_size,num_offsets);
	printf("num_el_per_cacheline: %d\n",num_el_per_cacheline);
	printf("number of elements: %ld\n",num_el);

	//Populating the buffer
	asm volatile ("CPUID\n\t":::);
	srand(time(NULL));
	randBuff[0] = (int)rand();

	for(int i=0;i<num_el;i++)
		buff[i] = (int)rand();
	asm volatile ("CPUID\n\t":::);

       long long int offsetCtr=0;
       double retVal;
     
	createEvictionSetFunction(buff,randBuff,&retVal,&offsetCtr,num_el_per_cacheline,num_offsets);

        printf("ignore: %lf",retVal);
#ifdef PAGE_EVICT
	/*double val = 1; //dummy var
	unsigned long pind[16]; //variable to hold the eviciton buff page indices
	
	/**Calculating the threshold**/
	//Flushing address from all cache levels
	/*asm volatile ("CPUID\n\t":::);
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

	createEvictionSetFunction(buff,randBuff,buff_size,num_pages,num_el,threshold,&val,pind);*/

#endif

	printf("End of operation\n");
	
	free(randBuff);
	free(buff);
	return 0;
}


void createEvictionSetFunction(datatype *buff,datatype *randbuff,double *retVal,long long int *offsetCtr,int num_el_per_cacheline,size_t num_offsets){

  size_t cycle_1,cycle_2;
  size_t cycle_diff_rand_buff_1,cycle_diff_rand_buff_2;
  
  unsigned long long int dummyVar_1=0,dummyVar_2=0;
  
  *retVal = 1;
  //Flushing the whole cache
  asm volatile ("CPUID\n\t":::);
  for(int i=0;i<num_offsets;i++){
	datatype *p = buff+i*num_el_per_cacheline;
        flush(p);
  }
  
  //Access the data from RAM
  //RDTSC_START(cycle_1);
  cycle_1 = rdtsc();
  dummyVar_1 += randbuff[0];
  cycle_2 = rdtsc();
  //RDTSC_STOP(cycle_2);

  //Measuring the access time
  cycle_diff_rand_buff_1 = cycle_2 - cycle_1;
  printf("cycle_diff_1: %ld\n",cycle_diff_rand_buff_1);

  //Access the data from cache
  //RDTSC_START(cycle_1);
  cycle_1 = rdtsc();
  dummyVar_1 += randbuff[0];
  cycle_2 = rdtsc();
  //RDTSC_STOP(cycle_2);

  //Measuring the access time
  cycle_diff_rand_buff_2 = cycle_2 - cycle_1;
  printf("cycle_diff_2: %ld\n",cycle_diff_rand_buff_2);

  long long int offsetCtrTemp =0;
  int test =0;

  while(1){
  
   if(offsetCtrTemp<num_offsets){

     dummyVar_2+= buff[offsetCtrTemp*num_el_per_cacheline]; 

     //RDTSC_START(cycle_1);
     cycle_1 = rdtsc();
     dummyVar_1 += randbuff[0];
     cycle_2 = rdtsc();
     //RDTSC_STOP(cycle_2);

     cycle_diff_rand_buff_2 = cycle_2 - cycle_1;
     //printf("cycle_diff_2: %ld\toffsetCtrTemp: %lld\n",cycle_diff_rand_buff_2,offsetCtrTemp);

     if(cycle_diff_rand_buff_2>=cycle_diff_rand_buff_1){
        printf("cycle_diff_2: %ld\toffsetCtrTemp: %lld\n",cycle_diff_rand_buff_2,offsetCtrTemp);
	
	offsetCtrTemp =0;
        *offsetCtr = offsetCtrTemp;
        if(test==2)	
          break;
        else{
          printf("\n\n=====================TEST #%d============================\n\n",test);
          test++;
        }  
     }

     *retVal+=(double)(abs(dummyVar_2-dummyVar_1)/(*retVal));
      offsetCtrTemp++;
   }
   else{
    printf("No set found\n");
    break;
   } 
  }


} 





