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

#define MB(X) ((X)*1024*1024)
#define KB(X) ((X)*1024)
typedef int datatype;

/*void maccess(void* p)*/
/*{*/
/*  asm volatile ("movq (%0), %%rax\n"*/
/*    :*/
/*    : "c" (p)*/
/*    : "rax");*/
/*}*/

static int load_data = 0;
#define maccess(ADDR) { load_data = load_data ^ *((datatype*)ADDR); }

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

void populate_rand_addr_buff(datatype *buff,volatile uint64_t** eviction_random,unsigned int num_pages);
void evictSetDeterminationFunc(volatile uint64_t** eviction_random,unsigned int num_pages);
int main(int argc,char *argv[]){

	cl_platform_id *platform;
	cl_device_id *devices;
	cl_uint num_devices, addr_data;
	cl_int err;

	cl_uint num_platform;
	err = clGetPlatformIDs(0,NULL,&num_platform);

	if(err!=CL_SUCCESS){
		printf("Error in getting number of platforms\n");
		return EXIT_FAILURE;
	}

	printf("Number of Platforms: %d\n",num_platform);

	platform = (cl_platform_id *)malloc(num_platform*sizeof(cl_platform_id));
	if(!platform){
		printf("Memory alloaction error\n");
		//exit(1);
		return EXIT_FAILURE;
	}

	err = clGetPlatformIDs(num_platform, platform, NULL);
	if(err !=CL_SUCCESS) {
		printf("Couldn't find any platforms");
		//exit(1);
		return EXIT_FAILURE;
	}

	/* Determine number of connected devices */
	err = clGetDeviceIDs(platform[0], CL_DEVICE_TYPE_GPU,0,NULL,&num_devices);
	if(err !=CL_SUCCESS) {
		printf("Couldn't find the number of devices\n");
		return EXIT_FAILURE;
		//exit(1);
	}

	/* Access connected devices */
	devices = (cl_device_id*) malloc(sizeof(cl_device_id) * num_devices);
	err = clGetDeviceIDs(platform[0], CL_DEVICE_TYPE_GPU,num_devices,devices,NULL);
	if(err!=CL_SUCCESS){
		printf("Error in getting the Intel integrated GPU\n");
		return EXIT_FAILURE;
	}

	cl_context ctx = clCreateContext(NULL,1,&devices[0],NULL,NULL,&err);
	if(err!=CL_SUCCESS){
		printf("Error in creating context\n");
		return EXIT_FAILURE;
	}
	else
		printf("Context created successfully\n");

	FILE *fpHandle;
	char* progSource;
	size_t progSize;

	fpHandle = fopen("CC_GPU_TIMING_EVICT_SET_1.cl","r");
	fseek(fpHandle,0,SEEK_END);
	progSize=ftell(fpHandle);
	rewind(fpHandle);

	progSource = (char *)malloc(progSize*sizeof(char)+1);
	if(!progSource){
		printf("Error in allocating program source memory\n");
		return EXIT_FAILURE;
	}
	progSource[progSize] = '\0';

	fread(progSource,sizeof(char),progSize,fpHandle);
	fclose(fpHandle);

	cl_program program = clCreateProgramWithSource(ctx,1,(const char **)&progSource,&progSize,&err);
	if(err!=CL_SUCCESS){
		printf("Error in creating program from source\n");
		//goto releaseContext;
		return EXIT_FAILURE;
	}

	free(progSource);

	char *log;
	size_t logSize;

	err  = clBuildProgram(program,1,&devices[0],NULL,NULL,NULL);
	if(err!=CL_SUCCESS){

		clGetProgramBuildInfo(program,devices[0],CL_PROGRAM_BUILD_LOG,0,NULL,&logSize);
		log=(char *)malloc(logSize+1);
		log[logSize]='\0';
		clGetProgramBuildInfo(program,devices[0],CL_PROGRAM_BUILD_LOG,logSize+1,log,NULL);
		printf("\n==========ERROR=========\n%s\n=======================\n",log);
		free(log);
		return EXIT_FAILURE;
	}
	else
		printf("Program build successfully\n");

	cl_kernel kern = clCreateKernel(program,"CCGPUEvictionSet",&err);
	if(err!=CL_SUCCESS){
		printf("Error in kernel creation");
		return EXIT_FAILURE;
	}

	cl_command_queue cQ = clCreateCommandQueue(ctx,devices[0],CL_QUEUE_PROFILING_ENABLE,&err);
	if(err!=CL_SUCCESS){
		printf("Error in creating the command queue\n");
		return EXIT_FAILURE;
	}

/////////////////////////// PROGRAM PREPARATION START //////////////////////////

  	datatype *buff_1_host,*buff_2_host;
	volatile uint64_t** eviction_random;

	size_t buff_size_1,buff_size_2;
  	buff_size_1 = MB(64);  //determining buffer 1 size 
	buff_size_2 = MB(8);  //determining buffer 2 size 

	printf("Size of buff_size_1: %lu\n",buff_size_1);
	printf("Size of buff_size_2: %lu\n",buff_size_2);

	int num_el_1 = buff_size_1/sizeof(datatype); //Number of elements in buffer 1
	int num_el_2 = buff_size_2/sizeof(datatype); //Number of elements in buffer 2

	printf("Number of elements in buffer 1: %d\n",num_el_1);
	printf("Number of elements in buffer 2: %d\n",num_el_2);

	unsigned int num_pages_1 = buff_size_1/KB(4); //Number of pages in buff 1
  	printf("Number of pages in buffer 1: %u\n",num_pages_1); 

	unsigned int num_pages_2 = buff_size_2/KB(4); //Number of pages in buff 2
  	printf("Number of pages in buffer 2: %u\n",num_pages_2);
	
 	buff_1_host = (datatype *)clSVMAlloc(ctx,CL_MEM_READ_WRITE,buff_size_1,2); //Allocating SVM buffer 1
	if(!buff_1_host){
	   printf("Error in allocating buffer SVM buffer 1\n");
	   return EXIT_FAILURE;
	}

 	buff_2_host = (datatype *)clSVMAlloc(ctx,CL_MEM_READ_WRITE,buff_size_2,2); //Allocating SVM buffer 2
	if(!buff_2_host){
	   printf("Error in allocating buffer SVM buffer\n");
	   return EXIT_FAILURE;
	}

	printf("buff_1 address (host) : %p\n",buff_1_host);
	
      	printf("buff_2 address (host) : %p\n",buff_2_host);
	
	//Mapping the SVM buffer 1 to populate the buffer 
	printf("Initializing buffer 1\n");
	err = clEnqueueSVMMap(cQ,CL_TRUE,CL_MAP_WRITE,buff_1_host,buff_size_1,0,NULL,NULL);
	if(err!=CL_SUCCESS){
	   printf("Err in SVM map\n");
	   return EXIT_FAILURE;
	}
	
	for(int i =0;i<num_el_1;i++)
	 buff_1_host[i] = i;
	
	err = clEnqueueSVMUnmap(cQ,buff_1_host,0,NULL,NULL);
	if(err!=CL_SUCCESS){
	   printf("Err in SVM unmap\n");
	   return EXIT_FAILURE;
	}

	//Mapping the SVM buffer 2 to populate the buffer 	
	printf("Initializing buffer 2\n");
	err = clEnqueueSVMMap(cQ,CL_TRUE,CL_MAP_WRITE,buff_2_host,buff_size_2,0,NULL,NULL);
	if(err!=CL_SUCCESS){
	   printf("Err in SVM map\n");
	   return EXIT_FAILURE;
	}
	
	for(int i =0;i<num_el_2;i++)
	 buff_2_host[i] = num_el_2 - i;
	
	err = clEnqueueSVMUnmap(cQ,buff_2_host,0,NULL,NULL);
	if(err!=CL_SUCCESS){
	   printf("Err in SVM unmap\n");
	   return EXIT_FAILURE;
	}

	eviction_random = (volatile uint64_t **)malloc((num_pages_1+1)*sizeof(uint64_t *));//num_pages_1
	if(!eviction_random){
    	  printf("Error in allocating memory eviction buffer\n");
	  return EXIT_FAILURE;
  	}
 	
	printf("buff_2_host+2*1024: %p\n",buff_2_host+2*1024);

  	eviction_random[0] = (uint64_t *)(buff_2_host+2*1024);
	printf("Start populating the eviction buffer addresses\n");
  	populate_rand_addr_buff(buff_1_host,eviction_random,num_pages_1);
  	printf("End populating\n");

	evictSetDeterminationFunc(eviction_random,num_pages_1);

	printf("Back from function\n");

	/*cl_mem buff_2 = clCreateBuffer(ctx,CL_MEM_READ_WRITE|CL_MEM_USE_HOST_PTR,10*sizeof(ushort),buff_1,&err);
	if(err!=CL_SUCCESS){
	   printf("Err in creating device buffer\n");
	   return EXIT_FAILURE;
	}
	
	buff_1 = (ushort *)clEnqueueMapBuffer(cQ,buff_2,CL_TRUE,CL_MAP_READ|CL_MAP_WRITE,0,10*sizeof(ushort),0,NULL,NULL,err);
        if(err!=CL_SUCCESS){
	 printf("Error in mapping the host buffer #1\n");
	 return EXIT_FAILURE;
        }

	printf("buff_1 address (host): %p\n",buff_1);

	clSetKernelArgSVMPointer(kern,0,buff_1);

	err = clSetKernelArg(kern,1,sizeof(cl_mem),&buff_2);
	if(err != CL_SUCCESS){
	   printf("Error in setting the kernel argument 2\n");
	   return EXIT_FAILURE;
	}

	int size = 10;

	err = clSetKernelArg(kern,2,sizeof(int),&size);
	if(err != CL_SUCCESS){
	   printf("Error in setting the kernel argument 2\n");
	   return EXIT_FAILURE;
	}*/

/*	size_t globalSize = 1;	*/
/*	size_t localSize = 1;//kern_pref_wgsize_mult*2;//*/

/*	//for(int i=0;i<10;i++){*/

/*	  err = clEnqueueNDRangeKernel(cQ,kern,1,NULL,&globalSize,&localSize,0,NULL,NULL);*/
/*	  if(err!=CL_SUCCESS){*/
/*	   printf("Unable to launch a kernel \t Error: %d\n",err);*/
/*	   return EXIT_FAILURE;*/
/*	  }*/
	//}

//	clFinish(cQ);
	
	clSVMFree(ctx,buff_1_host);
	clSVMFree(ctx,buff_2_host);
	//clReleaseMemObject(buff_2);
	clReleaseKernel(kern);
	clReleaseProgram(program);
	clReleaseCommandQueue(cQ);
	clReleaseContext(ctx);
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
    
    eviction_random[i] = (uint64_t *)(buff + selected_pages[i]*1024);

    /*if(i<100)
 	printf("%d) \t sel page: %lu \t eviction: %p \t deref eviction: %d \t buff: %p \t deref buff:%d\n",
	   i,selected_pages[i],eviction_random[i],*eviction_random[i],buff + selected_pages[i]*1024,*(buff + selected_pages[i]*1024));*/
  }
 

}

