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

typedef int datatype;
static int load_data = 0;
#define maccess(ADDR) { load_data = load_data ^ *((datatype*)ADDR); }


#define MB(X) ((X)*1024*1024)
#define KB(X) ((X)*1024) 
#define VA_PA_SIZE 8192
#define uniqueSameSlicePerCacheSet 100
#define force_inline __attribute__((always_inline))

#define numRndMessage 300//100000

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
  static const int h0[] = { 6, 10, 12, 14, 16, 17, 18, 20, 22, 24, 25, 26, 27, 28, 30, 32, 33, 35, 36 };
  static const int h1[] = { 7, 11, 13, 15, 17, 19, 20, 21, 22, 23, 24, 26, 28, 29, 31, 33, 34, 35, 37 };

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

void multiEvictSetDetFunc(CSList *temp);
int main(int argc,char *argv[]){

        set_affinity(0);

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

	fpHandle = fopen("SVMZC.cl","r");
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

	cl_kernel kern = clCreateKernel(program,"svmZcKern",&err);
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

        #define buffInMBSize 24
	#define pageSize 4
  	datatype *buffHost;
	size_t buffSz;

  	buffSz = MB(buffInMBSize);  //determining buffer 1 size 
	int numEl = buffSz/sizeof(datatype); //Number of elements in buffer 1
	unsigned int numPages = buffSz/KB(pageSize); //Number of pages in buff 1

  	printf("Number of pages in buffer 1: %u\n",numPages); 
	printf("Number of elements in buffer 1: %d\n",numEl);
	printf("Size of buff_size_1: %lu\n",buffSz);

 	buffHost = (datatype *)clSVMAlloc(ctx,CL_MEM_READ_WRITE,buffSz,2); //Allocating SVM buffer 1
	if(!buffHost){
	   printf("Error in allocating buffer SVM buffer 1\n");
	   return EXIT_FAILURE;
	}

	printf("Host buffer address (host): %p\n",buffHost);
	
	//Mapping the SVM buffer 1 to populate the buffer 
	printf("Initializing buffer 1\n");
	err = clEnqueueSVMMap(cQ,CL_TRUE,CL_MAP_WRITE,buffHost,buffSz,0,NULL,NULL);
	if(err!=CL_SUCCESS){
	   printf("Err in SVM map\n");
	   return EXIT_FAILURE;
	}
	
	for(int i =0;i<numEl;i++)
	 buffHost[i] = i;
	
	err = clEnqueueSVMUnmap(cQ,buffHost,0,NULL,NULL);
	if(err!=CL_SUCCESS){
	   printf("Err in SVM unmap\n");
	   return EXIT_FAILURE;
	}

	cl_mem buffDev = clCreateBuffer(ctx,CL_MEM_READ_WRITE|CL_MEM_USE_HOST_PTR,buffSz,buffHost,&err);
	if(err!=CL_SUCCESS){
	   printf("Err in creating device buffer\n");
	   return EXIT_FAILURE;
	}
	
	buffHost = (datatype *)clEnqueueMapBuffer(cQ,buffDev,CL_TRUE,CL_MAP_READ|CL_MAP_WRITE,0,buffSz,0,NULL,NULL,&err);
        if(err!=CL_SUCCESS){
	 printf("Error in mapping the host buffer #1\n");
	 return EXIT_FAILURE;
        }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////Determining physical address based eviction set//////////////////////////////////

        init_pagemap();
  
  	size_t numElsPerPage = (KB(4)/sizeof(datatype));
	
 	CSList *head,*temp;
 
  	head = NULL;
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
	
	printf("totalUniqueSet: %d\n",totalUniqueSet);
  	/////////////////////////////////////////////////////////////////////////////////
 
	temp = head;
 	int samectr=0;
 	while(temp){
  	  //printf("Detected set number: %d\n",samectr);
  	  multiEvictSetDetFunc(temp);
  	  samectr++;
  	  temp = temp->next;
 	}
	
	printf("samectr: %d\n",samectr);

  	////////////////////////////cache clear buffer allocation and transfer/////////////////////////////////////////////////////
		
	size_t cacheClearSize = KB(768);
		
	int *cacheClearBuffH = (int *)malloc(cacheClearSize);  	
	int cacheClearNumEls = cacheClearSize/sizeof(int);
	
	printf("Cache clear buffer num els: %d\n",cacheClearNumEls);
	
	for(int m =0 ;m<cacheClearNumEls;m++)
		cacheClearBuffH[m] = cacheClearNumEls - m;
	
	
	cl_mem cacheClearBuffDev = clCreateBuffer(ctx,CL_MEM_READ_WRITE,cacheClearSize,NULL,&err);
	if(err!=CL_SUCCESS){
	   printf("Err in creating device cache clear buffer\n");
	   return EXIT_FAILURE;
	}
	
	err = clEnqueueWriteBuffer(cQ,cacheClearBuffDev,CL_TRUE,0,cacheClearSize,cacheClearBuffH,0,NULL,NULL);
	if(err!=CL_SUCCESS){
		printf("Error in writing the cache clear buffer from host to device\n");
		return EXIT_FAILURE;
	}
	

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////Developing GPU transfer/////////////////////////////////////////////

	temp = head;
	
	size_t globalSize = 4096+32;	
	size_t localSize = 32;//kern_pref_wgsize_mult*2;//
	
	cl_event event;
	cl_ulong time_start;
	cl_ulong time_end;

	//while(temp){
	
	for(int i = 0;i<temp->totalSliceCtr;i++ ){
		
		printf("Current slice counter: %d\n",i);
			
		int localsameslcctr= temp->sameSliceCtr[i];
		//printf("%d\tlocalsameslcctr: %d\n",i,localsameslcctr);
		size_t sameSliceSz = localsameslcctr*sizeof(int);

		int *sameSliceInd = (int *) malloc(sameSliceSz);
		if(!sameSliceInd){
			printf("Error in allocating host side same slice Index\n");
			return EXIT_FAILURE;
		}
			
		for(int j=0;j<localsameslcctr;j++)
			sameSliceInd[j] = temp->selectedIndex[i][j];

		cl_mem sameSliceIndDev = clCreateBuffer(ctx,CL_MEM_READ_WRITE,sameSliceSz,NULL,&err);
		if(err!=CL_SUCCESS){
			printf("Err in creating device index buffer: %d\n",err);
			return EXIT_FAILURE;
		}
			
		err = clEnqueueWriteBuffer(cQ,sameSliceIndDev,CL_TRUE,0,sameSliceSz,sameSliceInd,0,NULL,NULL);
		if(err!=CL_SUCCESS){
			printf("Err in copying buffer from host to device\n");
			return EXIT_FAILURE;
		}
			
		err = clSetKernelArg(kern,0,sizeof(cl_mem),(void *)&buffDev);
		if(err != CL_SUCCESS){
			printf("Error in setting the kernel argument 0\n");
			return EXIT_FAILURE;
		}
		
			
		err = clSetKernelArg(kern,1,sizeof(cl_mem),(void *)&sameSliceIndDev);
		if(err != CL_SUCCESS){
			printf("Error in setting the kernel argument 1\n");
			return EXIT_FAILURE;
		}
			
		err = clSetKernelArg(kern,2,sizeof(cl_mem),(void *)&cacheClearBuffDev);
			
		if(err != CL_SUCCESS){
			printf("Error in setting the kernel argument 2: %d\n",err);
			return EXIT_FAILURE;
		}
			
		err = clSetKernelArg(kern,3,sizeof(int),&localsameslcctr);
			
		if(err != CL_SUCCESS){
			printf("Error in setting the kernel argument 3: %d\n",err);
			return EXIT_FAILURE;
		}
		
		err = clSetKernelArg(kern,4,sizeof(int),&cacheClearNumEls);
			
		if(err != CL_SUCCESS){
			printf("Error in setting the kernel argument 4: %d\n",err);
			return EXIT_FAILURE;
		}

		err = clSetKernelArg(kern,5,sizeof(size_t),&globalSize);
			
		if(err != CL_SUCCESS){
			printf("Error in setting the kernel argument 4: %d\n",err);
			return EXIT_FAILURE;
		}
		
			
		for(int j=0;j<numRndMessage;j++){
			
			printf("numRndMessage: %d\n",j);
			err = clEnqueueNDRangeKernel(cQ,kern,1,NULL,&globalSize,&localSize,0,NULL,&event);
			if(err!=CL_SUCCESS){
				printf("Unable to launch a kernel \t Error: %d\n",err);
				return EXIT_FAILURE;
			}
			
			err = clFlush(cQ);
			if(err!=CL_SUCCESS){
				printf("Unable to flush queue \t Error: %d\n",err);
				return EXIT_FAILURE;
			}

			err = clWaitForEvents(1,&event);
			if(err!=CL_SUCCESS){
				printf("Error clWaitForEvents \t Error: %d\n",err);
				return EXIT_FAILURE;
			}
			
			err = clFinish(cQ);
			if(err!=CL_SUCCESS){
				printf("Error clFinish \t Error: %d\n",err);
				return EXIT_FAILURE;
			}

			clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, NULL);
			clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(time_end), &time_end, NULL);

			double nanoSeconds = time_end-time_start;
			printf("OpenCl Execution time is: %0.3f milliseconds \n",nanoSeconds / 1000000.0);

			usleep(100);
		}

		free(sameSliceInd);
		err = clReleaseMemObject(sameSliceIndDev);
		if(err!=CL_SUCCESS){
			printf("Err in releasing device index buffer\n");
			return EXIT_FAILURE;
		}
			
	}
		
	//printf("\n");
	//temp = temp->next;
	//}
	
	clReleaseKernel(kern);
	clReleaseProgram(program);
	clReleaseCommandQueue(cQ);
	clReleaseContext(ctx);

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


