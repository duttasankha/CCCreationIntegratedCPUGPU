#include "CC_ALL_HEADER.h" 
/*
using namespace MetricsDiscovery;
using namespace std;

CloseMetricsDevice_fn  CloseMetricsDevice;
OpenMetricsDevice_fn   OpenMetricsDevice;
IMetricsDevice_1_5*    m_MetricsDevice;

extern bool InitMetrics();
bool ActivateMetricSet(IMetricSet_1_1* m_MetricSet);
void DeactivateMetricSet(IMetricSet_1_1* m_MetricSet);
void GetMetricsFromReport(IMetricSet_1_1* m_MetricSet,const char* pReportData,std::vector<TTypedValue_1_0>& results,std::vector<TTypedValue_1_0>& maxValues);
void PrintMetricNames(IMetricSet_1_1* m_MetricSet , std::ostream& os );
void PrintMetricValues(	IMetricSet_1_1* m_MetricSet,std::ostream& os,const std::string& name,const std::vector<TTypedValue_1_0>& results,
			const std::vector<TTypedValue_1_0>& maxValues );
void PrintValue( std::ostream& os, const TTypedValue_1_0& value );
TTypedValue_1_0* GetGlobalSymbolValue( const char* SymbolName ,IMetricsDevice_1_5* m_MetricsDevice);
*/

//void printMetricNameWrapper(IMetricSet_1_1* m_MetricSet);
//void PrintMetricUnitsWrapper(IMetricSet_1_1* m_MetricSet);

bool initMetrics(IMetricSet_1_1* m_MetricSet,IMetricsDevice_1_5* m_MetricsDevice);

int main(int argc,char* argv[]){

	IMetricsDevice_1_5* m_MetricsDevice;	
	IMetricSet_1_1* m_MetricSet;
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

	printf("Enlisting Intel integrated GPU details\n");

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

	fpHandle = fopen("CC_GPU_KERNEL.cl","r");
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

	cl_kernel kern = clCreateKernel(program,"CCGPUKernel",&err);
	if(err!=CL_SUCCESS){
		printf("Error in kernel creation");
		return EXIT_FAILURE;
	}

	cl_command_queue cQ = clCreateCommandQueue(ctx,devices[0],CL_QUEUE_PROFILING_ENABLE,&err);
	if(err!=CL_SUCCESS){
		printf("Error in creating the command queue\n");
		return EXIT_FAILURE;
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////
	             /////////////STARTING THE OPENCL PROG RELATED DECLARATION//////////////////////
	
	size_t dataBuffSize = MB(8);// arrSize*sizeof(type);
	int numEl = dataBuffSize/sizeof(type_1);
	
	//initMetrics(m_MetricSet,m_MetricsDevice);
	//printMetricNameWrapper(m_MetricSet);
	//PrintMetricUnitsWrapper(m_MetricSet);
	//PrintMetricNames(m_MetricSet , std::cout );
	//PrintMetricUnits(m_MetricSet , std::cout);

	printf("Number of elements: %d\n",numEl);

	printf("Allocating the host side buffer\n");
	type_1* hostDataBuff,*resBuff;

	hostDataBuff = (type_1 *)malloc(dataBuffSize);
	if(!hostDataBuff){
	   printf("Out of host memory for the data buffer\n");
	   return EXIT_FAILURE;
	}

	resBuff = (type_1 *)malloc(dataBuffSize);
	if(!resBuff){
	   printf("Out of host memory for the data buffer\n");
	   return EXIT_FAILURE;
	}

	for(int i =0;i<numEl;i++)
		hostDataBuff[i] = i;

	cl_mem inBuff = clCreateBuffer(ctx,CL_MEM_READ_WRITE,dataBuffSize,NULL, &err);
	if(err != CL_SUCCESS){
	   printf("Error in allocatinga data device buffer\n");
	   return EXIT_FAILURE;
	}

	printf("Transferring buffer from host to device\n");
	err = clEnqueueWriteBuffer(cQ,inBuff,CL_TRUE,0,dataBuffSize,hostDataBuff,0,NULL,NULL);
	if(err != CL_SUCCESS){
	   printf("Error in transferring data from host to device\n");
	   return EXIT_FAILURE;
	}

	cl_mem outBuff = clCreateBuffer(ctx,CL_MEM_READ_WRITE,dataBuffSize,NULL, &err);
	if(err != CL_SUCCESS){
	   printf("Error in allocatinga data device buffer\n");
	   return EXIT_FAILURE;
	}

	err = clSetKernelArg(kern,0,sizeof(cl_mem),&inBuff);
	if(err != CL_SUCCESS){
	   printf("Error in setting the kernel argument 0\n");
	   return EXIT_FAILURE;
	}

	err = clSetKernelArg(kern,1,sizeof(cl_mem),&outBuff);
	if(err != CL_SUCCESS){
	   printf("Error in setting the kernel argument 1\n");
	   return EXIT_FAILURE;
	}

	int ctr =1;
	int numLoops;
	cl_event event;

	#define WGSize 32

	while(1){

	  	printf("Ctr #%d\t",ctr);

		numLoops = numEl/(ctr*WGSize);

		printf("Number of loops: %d\n",numLoops);

		err = clSetKernelArg(kern,2,sizeof(int),&numLoops);
		if(err != CL_SUCCESS){
	   	  	printf("Error in setting the kernel argument 1\n");
	   		return EXIT_FAILURE;
		}

		printf("Launching the kernel\n");
		size_t globalSize = ctr*WGSize;
		size_t localSize = WGSize;

		printf("globalSize: %d \t localSize: %d\n",globalSize,localSize);

		err = clEnqueueNDRangeKernel(cQ,kern,1,NULL,&globalSize,&localSize,0,NULL,&event);
		if(err!=CL_SUCCESS){
	   		printf("Unable to launch a kernel \t Error: %d\n",err);
	   		return EXIT_FAILURE;
		}

		clWaitForEvents(1, &event);
		clFinish(cQ);
	
		cl_ulong time_start;
		cl_ulong time_end;

		clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, NULL);
		clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(time_end), &time_end, NULL);

		double nanoSeconds = time_end-time_start;
		printf("OpenCl Execution time is: %0.3f milliseconds \n\n",nanoSeconds / 1000000.0);

	
 	        if(numLoops==1)
		  break;

		ctr*=2;
	}

	err = clEnqueueReadBuffer(cQ,outBuff,CL_TRUE,0,dataBuffSize,resBuff,0,NULL,NULL);	
	if(err != CL_SUCCESS){
	  printf("Error in copying host buffer from device to host\n");
	  return EXIT_FAILURE;
	}

        printf("Printing the host resultant buffer\nData\n");
        for(int i =0;i<100;i++)
	   printf("%d: %d \n",i,resBuff[i]);
        
	printf("Kernel execution is over\n");
//releaseMemObject:
	clReleaseMemObject(inBuff);
	clReleaseMemObject(outBuff);
	free(hostDataBuff);
//releaseCommandQueue:
	clReleaseCommandQueue(cQ);
//releaseKernel:
	clReleaseKernel(kern);
//releaseProgram:
	clReleaseProgram(program);
//releaseContext:
	clReleaseContext(ctx);

	printf("Exiting the GPU function\n");

	return 0;
}
