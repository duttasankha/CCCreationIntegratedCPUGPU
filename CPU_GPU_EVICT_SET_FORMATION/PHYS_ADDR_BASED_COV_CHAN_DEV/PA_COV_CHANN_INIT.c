
#include "PA_COV_CHANN.h"

datatype * cpuDataBuffInit(){

  datatype *buff;
  size_t buffSize = MB(buffInMBSize);

  //buff = (dataType *)malloc(buffSize);
  int err = posix_memalign((void **)&buff,4096,buffSize); //allocating page aligned main buffer
  if(err!=0){
	printf("Error in memory allocation: %d\n",err);
	exit(EXIT_FAILURE);
  }

  int numEl = buffSize/sizeof(datatype);
 
  printf("Buffer Size: %d\n",buffInMBSize);
  printf("Number of elements: %d\n",numEl);
  printf("Buff start addr: %p\n",buff);

  for(int i=0;i<numEl;i++)
    buff[i] = i;

  return buff;
}

//////////////////////////////////GPU related declarations//////////////////////////////////

void gpuPlatInitFunc(){

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

	fpHandleGPU = fopen("SVMZC.cl","r");
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

	kern = clCreateKernel(program,"svmZcKern",&err);
	if(err!=CL_SUCCESS){
		printf("Error in kernel creation");
		exit(EXIT_FAILURE);
	}

	kernRE = clCreateKernel(program,"gpuL3RE",&err);
	if(err!=CL_SUCCESS){
		printf("Error in kernel creation");
		exit(EXIT_FAILURE);
	}

	cQ = clCreateCommandQueue(ctx,devices[0],CL_QUEUE_PROFILING_ENABLE,&err);
	if(err!=CL_SUCCESS){
		printf("Error in creating the command queue\n");
		exit(EXIT_FAILURE);
	}

}

datatype *gpuDataBuffInit(){

 	datatype *buffGPU;
	size_t buffSizeGPU;

  	buffSizeGPU = MB(buffInMBSize);  //determining buffer 1 size 
	int numElGPU = buffSizeGPU/sizeof(datatype); //Number of elements in buffer 1
	 
	printf("Number of elements (GPU): %d\n",numElGPU);
	printf("Size of GPU buffer: %lu\n",buffSizeGPU);

 	buffGPU = (datatype *)clSVMAlloc(ctx,CL_MEM_READ_WRITE,buffSizeGPU,2); //Allocating SVM buffer 1
	if(!buffGPU){
	   printf("Error in allocating buffer SVM buffer 1\n");
	   exit(EXIT_FAILURE);
	}

	printf("GPU buffer address (host) : %p\n",buffGPU);
	
	//Mapping the SVM buffer 1 to populate the buffer 
	printf("Initializing GPU buffer\n");
	err = clEnqueueSVMMap(cQ,CL_TRUE,CL_MAP_WRITE,buffGPU,buffSizeGPU,0,NULL,NULL);
	if(err!=CL_SUCCESS){
	   printf("Err in SVM map\n");
	   exit(EXIT_FAILURE);
	}
	
	for(int i =0;i<numElGPU;i++)
	 buffGPU[i] = i;
	
	err = clEnqueueSVMUnmap(cQ,buffGPU,0,NULL,NULL);
	if(err!=CL_SUCCESS){
	   printf("Err in SVM unmap\n");
	   exit(EXIT_FAILURE);
	}

	return buffGPU;
}
