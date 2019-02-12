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

	#define mbSize 64
  	datatype *buffHost;
	size_t buffSz;

  	buffSz = MB(mbSize);  //determining buffer 1 size 

	printf("Size of buffer: %d MB\n",mbSize);

	int numEl = buffSz/sizeof(datatype); //Number of elements in buffer 1
	unsigned int numPages = buffSz/KB(4); //Number of pages in buff 1
 	buffHost = (datatype *)clSVMAlloc(ctx,CL_MEM_READ_WRITE,buffSz,2); //Allocating SVM buffer 1

	printf("Number of elements in buffer: %d\n",numEl);
  	printf("Number of pages in buffer : %u\n",numPages); 

	if(!buffHost){
	   printf("Error in allocating buffer SVM buffer 1\n");
	   return EXIT_FAILURE;
	}

	printf("buffHost address : %p\n",buffHost);
	
	//Mapping the SVM buffer 1 to populate the buffer 
	printf("Initializing host buffer 1\n");
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
	
	buffHost = (datatype *)clEnqueueMapBuffer(cQ,buffDev,CL_TRUE,CL_MAP_READ|CL_MAP_WRITE,0,buffSz,0,NULL,NULL,err);
        if(err!=CL_SUCCESS){
	 printf("Error in mapping the host buffer #1\n");
	 return EXIT_FAILURE;
        }

	//clSetKernelArgSVMPointer(kern,0,buff_1);

	err = clSetKernelArg(kern,0,sizeof(cl_mem),&buffDev);
	if(err != CL_SUCCESS){
	   printf("Error in setting the kernel argument 2\n");
	   return EXIT_FAILURE;
	}

	int elmts = 10;

	err = clSetKernelArg(kern,1,sizeof(int),&elmts);
	if(err != CL_SUCCESS){
	   printf("Error in setting the kernel argument 2\n");
	   return EXIT_FAILURE;
	}

	size_t globalSize = 1;	
	size_t localSize = 1;//kern_pref_wgsize_mult*2;//

	//for(int i=0;i<10;i++){

	  err = clEnqueueNDRangeKernel(cQ,kern,1,NULL,&globalSize,&localSize,0,NULL,NULL);
	  if(err!=CL_SUCCESS){
	   printf("Unable to launch a kernel \t Error: %d\n",err);
	   return EXIT_FAILURE;
	  }
	//}


	

	clFinish(cQ);
	
	clSVMFree(ctx,buffHost);
	clReleaseKernel(kern);
	clReleaseProgram(program);
	clReleaseCommandQueue(cQ);
	clReleaseContext(ctx);
}
