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


int main(int argc,char *argv[]){

 
 int *vec;
 unsigned long long cycle_1,cycle_2;

 vec = (int *)malloc(5*sizeof(int));
 
 for(int i =0;i<5;i++)
  vec[i] = i;

 int temp =0;
 int *p = vec;
 asm volatile ("clflush (%0)"::"r"(p));

 RDTSC_START(cycle_1);
 temp += vec[0]; 
 RDTSC_STOP(cycle_2);

 printf("First access: %llu\n",(cycle_2-cycle_1));
 
 for(int i =1;i<5;i++)
  temp+=vec[i];
 
 RDTSC_START(cycle_1);
 temp += vec[0]; 
 RDTSC_STOP(cycle_2);

 printf("Second access: %llu\n",(cycle_2-cycle_1));

 printf("Ignore: %d\n",temp);

 free(vec);
 return 0;

}
