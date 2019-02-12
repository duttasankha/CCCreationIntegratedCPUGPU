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
#include <pthread.h>

void * pThreadFunc(void *arg);
typedef int dataType;
typedef struct testArg{
	volatile int *p;
}ta;
#define force_inline __attribute__((always_inline))

static inline force_inline void flush(void* p) {
    asm volatile ("clflush 0(%0)\n"
      :
      : "c" (p)
      : "rax");
}

int main(){
	
	cl_platform_id *platform;
	cl_device_id *devices;
	cl_uint num_devices, addr_data;
	cl_int err;
	cl_uint num_platform;
	cl_program program;
	cl_context ctx;
	cl_kernel kern;
	cl_command_queue cQ;

	FILE *fpHandleGPU;
	char* progSource;
	size_t progSize;

	char *log;
	size_t logSize;
	err = clGetPlatformIDs(0,NULL,&num_platform);

	if(err!=CL_SUCCESS){
		printf("Error in getting number of platforms\n");
		exit(EXIT_FAILURE);
	}

	printf("Number of Platforms: %d\n",num_platform);
 
	platform = (cl_platform_id *)malloc(num_platform*sizeof(cl_platform_id));
	if(!platform){
		printf("Memory alloaction error\n");
		//exit(1);
		exit(EXIT_FAILURE);
	}

	err = clGetPlatformIDs(num_platform, platform, NULL);
	if(err !=CL_SUCCESS) {
		printf("Couldn't find any platforms");
		//exit(1);
		exit(EXIT_FAILURE);
	}

	/* Determine number of connected devices */
	err = clGetDeviceIDs(platform[0], CL_DEVICE_TYPE_GPU,0,NULL,&num_devices);
	if(err !=CL_SUCCESS) {
		printf("Couldn't find the number of devices\n");
		exit(EXIT_FAILURE);
		//exit(1);
	}

	/* Access connected devices */
	devices = (cl_device_id*) malloc(sizeof(cl_device_id) * num_devices);
	err = clGetDeviceIDs(platform[0], CL_DEVICE_TYPE_GPU,num_devices,devices,NULL);
	if(err!=CL_SUCCESS){
		printf("Error in getting the Intel integrated GPU\n");
		exit(EXIT_FAILURE);
	}

	ctx = clCreateContext(NULL,1,&devices[0],NULL,NULL,&err);
	if(err!=CL_SUCCESS){
		printf("Error in creating context\n");
		exit(EXIT_FAILURE);
	}
	else
		printf("Context created successfully\n");

	fpHandleGPU = fopen("test.cl","r");
	fseek(fpHandleGPU,0,SEEK_END);
	progSize=ftell(fpHandleGPU);
	rewind(fpHandleGPU);

	progSource = (char *)malloc(progSize*sizeof(char)+1);
	if(!progSource){
		printf("Error in allocating program source memory\n");
		exit(EXIT_FAILURE);
	}
	progSource[progSize] = '\0';

	fread(progSource,sizeof(char),progSize,fpHandleGPU);
	fclose(fpHandleGPU);

	program = clCreateProgramWithSource(ctx,1,(const char **)&progSource,&progSize,&err);
	if(err!=CL_SUCCESS){
		printf("Error in creating program from source\n");
		//goto releaseContext;
		exit(EXIT_FAILURE);
	}

	free(progSource);

	err  = clBuildProgram(program,1,&devices[0],NULL,NULL,NULL);
	if(err!=CL_SUCCESS){

		clGetProgramBuildInfo(program,devices[0],CL_PROGRAM_BUILD_LOG,0,NULL,&logSize);
		log=(char *)malloc(logSize+1);
		log[logSize]='\0';
		clGetProgramBuildInfo(program,devices[0],CL_PROGRAM_BUILD_LOG,logSize+1,log,NULL);
		printf("\n==========ERROR=========\n%s\n=======================\n",log);
		free(log);
		exit(EXIT_FAILURE);
	}
	else
		printf("Program build successfully\n");
		
	kern = clCreateKernel(program,"testKern",&err);
	if(err!=CL_SUCCESS){
		printf("Error in kernel creation");
		exit(EXIT_FAILURE);
	}

	cQ = clCreateCommandQueue(ctx,devices[0],CL_QUEUE_PROFILING_ENABLE,&err);
	if(err!=CL_SUCCESS){
		printf("Error in creating the command queue\n");
		exit(EXIT_FAILURE);
	}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
	dataType *buffGPU;
	size_t buffSizeGPU;
	int numEls = 100;
	volatile int *syncVar = (int *)malloc(sizeof(int));

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

 	buffGPU = (dataType *)clSVMAlloc(ctx,CL_MEM_READ_WRITE,numEls*sizeof(dataType),2); //Allocating SVM buffer 1
	if(!buffGPU){
	   printf("Error in allocating buffer SVM buffer 1\n");
	   exit(EXIT_FAILURE);
	}

	err = clEnqueueSVMMap(cQ,CL_TRUE,CL_MAP_WRITE,buffGPU,numEls*sizeof(dataType),0,NULL,NULL);
	if(err!=CL_SUCCESS){
	   printf("Err in SVM map\n");
	   exit(EXIT_FAILURE);
	}
	
	for(int i =0;i<numEls;i++)
	 buffGPU[i] = i;
	
	err = clEnqueueSVMUnmap(cQ,buffGPU,0,NULL,NULL);
	if(err!=CL_SUCCESS){
	   printf("Err in SVM unmap\n");
	   exit(EXIT_FAILURE);
	}
	
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////	
/* 	syncVar = (dataType *)clSVMAlloc(ctx,CL_MEM_READ_WRITE,sizeof(dataType),2); //Allocating SVM buffer 1*/
/*	if(!syncVar){*/
/*	   printf("Error in allocating buffer SVM buffer 1\n");*/
/*	   exit(EXIT_FAILURE);*/
/*	}*/

/*	err = clEnqueueSVMMap(cQ,CL_TRUE,CL_MAP_WRITE,(volatile int *)syncVar,sizeof(dataType),0,NULL,NULL);*/
/*	if(err!=CL_SUCCESS){*/
/*	   printf("Err in SVM map\n");*/
/*	   exit(EXIT_FAILURE);*/
/*	}*/
/*	*/
/*	syncVar[0] = 1;*/
/*	*/
/*	err = clEnqueueSVMUnmap(cQ,syncVar,0,NULL,NULL);*/
/*	if(err!=CL_SUCCESS){*/
/*	   printf("Err in SVM unmap\n");*/
/*	   exit(EXIT_FAILURE);*/
/*	}*/
	
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	cl_mem buffDev = clCreateBuffer(ctx,CL_MEM_READ_WRITE|CL_MEM_USE_HOST_PTR,numEls*sizeof(dataType),buffGPU,&err);
	if(err!=CL_SUCCESS){
		printf("Err in creating device buffer\n");
		return EXIT_FAILURE;
	}
	
	buffGPU = (dataType *)clEnqueueMapBuffer(cQ,buffDev,CL_TRUE,CL_MAP_READ|CL_MAP_WRITE,0,numEls*sizeof(dataType),0,NULL,NULL,&err);
        if(err!=CL_SUCCESS){
		printf("Error in mapping the host buffer #1\n");
		return EXIT_FAILURE;
        }
        
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*	cl_mem syncVarDev = clCreateBuffer(ctx,CL_MEM_READ_WRITE|CL_MEM_USE_HOST_PTR,sizeof(dataType),syncVar,&err);*/
/*	if(err!=CL_SUCCESS){*/
/*	   printf("Err in creating device buffer\n");*/
/*	   return EXIT_FAILURE;*/
/*	}*/
/*	*/
/*	syncVar = (dataType *)clEnqueueMapBuffer(cQ,syncVarDev,CL_TRUE,CL_MAP_READ|CL_MAP_WRITE,0,sizeof(dataType),0,NULL,NULL,&err);*/
/*        if(err!=CL_SUCCESS){*/
/*	 printf("Error in mapping the host buffer #1\n");*/
/*	 return EXIT_FAILURE;*/
/*        }*/

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	err = clSetKernelArg(kern,0,sizeof(cl_mem),&buffDev);
	if(err != CL_SUCCESS){
		printf("Error in setting the kernel argument 0\n");
		return EXIT_FAILURE;
	}
		
/*	err = clSetKernelArg(kern,1,sizeof(cl_mem),&syncVarDev);*/
/*	if(err != CL_SUCCESS){*/
/*		printf("Error in setting the kernel argument 0\n");*/
/*		return EXIT_FAILURE;*/
/*	}*/
	
	err = clSetKernelArg(kern,1,sizeof(int),&numEls);
	if(err != CL_SUCCESS){
		printf("Error in setting the kernel argument 2\n");
		return EXIT_FAILURE;
	}
	
	*syncVar = 1;
	
	pthread_t thread;
	ta *la = (ta *)malloc(sizeof(ta));
	
	la->p = syncVar;
	
	err = pthread_create(&thread,NULL,pThreadFunc,(void *)la);//(void *)arg
	if(err){
		printf("Error in creating pthread 1\n");
		return EXIT_FAILURE;
	}

	size_t globalSize = 1;	
	size_t localSize = 1;//kern_pref_wgsize_mult*2;//
	printf("launching kernel\n");
	err = clEnqueueNDRangeKernel(cQ,kern,1,NULL,&globalSize,&localSize,0,NULL,NULL);
	if(err!=CL_SUCCESS){
		printf("Unable to launch a kernel \t Error: %d\n",err);
		return EXIT_FAILURE;
	}
	
	err = clFlush(cQ);
	if(err!=CL_SUCCESS){
		printf("Error clFlush\t Error: %d\n",err);
		return EXIT_FAILURE;
	}

	err = clFinish(cQ);
	if(err!=CL_SUCCESS){
		printf("Error clFinish\t Error: %d\n",err);
		return EXIT_FAILURE;
	}
	
	printf("back here\n");
	
	*syncVar = 0;
	
	pthread_join(thread,NULL);
	
	return 0;
}



