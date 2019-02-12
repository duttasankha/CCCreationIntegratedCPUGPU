#include "metrics_header.h"

int main(int argc,char *argv[]){

	printf("\n\tStart GPU Sender Host side\n-----------------------------------------\n");

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
		exit(1);
	}

	err = clGetPlatformIDs(num_platform, platform, NULL);
	if(err !=CL_SUCCESS) {
		perror("Couldn't find any platforms");
		exit(1);
	}

	printf("Enlisting Intel integrated GPU details\n");

	/* Determine number of connected devices */
	err = clGetDeviceIDs(platform[2], CL_DEVICE_TYPE_GPU,0,NULL,&num_devices);

	if(err !=CL_SUCCESS) {
		printf("Couldn't find the number of devices\n");
		exit(1);
	}

	/* Access connected devices */
	devices = (cl_device_id*) malloc(sizeof(cl_device_id) * num_devices);
	err = clGetDeviceIDs(platform[2], CL_DEVICE_TYPE_GPU,num_devices,devices,NULL);

	if(err!=CL_SUCCESS){
		printf("Error in getting the Intel integrated GPU\n");
	}

	cl_context ctx = clCreateContext(NULL,1,&devices[0],NULL,NULL,&err);

	if(err!=CL_SUCCESS){
		printf("Error in creating context\n");
		exit(1);
	}
	else
		printf("Context created successfully\n");

	FILE *fpHandle;
	char* progSource;
	size_t progSize;

	fpHandle = fopen("CC_GPU_KERNEL.cl","r");
	fseek(fpHandle,0,SEEK_END);
	progSize=ftell(fpHandle);
	rewind(fpHandle);

	progSource = (char *)malloc(progSize*sizeof(char)+1);
	if(!progSource){
		printf("Error in allocating program source memory\n");
		exit(0);
	}
	progSource[progSize] = '\0';

	fread(progSource,sizeof(char),progSize,fpHandle);
	fclose(fpHandle);

	cl_program program = clCreateProgramWithSource(ctx,1,(const char **)&progSource,&progSize,&err);
	if(err!=CL_SUCCESS){
		printf("Error in creating program from source\n");
		goto releaseContext;
	}

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
		goto releaseContext;
	}
	else
		printf("Program build successfully\n");

	cl_kernel kern = clCreateKernel(program,"GPUSenderKernel",err);

	if(err!=CL_SUCCESS){
		printf("Error in kernel creation");
		goto releaseProgram;
	}

	cl_command_queue cQ = clCreateCommandQueue(ctx,devices[0],0,&err);

	if(err!=CL_SUCCESS){
		printf("Error in creating the command queue\n");
		goto releaseKernel;
	}

	size_t SIZE = LLCassoc*cacheLine*LLCnumSets*buffConstFactor;

	cl_mem sendBuffDev = clCreateBuffer(ctx,CL_MEM_READ_WRITE,SIZE,NULL,&err);
	if(err!=CL_SUCCESS){
		printf("Error in allocating the device buffer\n");
		goto releaseCommandQueue;
	}

	err = clSetKernelArg(kern,0,sizeof(cl_mem),&sendBuffDev);

	if(err!=CL_SUCCESS){
		printf("Unable to set the kernel argument\n");
		goto releaseMemObject;
	}

	printf("Launching the kernel\n");
	size_t globalSize = LLCnumSets;
	size_t localSize = WGSize;

	err = clEnqueueNDRangeKernel(cQ,kern,1,NULL,&globalSize,&localSize,0,NULL,NULL);
	if(err!=CL_SUCCESS){
		printf("Unable to launch a kernel \t Error: %d\n",err);
		goto releaseMemObject;

	}
	clFinish(cQ);

	printf("Kernel execution is over\n");
releaseMemObject:
	clReleaseMemObject(sendBuffDev);
releaseCommandQueue:
	clReleaseCommandQueue(cQ);
releaseKernel:
	clReleaseKernel(kern);
releaseProgram:
	clReleaseProgram(program);
releaseContext:
	clReleaseContext(ctx);

	printf("Exiting the GPU sender function\n");
	return 0;
}