#define EVICT_SET_MISS_RATIO 0.99
#define EVICT_SET_HIT_RATIO 0.99
#define NUM_RNDS 5000
#define THRESHOLD 400
//This is the eviction set creation after discussing with daimeng
void evictSetDeterminationFunc(volatile uint64_t** eviction_random,unsigned int num_pages){

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
    
/*     if(!(evEndPtr%5))*/
/*      fprintf(stderr,"currernt ctr : %d\t",evEndPtr);*/

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
     
/*     if(!(evEndPtr%5))*/
/*      fprintf(stderr,"mr : %f\n",mr);*/

     if(mr>EVICT_SET_MISS_RATIO)
      break;
     else
      evEndPtr++;
  } 

  fprintf(stderr,"mr: %f \t Num of page addresses to reach eviction: %d\n\n",mr,evEndPtr);

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
   
   for(int rnds = 0;rnds<10;rnds++){

 	  int currPos = 0;
   	  int detectedPages = 0;

	  while(currPos<evEndPtr-1){
	   
	   mr=0;

	   for(int j=0;j<NUM_RNDS;j++){
		
	    flush((void *)eviction_random[0]);
           
	    for(int k=currPos+1;k<evEndPtr;k++)
	     flush((void *)tempEvictBuff[k]);
	   
/*	    if(detectedPages)*/
/*	      for(int k=0;k<detectedPages;k++)*/
/*	       flush((void *)finalEvictBuff[k]);*/

	    maccess((void *)eviction_random[0]);
	    asm volatile ("mfence");

/*	    if(detectedPages)*/
/*	      for(int k=0;k<detectedPages;k++)*/
/*	       maccess((void *)finalEvictBuff[k]);*/

//	    asm volatile ("mfence");

            for(int k=currPos+1;k<evEndPtr;k++)
	      maccess((void *)tempEvictBuff[k]);  

	    time = rdtsc();
  	    maccess((void *)eviction_random[0]);
	    delta_1 = rdtsc() - time;

	    if(delta_1>THRESHOLD)
	     mr += 1.0/NUM_RNDS;

	  }

	  if(mr<EVICT_SET_HIT_RATIO){
	    finalEvictBuff[detectedPages] = tempEvictBuff[currPos];
	    idxBuff[detectedPages] = currPos;
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

/*       if(rnds==0)*/
/*	  for(int i=0;i<detectedPages;i++)*/
/*	   fprintf(stderr,"page index: %d \t address: %p\n",idxBuff[i],finalEvictBuff[i]);*/
             
   }

}

