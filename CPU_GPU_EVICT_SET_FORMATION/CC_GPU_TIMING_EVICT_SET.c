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

#pragma OPENCL EXTENSION cl_khr_int64_extended_atomics : enable
#define MB(X) ((X)*1024*1024)

typedef unsigned short ushort;
size_t getKernelInfo(cl_kernel ,cl_device_id);

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

	fpHandle = fopen("CC_GPU_TIMING_EVICT_SET.cl","r");
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

	cl_kernel kern = clCreateKernel(program,"CCGPUEVICTIONSET_Single",&err);
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

	printf("short size: %ld\n",sizeof(unsigned short));

	ushort *buff_1_host,*buff_2_host; 
	long int *ctrBuffHost;

	int buff_1_single_access = 32;//Assuming that the warp size is 32
	int buff_2_single_access = 32;
	int repeat_1 = 1;
	int repeat_2 = MB(4)/(buff_2_single_access*sizeof(ushort)); 

        printf("# Times buffer 2 would be accessed: %d\n",repeat_2);
/////////////////////////ALLOCATING HOST BUFFER/////////////////////////////////

	printf("Allocating the host side buffer\n");

	size_t buff_1_size = buff_1_single_access * repeat_1 * sizeof(ushort);//1 cacheline size = 64B

	int err = posix_memalign((void **)&buff_1_host,4096,buff_1_size);//(int *)malloc(buff_1_size);
	if(err){
	  printf("Error in allocating host input buffer\n");
	  return EXIT_FAILURE;
	}

	size_t buff_2_size = buff_2_single_access * repeat_2 * sizeof(ushort);//

	int err = posix_memalign((void **)&buff_2_host,4096,buff_2_size);//(int *)malloc(buff_1_size);
	if(err){
	  printf("Error in allocating host input buffer\n");
	  return EXIT_FAILURE;
	}

//////////////////////////////INITIALIZING HOST BUFFER 1 and 2////////////////////////////////////
	
	int buff_1_num_el = buff_1_single_access*repeat_1; 
	printf("Number of elements in buffer 1: %d\n",buff_1_num_el);

	for(int i =0;i<buff_1_num_el;i++)
	   buff_1_host[i] = i;

	int buff_2_num_el = buff_2_single_access*repeat_1; 
	printf("Number of elements in buffer 2: %d\n",buff_2_num_el);

	for(int i =0;i<buff_2_num_el;i++)
	   buff_2_host[i] = i;

///////////////////////////ALLOCATING/MAPPING DEVICE SIDE BUFFER/////////////////////////////////

	printf("Allocating device side buffer\n");	

	cl_mem buff_1_dev = clCreateBuffer(ctx,CL_MEM_USE_HOST_POINTER,buff_1_size,NULL, &err);//CL_MEM_READ_WRITE
	if(err != CL_SUCCESS){
	   printf("Error in allocating input device buffer 1\n");
	   return EXIT_FAILURE;
	}	

	cl_mem buff_2_dev = clCreateBuffer(ctx,CL_MEM_USE_HOST_POINTER,buff_2_size,NULL,&err);
	if(err != CL_SUCCESS){
	   printf("Error in allocating input device buffer 2\n");
	   return EXIT_FAILURE;
	}	

/////////////////////////TRANSFERRING FROM HOST TO DEVICE BUFFER/////////////////////////////////

       buff_1_host = (ushort *)clEnqueueMapBuffer(cQ,buff_1_dev,CL_TRUE,CL_MAP_READ|CL_MAP_WRITE,0,buff_1_size,0,NULL,NULL,err);
       if(!buff_1_host){
	printf("Error in mapping the host buffer #1\n");
	return EXIT_FAILURE;
       }

       buff_2_host = (ushort *)clEnqueueMapBuffer(cQ,buff_2_dev,CL_TRUE,CL_MAP_READ|CL_MAP_WRITE,0,buff_2_size,0,NULL,NULL,err);
       if(!buff_2_host){
	printf("Error in mapping the host buffer #2\n");
	return EXIT_FAILURE;
       }
	
/////////////////////////GETTING KERNEL INFO AND WARP SIZE/////////////////////////////////

	size_t kern_pref_wgsize_mult = getKernelInfo(kern,devices[0]);

 	if(kern_pref_wgsize_mult<0){
		printf("Error in getting kernel info\n");
 	        return EXIT_FAILURE;
  	}

 	printf("kern_pref_wgsize_mult: %lu\n",kern_pref_wgsize_mult);

/////////////////////////SETTING KERNEL ARGUMENTS/////////////////////////////////

	printf("Setting kernel arguments\n");
	err = clSetKernelArg(kern,0,sizeof(cl_mem),&buff_1_dev);
	if(err != CL_SUCCESS){
	   printf("Error in setting the kernel argument 0\n");
	   return EXIT_FAILURE;
	}

	err = clSetKernelArg(kern,1,sizeof(cl_mem),&buff_1_dev);
	if(err != CL_SUCCESS){
	   printf("Error in setting the kernel argument 1\n");
	   return EXIT_FAILURE;
	}

	err = clSetKernelArg(kern,2,sizeof(int),&buff_1_num_el);
	if(err != CL_SUCCESS){
	   printf("Error in setting the kernel argument 2\n");
	   return EXIT_FAILURE;
	}

	err = clSetKernelArg(kern,3,sizeof(int),&buff_2_num_el);
	if(err != CL_SUCCESS){
	   printf("Error in setting the kernel argument 3\n");
	   return EXIT_FAILURE;
	}

