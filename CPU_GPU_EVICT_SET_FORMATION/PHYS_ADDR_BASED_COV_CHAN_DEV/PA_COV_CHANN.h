#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <CL/cl.h>
#include <CL/cl_platform.h>
#include <sched.h>
#include <sys/time.h>
#include <time.h>
#include <inttypes.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <assert.h>
#include <pthread.h>


#define MB(X) ((X)*1024*1024)
#define KB(X) ((X)*1024)
#define VA_PA_SIZE 8192

#define force_inline __attribute__((always_inline))
#define buffInMBSize 24
#define pageSize 4
#define uniqueSameSlicePerCacheSet 10
#define SLICEID 0


typedef int datatype;

typedef struct cacheSetList{
 
	uint64_t cacheSet;
	uint64_t VA[VA_PA_SIZE];
	uint64_t PA[VA_PA_SIZE];
	int 	 sameSetIndex[VA_PA_SIZE];
	int 	 addrInSetCtr;
 
	uint64_t **selectedPA;
	uint64_t **selectedVA;
	int	 **selectedIndex;
	int 	 sameSliceCtr[uniqueSameSlicePerCacheSet];
	int 	 sliceID[uniqueSameSlicePerCacheSet];
	int	 totalSliceCtr;

	struct cacheSetList *next;

}CSList;

/*static inline force_inline void maccess(void* p)*/
/*{*/
/*  asm volatile ("movq (%0), %%rax\n"*/
/*    :*/
/*    : "r" (p)*/
/*    : "rax");*/
/*}*/

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

//int g_pagemap_fd = -1;

void init_pagemap();
uint64_t frame_number_from_pagemap(uint64_t value);

uint64_t get_physical_addr(uint64_t virtual_addr);
int get_cache_slice(uint64_t phys_addr, int bad_bit);
void set_affinity(int coreid);
void multiEvictSetDetFunc(CSList *);
CSList *evictSetCreateFunc(datatype *);

datatype *cpuDataBuffInit();

////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////Only GPU related variables and functions///////////////////////////

cl_platform_id *platform;
cl_device_id *devices;
cl_uint num_devices, addr_data;
cl_int err;
cl_uint num_platform;
cl_program program;
cl_context ctx;
cl_kernel kern,kernRE;
cl_command_queue cQ;

FILE *fpHandleGPU;
char* progSource;
size_t progSize;

char *log;
size_t logSize;

void gpuPlatInitFunc();
datatype *gpuDataBuffInit();

//////////////////////////////////////////L3 Cache RE related//////////////////////////////////////////
int* gpuNonCacheSetIndDetFunc(datatype *,CSList *,int ,int *);
int gpuL3EvictRangeDetFunc(datatype *,int *,int);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////GPU <-> CPU covert channel communication related declarations///////////////////////////
#define numRndMessage 10
#define numThreads 2
void *gpuSideSendFunc(void *);
void *cpuSideRecvFunc(void *);	

typedef struct pthreadArgs{

	CSList 	 *evictSetArg;
	datatype *buffArg;
	int chosenSlice;
	volatile int *syncVar; ///only used for cpu thread
	float *dummy;

}pThArgsSt;


///within GPU pthread another pthread would be launched to flush the LLC cache 
typedef struct gpuPThr{
	datatype *gpuPthBuff;
	int *gpuChosenInd;
	int numInd;
	volatile int *gpuSync;
}gPthSt;

void* pThGPUFlushFunc(void *arg);







