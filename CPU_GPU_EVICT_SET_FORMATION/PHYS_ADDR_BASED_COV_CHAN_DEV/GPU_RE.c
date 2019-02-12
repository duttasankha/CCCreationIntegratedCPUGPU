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

#define MB(X) (X*1024*1024)
#define KB(X) (X*1024)
#define pageSzKB 4
#define buffSzMB 8

typedef int dataType;
#define force_inline __attribute__((always_inline))
static inline force_inline void flush(void* p) {
    asm volatile ("clflush 0(%0)\n"
      :
      : "c" (p)
      : "rax");
}

unsigned int *randIndDetFunc(int numElGPU){

	int num_pages = MB(buffSzMB)/KB(pageSzKB);
	printf("Number of pages: %d\n",num_pages);
	
	unsigned int* selected_pages = (unsigned int *)malloc(num_pages*sizeof(unsigned int));
	memset(selected_pages,0,num_pages*sizeof(unsigned int));

/*	unsigned int* selected_indices = (unsigned int *)malloc(numElGPU*sizeof(unsigned int));*/
/*	memset(selected_indices,0,numElGPU*sizeof(unsigned int));*/

	int ctr =0;
	bool matched_pages = false;

	srand(time(NULL));
	for(int i=0;i<num_pages;i++){
	//for(int i=0;i<numElGPU;i++){

		unsigned int pageNum = rand()%(num_pages + 1);
		//unsigned int indNum = rand()%(numElGPU + 1);
	
		for(unsigned int j=0;j<ctr;j++){

			if(selected_pages[j]==pageNum){
			//if(selected_indices[j]==indNum){
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
			//selected_indices[i] = indNum;
			ctr++;
		}
	}
	
	return selected_pages;
	//return selected_indices;
}


int queryWorkGrpDetailsFunc(cl_kernel kern,cl_device_id device){
	
	size_t paramSize;
	int warpLength;
	cl_int err;
	
	err = clGetKernelWorkGroupInfo(kern,device,CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE,0,NULL,&paramSize);
	if(err!=CL_SUCCESS){
		printf("Error in getting the kernel info: %d\n",err);
		return -1;
	}
	
	
	err = clGetKernelWorkGroupInfo(kern,device,CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE,paramSize,(void *)&warpLength,0);
	if(err!=CL_SUCCESS){
		printf("Error in getting the kernel info after getting the parameter size: %d\n",err);
		return -1;
	}	
	
	printf("Warp length: %d\n",warpLength);
	
	return warpLength;
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

	fpHandleGPU = fopen("GPU_RE.cl","r");
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
		
	kern = clCreateKernel(program,"reKernel",&err);
	if(err!=CL_SUCCESS){
		printf("Error in kernel creation");
		exit(EXIT_FAILURE);
	}

	cQ = clCreateCommandQueue(ctx,devices[0],CL_QUEUE_PROFILING_ENABLE,&err);
	if(err!=CL_SUCCESS){
		printf("Error in creating the command queue\n");
		exit(EXIT_FAILURE);
	}
	
	//////////////////////Quering warp/wavefront size || could be used for querying other (check clGetKernelWorkGroupInfo)
	int warpSize = queryWorkGrpDetailsFunc(kern,devices[0]);
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//This is related to the reverse enginnering of the GPU L3 cache
	
	dataType *dataHost;
	int *ptChaseRandIndxBuff;
	
	size_t dataSize = MB(buffSzMB);
	int numElGPU = dataSize/sizeof(dataType); //Number of elements in buffer 1
	 
	printf("Number of elements (GPU): %d\n",numElGPU);
	printf("Size of GPU buffer: %lu MB\n",dataSize/MB(1));
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 	dataHost = (dataType *)clSVMAlloc(ctx,CL_MEM_READ_WRITE,dataSize,2); //Allocating SVM buffer 1
	if(!dataHost){
	   printf("Error in allocating buffer SVM buffer 1\n");
	   exit(EXIT_FAILURE);
	}

	printf("Buff start addr: %p\n",dataHost);
	
	//Mapping the SVM buffer 1 to populate the buffer 
	printf("Initializing GPU buffer\n");
	err = clEnqueueSVMMap(cQ,CL_TRUE,CL_MAP_WRITE,dataHost,dataSize,0,NULL,NULL);
	if(err!=CL_SUCCESS){
	   printf("Err in SVM map\n");
	   exit(EXIT_FAILURE);
	}
	
	for(int i =0;i<numElGPU;i++)
	 dataHost[i] = i;
	
	err = clEnqueueSVMUnmap(cQ,dataHost,0,NULL,NULL);
	if(err!=CL_SUCCESS){
	   printf("Err in SVM unmap\n");
	   exit(EXIT_FAILURE);
	}
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
	int numPages = MB(buffSzMB)/KB(pageSzKB);
	int numElsPerPage = (KB(4)/sizeof(dataType));
	
	int numEl768KB = KB(768)/sizeof(dataType);//768 KB size of indices
	
	printf("Entering function to create non-paged based random index buffer\n");
	ptChaseRandIndxBuff = randIndDetFunc(numEl768KB);
	printf("Exited the function \n");
	
	cl_mem dataDev = clCreateBuffer(ctx,CL_MEM_READ_WRITE|CL_MEM_USE_HOST_PTR,dataSize,dataHost,&err);
	if(err!=CL_SUCCESS){
	   printf("Err in creating device buffer : %d\n",err);
	   return EXIT_FAILURE;
	}

	dataHost = (dataType *)clEnqueueMapBuffer(cQ,dataDev,CL_TRUE,CL_MAP_READ|CL_MAP_WRITE,0,dataSize,0,NULL,NULL,&err);
        if(err!=CL_SUCCESS){
	 printf("Error in mapping the host buffer: %d\n",err);
	 return EXIT_FAILURE;
        }
	
/*	cl_mem dataDev = clCreateBuffer(ctx,CL_MEM_READ_WRITE,dataSize,NULL,&err);*/
/*	if(err!=CL_SUCCESS){*/
/*		printf("Err in creating device index buffer: %d\n",err);*/
/*		return EXIT_FAILURE;*/
/*	}*/
/*	*/
/*	err = clEnqueueWriteBuffer(cQ,dataDev,CL_TRUE,0,dataSize,dataHost,0,NULL,NULL);*/
/*	if(err!=CL_SUCCESS){*/
/*		printf("Err in copying buffer from host to device\n");*/
/*		return EXIT_FAILURE;*/
/*	}*/

////////////////////////////////////////////////////////////THIS IS PAGE INDEX BASED/////////////////////////////////////////////////	

	cl_mem ptChaseRandIndxBuffDev = clCreateBuffer(ctx,CL_MEM_READ_WRITE,numPages*sizeof(unsigned int),NULL,&err);
	if(err!=CL_SUCCESS){
		printf("Err in creating device index buffer: %d\n",err);
		return EXIT_FAILURE;
	}
	
	err = clEnqueueWriteBuffer(cQ,ptChaseRandIndxBuffDev,CL_TRUE,0,numPages*sizeof(unsigned int),ptChaseRandIndxBuff,0,NULL,NULL);
	if(err!=CL_SUCCESS){
		printf("Err in copying buffer from host to device\n");
		return EXIT_FAILURE;
	}

////////////////////////////////////////////////////////////THIS IS ANY INDEX BASED/////////////////////////////////////////////////

/*	cl_mem ptChaseRandIndxBuffDev = clCreateBuffer(ctx,CL_MEM_READ_WRITE,numEl768KB*sizeof(unsigned int),NULL,&err);*/
/*	if(err!=CL_SUCCESS){*/
/*		printf("Err in creating device index buffer: %d\n",err);*/
/*		return EXIT_FAILURE;*/
/*	}*/
/*	*/
/*	err = clEnqueueWriteBuffer(cQ,ptChaseRandIndxBuffDev,CL_TRUE,0,numEl768KB*sizeof(unsigned int),ptChaseRandIndxBuff,0,NULL,NULL);*/
/*	if(err!=CL_SUCCESS){*/
/*		printf("Err in copying buffer from host to device\n");*/
/*		return EXIT_FAILURE;*/
/*	}*/

	err = clSetKernelArg(kern,0,sizeof(cl_mem),&dataDev);
	if(err != CL_SUCCESS){
		printf("Error in setting the kernel argument 0\n");
		return EXIT_FAILURE;
	}
	
/*	err = clSetKernelArgSVMPointer (kern,0,(void *)dataHost);*/
/*	if(err != CL_SUCCESS){*/
/*		printf("Error in setting the kernel argument 0\n");*/
/*		return EXIT_FAILURE;*/
/*	}*/
	
	err = clSetKernelArg(kern,1,sizeof(cl_mem),&ptChaseRandIndxBuffDev);
	if(err != CL_SUCCESS){
		printf("Error in setting the kernel argument 1\n");
		return EXIT_FAILURE;
	}

	err = clSetKernelArg(kern,2,sizeof(int),&numPages);
	if(err != CL_SUCCESS){
		printf("Error in setting the kernel argument 2\n");
		return EXIT_FAILURE;
	}
	
	int rndIdxLimit = 1;
	int numRnd = 2;

	size_t globalSize = 1;//4096+32;	
	size_t localSize = 1;//kern_pref_wgsize_mult*2;//
	cl_event event;
	cl_ulong tStart,tEnd;
	//testing the cache inclusiveness;
	#define numRun 100
	double execTimePerRnd[numRnd][numRun] ;
	
	
	//////////////////////////////////////////warm up ////////////////////////////////////////////////////
	
	err = clSetKernelArg(kern,3,sizeof(int),&rndIdxLimit);
	if(err != CL_SUCCESS){
		printf("Error in setting the kernel argument 3\n");
		return EXIT_FAILURE;
	}

	err = clSetKernelArg(kern,4,sizeof(int),&numRnd);
	if(err != CL_SUCCESS){
		printf("Error in setting the kernel argument 4\n");
		return EXIT_FAILURE;
	}

	err = clSetKernelArg(kern,5,sizeof(int),&numElsPerPage);
	if(err != CL_SUCCESS){
		printf("Error in setting the kernel argument 4\n");
		return EXIT_FAILURE;
	}

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

	err = clFinish(cQ);
	if(err!=CL_SUCCESS){
		printf("Error in blocking previously queued commands \t Error: %d\n",err);
		return EXIT_FAILURE;
	}
	////////////////////////////////////////warm up end////////////////////////////////////////////////////////
	
	FILE *fp1,*fp2,*fp3;
	double averageTime[numPages][numRnd];
	double rateOfChngBuff[numPages];
	memset(averageTime,0.0,numPages*numRnd*sizeof(double));
	memset(averageTime,0.0,numPages*sizeof(double));
	
	fp1 = fopen("first_access_1","w+");
	fp2 = fopen("second_access_1","w+");
	fp3 = fopen("access_diff_1","w+");
	
	while(rndIdxLimit<1024){//	numPages
	//while(rndIdxLimit<numEl768KB){	
	
		err = clSetKernelArg(kern,3,sizeof(int),&rndIdxLimit);
		if(err != CL_SUCCESS){
			printf("Error in setting the kernel argument 3\n");
			return EXIT_FAILURE;
		}


		for(int i = 0;i<numRnd;i++){

			int currRnd = i+1;		
			err = clSetKernelArg(kern,4,sizeof(int),&currRnd);
			if(err != CL_SUCCESS){
				printf("Error in setting the kernel argument 4\n");
				return EXIT_FAILURE;
			}
	
			for(int k = 0;k<numRun;k++){

	/*			for(int i = 0; i < rndIdxLimit ;i++ ){*/
	/*				//printf("Index #: %d \t %p\n",ptChaseRandIndxBuff[i],(dataHost + numElsPerPage*ptChaseRandIndxBuff[i]));*/
	/*				flush((void *)(dataHost + numElsPerPage*ptChaseRandIndxBuff[i]) );*/
	/*			}*/
		
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

				err = clFinish(cQ);
				if(err!=CL_SUCCESS){
					printf("Error in blocking previously queued commands \t Error: %d\n",err);
					return EXIT_FAILURE;
				}
	
	
				clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(tStart), &tStart, NULL);
				clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(tEnd), &tEnd, NULL);

	/*			double nanoSeconds = tEnd-tStart;*/
	/*			printf("time : %0.6f us \n",nanoSeconds / 1000.0);*/

				execTimePerRnd[i][k] = (double)((tEnd-tStart)/ 1000.0);
		
			}
		}
	
	
		for(int i = 0;i<numRnd;i++){

			double tempTime = 0;
			for(int k = 0;k<numRun;k++)
				tempTime+=execTimePerRnd[i][k];
			
			averageTime[rndIdxLimit-1][i] = (double)(tempTime/numRun);
		
		}
	
		fprintf(fp1,"%0.3f \t",averageTime[rndIdxLimit-1][0]);
		fprintf(fp2,"%0.3f \t",averageTime[rndIdxLimit-1][1]);
		fprintf(fp3,"%0.3f \t",averageTime[rndIdxLimit-1][1] - averageTime[rndIdxLimit-1][0]);
		rateOfChngBuff[rndIdxLimit-1] = averageTime[rndIdxLimit-1][1] - averageTime[rndIdxLimit-1][0];
		
		printf("%d => average access time #1: %0.3f \t average access time #2: %0.3f\t cache difference: %0.4f\n",rndIdxLimit,averageTime[rndIdxLimit-1][0],averageTime[rndIdxLimit-1][1],averageTime[rndIdxLimit-1][1] - averageTime[rndIdxLimit-1][0]);
		
		rndIdxLimit++;
	}
	
	
	FILE *fp4;
	fp4 = fopen("rateofchange","w");
	
	for(int k =1;k<numPages;k++){
		fprintf(fp4,"%0.3f \t",rateOfChngBuff[k]- rateOfChngBuff[k-1]);
	}
		
	fclose(fp1);
	fclose(fp2);
	fclose(fp3);
	fclose(fp4);
	
	clReleaseKernel(kern);
	clReleaseProgram(program);
	clReleaseCommandQueue(cQ);
	clReleaseContext(ctx);
}



