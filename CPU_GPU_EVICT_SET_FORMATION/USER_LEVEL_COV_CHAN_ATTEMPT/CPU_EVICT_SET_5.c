// This is the eviction set creation based on the Daimeng code
// from the userspace.

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <string.h>

#define EVICT_SET_NUM_BLOCKS_MAX  64
#define EVICT_SET_BLOCK_SIZE 0x1000000
#define PAGE_SIZE 4096
#define PAGE_OFFSET(X) (X & (PAGE_SIZE-1))
#define EVICT_SET_ACCESS_STEPS 8
#define EVICT_SET_ACCESS_INC 3
#define MB(X) ((X)*1024*1024)
#define EVICT_SET_MISS_RATIO 0.8

/*#define LOAD(ADDR) {\*/
/*    asm(".intel_syntax_noprefix");\*/
/*    asm volatile ("mov %0, [%1]\n\t" : "=r" (fnr_read) : "r" (ADDR)); \*/
/*}*/

typedef int datatype;
static uint64_t fnr_ts0,fnr_ts1;

static int load_data = 0;
#define LOAD(ADDR) { load_data = load_data ^ *((datatype*)ADDR); }

typedef uint64_t MEM_ADDR;
static int FNR_THRESHOLD = 400;

typedef struct EvictSet {
    void * mappedMem[EVICT_SET_NUM_BLOCKS_MAX];
    int numMemBlocks;
    MEM_ADDR * addrList;
    int addrListLen;
} EvictSet;

uint64_t readtsc() {
  uint64_t a, d;
  asm volatile ("mfence");
  asm volatile ("rdtsc" : "=a" (a), "=d" (d));
  a = (d<<32) | a;
  asm volatile ("mfence");
  return a;
}

#define RELOAD_TIME(TARGET, RET) { \
    fnr_ts0 = readtsc(); \
    LOAD(TARGET); \
    fnr_ts1 = readtsc(); \
    RET = fnr_ts1 - fnr_ts0; \
}

#define EVICT(ES) { \
    accessEvictSet(ES); \
    asm volatile ("mfence"); \
}

static void accessEvictSet(struct EvictSet * s) {
    int i, j;
    for (i = 0; i < s->addrListLen - EVICT_SET_ACCESS_STEPS + 1; i += EVICT_SET_ACCESS_INC) {
        for (j = 0; j < EVICT_SET_ACCESS_STEPS; j ++) {
            LOAD(s->addrList[i+j]);
        }
    }
}

static float testEvictSetCompleteness(struct EvictSet * es, MEM_ADDR target_addr, int rounds) {
    int i;
    uint64_t ts, t0, t1, c0, c1;
    float mr, tsv, tv, cv;
    float mr0, mr1;

    mr = tsv = tv = cv = 0.0;
    for (i = 0; i < rounds; i++) {

        RELOAD_TIME(target_addr, ts);
        EVICT(es);
     
        if (ts > FNR_THRESHOLD)
            mr += 1.0 / rounds;
        tsv += ts * 1.0 / rounds;
     
    }

    mr0 = mr;

    fprintf(stderr, "mr0: %.02f\n", mr0);

/*    for (i = 0; i < rounds; i++) {*/
/*     */
/*        RELOAD_TIME(target_addr, ts);*/
/*        EVICT(es);*/
/*        */
/*        if (ts > FNR_THRESHOLD)*/
/*            mr += 1.0 / rounds;*/
/*        tsv += ts * 1.0 / rounds;*/
/*        */
/*        EVICT(es);*/
/*        LOAD(target_addr);*/
/*    }*/
/*   */
/*    mr1 = mr;*/
/*    fprintf(stderr, "mr0: %.02f\tmr1:%.02f\n", mr0, mr1);*/

    return mr0;// - mr1;
}

static struct EvictSet * CreateEvictionSet(MEM_ADDR addr){

    fprintf(stderr, "Creating eviction set by iteration.\n");

    int i, j, k;
    int num_of_iterations = 3;
    float cmp;
    int numPagesPerBlock = EVICT_SET_BLOCK_SIZE / PAGE_SIZE;
    struct EvictSet * ret = malloc(sizeof(struct EvictSet));
   
    ret->numMemBlocks = 0;
    ret->addrList = malloc(numPagesPerBlock * EVICT_SET_NUM_BLOCKS_MAX * sizeof(MEM_ADDR));
    ret->addrListLen = 0;

    MEM_ADDR offset = PAGE_OFFSET(addr);
   
    fprintf(stderr,"Populating addrList buffer start\n");
    for (i = 0; i < EVICT_SET_NUM_BLOCKS_MAX; i++) {
        ret->mappedMem[i] = mmap(NULL, EVICT_SET_BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
        ret->numMemBlocks ++;
        ret->addrListLen += numPagesPerBlock;
        for (j = 0; j < numPagesPerBlock; j++) {
            ret->addrList[i * numPagesPerBlock + j] = (MEM_ADDR)ret->mappedMem[i] + j * PAGE_SIZE + offset;
        }
        cmp = testEvictSetCompleteness(ret, addr, 1000);
        if (cmp > EVICT_SET_MISS_RATIO)
           break;
     }
     
     fprintf(stderr,"Populating addrList buffer end\n");
     fprintf(stderr, "addrListLen: %d \t cmp: %f\n", ret->addrListLen, cmp);
    
     MEM_ADDR * tmpAddrs = malloc(ret->addrListLen * sizeof(MEM_ADDR));
     int tmpAddrsLen;
     int numRemoval;

     fprintf(stderr,"Actually creating the eviction buffer start\n");

     for (k = 0; k < num_of_iterations; k++) {
        i = 0;
        numRemoval = 0;
        while (i < ret->addrListLen) {
            memcpy(tmpAddrs, ret->addrList, ret->addrListLen * sizeof(MEM_ADDR));
            tmpAddrsLen = ret->addrListLen;
            // Remove one item
            for (j = i; j < ret->addrListLen - 1; j++) {
                ret->addrList[j] = ret->addrList[j + 1];
            }
            ret->addrListLen -= 1;
            // Test result
            cmp = testEvictSetCompleteness(ret, addr, 1000);
            if (cmp > EVICT_SET_MISS_RATIO) {
                // fprintf(stderr, "0");
                numRemoval ++;
                continue;
            } else {
                // fprintf(stderr, "1");
                memcpy(ret->addrList, tmpAddrs, tmpAddrsLen * sizeof(MEM_ADDR));
                ret->addrListLen = tmpAddrsLen;
                i ++;
            }
	    fprintf(stderr,"numRemoval: %d \t i: %d\n\n",numRemoval,i);   
        }
    
        fprintf(stderr,"iter # %d \t cmp: %f\n",k,cmp);
        cmp = testEvictSetCompleteness(ret, addr, 1000);
        fprintf(stderr, "addrListLen: %d cmp: %f\n", ret->addrListLen, cmp);
    }

    return ret;

}

int main(){

 datatype *buff_1;

 size_t buff_size_1 = MB(16);  //allocating buffer size 
  
 int err = posix_memalign((void **)&buff_1,4096,buff_size_1); //allocating page aligned main buffer

 if(err!=0){
	printf("Err: %d\n",err);
	return EXIT_FAILURE;
 }

  int num_el_1 = buff_size_1/sizeof(datatype);

  for(int i=0;i<num_el_1;i++)
      buff_1[i] = i;

  MEM_ADDR addr = (MEM_ADDR)(buff_1 + 2*1024);

  fprintf(stderr,"addr: %x\n",addr);
  struct EvictSet *set = CreateEvictionSet(addr);

 return 0;
}



