#include "PA_COV_CHANN.h"

int main(int argc,char*argv[]){

  	set_affinity(0);  
 
  	CSList *gpuEvictSet,*cpuEvictSet,*temp;
  	datatype *gpuBuff,*cpuBuff;
	int samectr;

  	init_pagemap();
  
/////////////////////////////////CPU related works///////////////////////////////////////////////

	////////////CPU buffer creation and initialization////////////
 	 cpuBuff = cpuDataBuffInit();
 	////////////CPU eviction set creation////////////
  	 cpuEvictSet = evictSetCreateFunc(cpuBuff);
 	 temp = cpuEvictSet;
 	 
	 samectr=0;
 	 while(temp){
		//printf("Detected set number: %d\n",samectr);
		multiEvictSetDetFunc(temp);
		samectr++;
		//printf("///////////////////////////////////////////////////////////////////////////\n");
		temp = temp->next;
	 }
	 
/////////////////////////////////GPU related works///////////////////////////////////////////////   
	
	////////////GPU buffer creation and initialization////////////
	gpuPlatInitFunc();
 	////////////GPU buffer creation and initialization////////////
	gpuBuff = gpuDataBuffInit();

	////////////GPU eviction set creation////////////
  	gpuEvictSet = evictSetCreateFunc(gpuBuff);
 	temp = gpuEvictSet;
 	
	samectr=0;
 	while(temp){
		//printf("Detected set number: %d\n",samectr);
		multiEvictSetDetFunc(temp);
		samectr++;
		//printf("///////////////////////////////////////////////////////////////////////////\n");
		temp = temp->next;
	}

//////////////////////////////Building up the communication//////////////////////////////////////

///////////Matching cache set//////////
	CSList *cpuTemp,*gpuTemp;
	#define cacheSetArrIndHeight 33
	cpuTemp = cpuEvictSet;
	int cpuGpuCacheSetMapArr[cacheSetArrIndHeight][2];

	int aa =0;

	memset(cpuGpuCacheSetMapArr,0,cacheSetArrIndHeight*2*sizeof(int));
	while(cpuTemp){
		
		gpuTemp = gpuEvictSet;
		int bb=0;
		cpuGpuCacheSetMapArr[aa][0] = aa;

		while(gpuTemp){
			
			if(cpuTemp->cacheSet == gpuTemp->cacheSet)
				cpuGpuCacheSetMapArr[aa][1] = bb;
				
			gpuTemp = gpuTemp->next;
			bb++;
		}
		
		cpuTemp = cpuTemp->next;
		aa++;
	}
	
///////////Matching cache set done//////////

//Testing the communication over 1 cache set. The same cache set is determined in the above statements. Now just using 1 CPU cache set and it's corresponding GPU cache set. This would be extended to all later if reqd.

	gpuTemp = gpuEvictSet;
	cpuTemp = cpuEvictSet;
	int totNumDiffInd;
	
	printf("Before call: %d\n",cpuGpuCacheSetMapArr[0][1]);
	printf("Entering the non-cache set index buffer determination function\n");
	int *diffCacheSetIndBuff = gpuNonCacheSetIndDetFunc(gpuBuff,gpuEvictSet,cpuGpuCacheSetMapArr[0][1],&totNumDiffInd);
	printf("Exiting the function\n");
	printf("totNumDiffInd: %d\n",totNumDiffInd);	
	printf("Entering GPU L3 evict set determination\n");
	printf("START \n");
	int gpuL3EvictRangeVar = gpuL3EvictRangeDetFunc(gpuBuff,diffCacheSetIndBuff,totNumDiffInd);
	printf("Exiting GPU L3 evict set determination\n");
	
	pThArgsSt *cpuArg, * gpuArg;
	pthread_t threadCPU,threadGPU;
	
	for(int i=0;i<cpuGpuCacheSetMapArr[0][1];i++)
		gpuTemp = gpuTemp->next;
	
	/////////CPU pthread arguments/////
	cpuArg = (pThArgsSt *)malloc(sizeof(pThArgsSt));
	if(!cpuArg){
		printf("Error in allocating cpu argument memory\n");
		return EXIT_FAILURE;
	}

	float dummyRet=0;
	volatile int *syncVarCPU = (int *)malloc(sizeof(int));
	*syncVarCPU  = 1;
	
	cpuArg->evictSetArg = cpuTemp;
	cpuArg->buffArg = cpuBuff;
	cpuArg->dummy = &dummyRet;
	cpuArg->chosenSlice = SLICEID;
	cpuArg->syncVar = syncVarCPU;

	/////////GPU pthread arguments/////	
	gpuArg = (pThArgsSt *)malloc(sizeof(pThArgsSt));
	if(!gpuArg){
		printf("Error in allocating cpu argument memory\n");
		return EXIT_FAILURE;
	}

	gpuArg->evictSetArg = gpuTemp;
	gpuArg->buffArg = gpuBuff;
	gpuArg->dummy = &dummyRet;
	gpuArg->chosenSlice = SLICEID;
	
	err = pthread_create(&threadGPU,NULL,gpuSideSendFunc,(void *)gpuArg);
	if(err){
		printf("Error in creating pthread 1\n");
		return EXIT_FAILURE;
	}

	err = pthread_create(&threadCPU,NULL,cpuSideRecvFunc,(void *)cpuArg);
	if(err){
		printf("Error in creating pthread 1\n");
		return EXIT_FAILURE;
	}

	pthread_join(threadGPU,NULL);
	*syncVarCPU = 0;
	pthread_join(threadCPU,NULL);
/*	*/
/*	printf("%f\n",dummyRet);*/
	
/*	free(cpuArg);*/
/*	//free(gpuArg);*/
	
	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Doing the communication over same GPU and CPU cache set and also on the same slices that is determined within the main function

void* cpuSideRecvFunc(void *pthCPUArg){

	printf("Entering CPU side receive thread\n");
	
	pThArgsSt *cpuArg = (pThArgsSt *)pthCPUArg;

	CSList   *cpuEvictSetNode  = cpuArg->evictSetArg;
	datatype *cpuBuff = cpuArg->buffArg;
	int cpuChosenSlice = cpuArg->chosenSlice;
	volatile int *sync = cpuArg->syncVar;
	CSList	 *temp = cpuEvictSetNode;
	
	#define timeBuffSizeMB 1024
	size_t sizeTimeGB = timeBuffSizeMB*1024*1024;
	uint64_t *timeBuff = (uint64_t *)malloc(sizeTimeGB);
	
	//int numElTimeBuff = MB(timeBuffSizeMB)/sizeof(uint64_t));
	int sliceInd = 0;
	for(int i = 0;i<temp->totalSliceCtr;i++){
		if(temp->sliceID[i] == cpuChosenSlice){
			sliceInd = i;
			break;		
		}
	}
	
/*	for(int i =0;i<16;i++)*/
/*		printf("index: %d\n",temp->selectedIndex[sliceInd][i]);*/
		
	int dummy = 0;
	for(int i =0;i<16;i++) // 16 due to cache waystemp->sameSliceCtr[sliceInd]
		dummy +=cpuBuff[temp->selectedIndex[sliceInd][i]];

	uint64_t start,end;
	usleep(5);
	
	int w = 0;
	printf("here\n");
	while(*sync){
		
		start = rdtsc();
		
		for(int i =0;i<16;i++) // 16 due to cache waystemp->sameSliceCtr[sliceInd]
			dummy +=cpuBuff[temp->selectedIndex[sliceInd][i]];
		end = rdtsc() - start;
		timeBuff[w] = end;		
		w++;
	}
	
	FILE *fp;
	fp = fopen("time.txt","w");
	
	if(!fp){
		printf("Error in opening file");
		exit(-1);
	}
	
	for(int i = 0;i<w;i++)
		fprintf(fp,"%"PRId64"\t",timeBuff[i]);

	fclose(fp);
	
	printf("\nExiting CPU side receive thread\n");
}

void *gpuSideSendFunc(void *pthGPUArg){

	printf("Entering GPU side sending thread\n");
	
	pThArgsSt *gpuArg = (pThArgsSt *)pthGPUArg;

	CSList   *gpuEvictSetNode = gpuArg->evictSetArg;
	datatype *gpuBuff = gpuArg->buffArg;
	float	 *retDummy = gpuArg->dummy;
	int gpuChosenSlice = gpuArg->chosenSlice;//The chosen slice send from main
	CSList *temp = gpuEvictSetNode; // working on only one GPU set mapping first; would be extended to all the sets

	int sliceInd = 0;

	for(int i = 0;i<gpuEvictSetNode->totalSliceCtr;i++){
		if(gpuEvictSetNode->sliceID[i] == gpuChosenSlice){ //Matching the chosen slice
			sliceInd = i;
			break;		
		}
	}

	//Allocating device side of the main buffer and mapping it to the host buffer for zero copy	
	size_t buffSz = MB(buffInMBSize);
	
	cl_mem buffDev = clCreateBuffer(ctx,CL_MEM_READ_WRITE|CL_MEM_USE_HOST_PTR,buffSz,gpuBuff,&err);
	if(err!=CL_SUCCESS){
	   printf("Err in creating device buffer : %d\n",err);
	   return EXIT_FAILURE;
	}

	//mapping of the device buffer	
	gpuBuff = (datatype *)clEnqueueMapBuffer(cQ,buffDev,CL_TRUE,CL_MAP_READ|CL_MAP_WRITE,0,buffSz,0,NULL,NULL,&err);
        if(err!=CL_SUCCESS){
	 printf("Error in mapping the host buffer: %d\n",err);
	 return EXIT_FAILURE;
        }
	
	////////////////////preparing to create device buffer to hold the indices that belong to the desired slice and copying to the device////////////////////
	
	int localsameslcctr= gpuEvictSetNode->sameSliceCtr[sliceInd];
	//printf("%d\tlocalsameslcctr: %d\n",i,localsameslcctr);
	size_t sameSliceSz = localsameslcctr*sizeof(int);

	int *sameSliceInd = (int *) malloc(sameSliceSz);
	if(!sameSliceInd){
		printf("Error in allocating host side same slice Index\n");
		return EXIT_FAILURE;
	}
		
	for(int j=0;j<localsameslcctr;j++)
		sameSliceInd[j] = gpuEvictSetNode->selectedIndex[sliceInd][j];

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
	
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		
	err = clSetKernelArg(kern,0,sizeof(cl_mem),&buffDev);
	if(err != CL_SUCCESS){
		printf("Error in setting the kernel argument 0\n");
		return EXIT_FAILURE;
	}
		
	err = clSetKernelArg(kern,1,sizeof(cl_mem),&sameSliceIndDev);
	if(err != CL_SUCCESS){
		printf("Error in setting the kernel argument 2\n");
		return EXIT_FAILURE;
	}
		
	err = clSetKernelArg(kern,2,sizeof(int),&localsameslcctr);
	if(err != CL_SUCCESS){
		printf("Error in setting the kernel argument 2: %d\n",err);
		return EXIT_FAILURE;
	}
	
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/////pthread related stuff
	
	pthread_t pth;
	volatile int *gPthSync = (int *)malloc(sizeof(int));
	*gPthSync = 1;
	
	gPthSt *gpuPThrArg = (gPthSt *)malloc(sizeof(gPthSt));
	
	gpuPThrArg->gpuPthBuff = gpuBuff;
	gpuPThrArg->gpuChosenInd = sameSliceInd;
	gpuPThrArg->numInd = localsameslcctr;
	gpuPThrArg->gpuSync = gPthSync;
	
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
	size_t globalSize = 1;//4096+32;	
	size_t localSize = 1;//kern_pref_wgsize_mult*2;//

	err = pthread_create(&pth,NULL,pThGPUFlushFunc,(void *)gpuPThrArg);//(void *)arg
	if(err){
		printf("Error in creating pthread 1\n");
		return EXIT_FAILURE;
	}
	
	//while(1){
	usleep(1);
	
	printf("reached here\n");
	
	for(int j=0;j<numRndMessage;j++){
		
		printf("Message round send #: %d\n",j);
		err = clEnqueueNDRangeKernel(cQ,kern,1,NULL,&globalSize,&localSize,0,NULL,NULL);
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

		
		usleep(10);
	}
		
/*		usleep(500);*/
/*	}*/

	*gPthSync = 0;

	pthread_join(pth,NULL);

	free(sameSliceInd);
	err = clReleaseMemObject(sameSliceIndDev);
	if(err!=CL_SUCCESS){
		printf("Err in releasing device index buffer\n");
		return EXIT_FAILURE;
	}
		
//	}
	
	printf("Exiting GPU side sending thread\n");
}

void* pThGPUFlushFunc(void *arg){
	
	printf("Entering the flush pthread\n");
	
	gPthSt *gpuPThrArg = (gPthSt *)arg;
	
	datatype *gpuBuff = gpuPThrArg->gpuPthBuff;
	int* sameSliceInd = gpuPThrArg->gpuChosenInd;
	int localsameslcctr = gpuPThrArg->numInd;
	volatile int * gPthSync = gpuPThrArg->gpuSync;

	printf("flush pthread: %d\n",localsameslcctr);

	while(*gPthSync){
		for(int i =0; i< localsameslcctr;i++){
			flush((void *)(gpuBuff+sameSliceInd[i]));
		}
	}
	
	printf("Exiting the flush pthread\n");	
}




