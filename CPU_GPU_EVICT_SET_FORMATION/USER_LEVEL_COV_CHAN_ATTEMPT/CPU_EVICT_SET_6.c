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
#include "libpfc.h"


int main(){


#ifdef DUMP
 pfcDumpEvts();
#endif

 pfcPinThread(0);

 if(pfcInit() != 0){
		printf("Could not open /sys/module/pfc/* handles; Is module loaded?\n");
		exit(1);
 }
 else
   printf("PFC initialization successful\n");
	

}