void * pThreadFunc(void *arg){
	
	ta *la = (ta *)arg;
	volatile int *var = la->p;
	
	while(*var)
		flush((void *)var);
	
}

/*	printf("pthread launched\n");*/
/*	ta *la = (ta *)arg;*/
/*	int *syncVar = la->p;*/
/*	cl_command_queue cQ = la->q;*/
/*	*/
/*	int pp = 0;*/
/*	cl_int err;*/
/*	printf("hahaha\n");*/

/*	*/
/*	while(pp<5){*/
/*		*/
/*		while(syncVar[0]);*/
/*		*/
/*		err = clEnqueueSVMMap(cQ,CL_FALSE,CL_MAP_WRITE,syncVar,sizeof(dataType),0,NULL,NULL);*/
/*		if(err!=CL_SUCCESS){*/
/*		   printf("Err in SVM map\n");*/
/*		   exit(EXIT_FAILURE);*/
/*		}*/

/*		for(int i =0;i<1000;i++)*/
/*		flush((void *)&syncVar[0]);*/
/*		*/
/*		syncVar[0] = 1;*/

/*		err = clEnqueueSVMUnmap(cQ,syncVar,0,NULL,NULL);*/
/*		if(err!=CL_SUCCESS){*/
/*		   printf("Err in SVM unmap\n");*/
/*		   exit(EXIT_FAILURE);*/
/*		}*/

/*		err = clFlush(cQ);*/
/*		if(err!=CL_SUCCESS){*/
/*			printf("Unable to flush queue \t Error: %d\n",err);*/
/*			return EXIT_FAILURE;*/
/*		}*/

/*		*/
/*		*/
/*		printf("cpu: %d\n",pp);*/

/*		pp++;*/
/*	}*/