/////////////////////////PREPARING KERNEL LAUNCH///////////////////////////////

       	printf("Launching the kernel\n");
	size_t globalSize = (kern_pref_wgsize_mult + 1);//kern_pref_wgsize_mult*2;//
	size_t localSize = (kern_pref_wgsize_mult + 1);//kern_pref_wgsize_mult*2;//

	err = clEnqueueNDRangeKernel(cQ,kern,1,NULL,&globalSize,&localSize,0,NULL,NULL);
	if(err!=CL_SUCCESS){
	 printf("Unable to launch a kernel \t Error: %d\n",err);
	 return EXIT_FAILURE;
	}

	clFinish(cQ);
	
	/*clWaitForEvents(1, &event);
		
	cl_ulong time_start;
	cl_ulong time_end;

	clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, NULL);
	clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(time_end), &time_end, NULL);

	double nanoSeconds = time_end-time_start;
	printf("OpenCl Execution time is: %0.3f ms \n",nanoSeconds/1000000.0);

	ctr--;//=2;*/
	
/////////////////////////TRANSFERRING FROM DEVICE TO HOST BUFFER/////////////////////////////////

	printf("Reading data from device to host\n");

	err = clEnqueueReadBuffer(cQ,inBuffDev,CL_TRUE,0,mainBuffSize,inBuffHost,0,NULL,NULL);	
	if(err != CL_SUCCESS){
	  printf("Error in copying host buffer from device to host\n");
	  return EXIT_FAILURE;
	}

	err = clEnqueueReadBuffer(cQ,ctrBuffDev,CL_TRUE,0,2*sizeof(long int),ctrBuffHost,0,NULL,NULL);	
	if(err != CL_SUCCESS){
	  printf("Error in copying host buffer from device to host\n");
	  return EXIT_FAILURE;
	}

	printf("Printing the ctr values\n");
	for(int i =0;i<2;i++)
		printf("%ld\t",ctrBuffHost[i]);

	clReleaseMemObject(inBuffDev);
	clReleaseMemObject(ctrBuffDev);
	
	free(inBuffHost);
	free(ctrBuffHost);

	clReleaseKernel(kern);
	clReleaseProgram(program);
	clReleaseCommandQueue(cQ);
	clReleaseContext(ctx);

}

size_t getKernelInfo(cl_kernel kern,cl_device_id device){

	cl_int err;
	size_t kern_glob_work_size[3];
	size_t kern_wgsize;
	cl_ulong kern_loc_mem_size;
	size_t kern_pref_wgsize_mult;
	cl_ulong kern_priv_mem_size;

	/*err = clGetKernelWorkGroupInfo(kern,device,CL_KERNEL_GLOBAL_WORK_SIZE,3*sizeof(size_t),kern_glob_work_size,NULL);
	if(err != CL_SUCCESS){
		printf("Error in getting CL_KERNEL_GLOBAL_WORK_SIZE\n");
		return;
	}*/

	err = clGetKernelWorkGroupInfo(kern,device,CL_KERNEL_WORK_GROUP_SIZE,sizeof(size_t),&kern_wgsize,NULL);
	if(err != CL_SUCCESS){
		printf("Error in getting CL_KERNEL_WORK_GROUP_SIZE\n");
		return -1;
	}

	err = clGetKernelWorkGroupInfo(kern,device,CL_KERNEL_LOCAL_MEM_SIZE,sizeof(cl_ulong),&kern_loc_mem_size,NULL);
	if(err != CL_SUCCESS){
		printf("Error in getting CL_KERNEL_LOCAL_MEM_SIZE\n");
		return -1;
	}

	err = clGetKernelWorkGroupInfo(kern,device,CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE,sizeof(size_t),&kern_pref_wgsize_mult,NULL);
	if(err != CL_SUCCESS){
		printf("Error in getting CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE\n");
		return -1;
	}

	err = clGetKernelWorkGroupInfo(kern,device,CL_KERNEL_PRIVATE_MEM_SIZE,sizeof(cl_ulong),&kern_priv_mem_size,NULL);
	if(err != CL_SUCCESS){
		printf("Error in getting CL_KERNEL_PRIVATE_MEM_SIZE\n");
		return -1;
	}

	printf("CL_KERNEL_GLOBAL_WORK_SIZE: x: %lu \t y: %lu \t z: %lu\n"
	       "CL_KERNEL_WORK_GROUP_SIZE: %lu\n"
               "CL_KERNEL_LOCAL_MEM_SIZE: %lu\n"
	       "CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE: %d\n"
	       "CL_KERNEL_PRIVATE_MEM_SIZE: %lu\n", kern_glob_work_size[0],kern_glob_work_size[1],kern_glob_work_size[2],
		kern_wgsize,kern_loc_mem_size,kern_pref_wgsize_mult,kern_priv_mem_size); 


   return kern_pref_wgsize_mult;
}













