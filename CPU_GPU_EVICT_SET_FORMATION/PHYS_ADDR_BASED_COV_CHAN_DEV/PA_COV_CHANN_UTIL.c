#include "PA_COV_CHANN.h"

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


CSList * evictSetCreateFunc(datatype *buff){

	CSList *head,*temp;
	head = NULL;//list[0];
	int totalUniqueSet = 0;
	int numPages = MB(buffInMBSize)/KB(pageSize);
	size_t numElsPerPage = (KB(pageSize)/sizeof(datatype));

	printf("Number of pages: %d\n",numPages);

	for(int i=0;i<numPages;i++){

     		uint64_t VA= (uint64_t)(buff+ numElsPerPage*i);
     		uint64_t PA = get_physical_addr(VA);
     		uint64_t CacheSet = (uint64_t)((PA>>6)&0x00000000000007ff);//(PA&((uint64_t) 1 << 17) - 1);//
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
	
 	return head;
}


void multiEvictSetDetFunc(CSList *temp){

  int breakCtr = temp->addrInSetCtr;
  
  /////////////////////////////////////////////////////////////////////////////////

  temp->selectedPA = (uint64_t **)malloc(uniqueSameSlicePerCacheSet*sizeof(uint64_t *)); 
  temp->selectedVA = (uint64_t **)malloc(uniqueSameSlicePerCacheSet*sizeof(uint64_t *));   
  temp->selectedIndex = (int   **)malloc(uniqueSameSlicePerCacheSet*sizeof(int *));   
  temp->totalSliceCtr = 0;

  int **sameIndex = (int **)malloc(uniqueSameSlicePerCacheSet*sizeof(int *));   

  uint64_t localPA[breakCtr],localVA[breakCtr];
  int localInd[breakCtr];

  memcpy(localPA,temp->PA,breakCtr*sizeof(uint64_t));
  memcpy(localVA,temp->VA,breakCtr*sizeof(uint64_t));
  memcpy(localInd,temp->sameSetIndex,breakCtr*sizeof(int));
  memset(temp->sameSliceCtr,0,uniqueSameSlicePerCacheSet*sizeof(int));    

  int initTargetIndex;// = 0;
  int allAddrCtr = breakCtr;
  int count = 0;
  
  while(allAddrCtr>10){
     
	for(int n=0;n<breakCtr;n++){
		if(localPA[n]!=0){  
			initTargetIndex = n;
			break;
		}
	}
	
	temp->sliceID[count] = get_cache_slice(localPA[initTargetIndex],1);
	
	if(count!=0){

		for(int i=0;i<breakCtr;i++){

			int found = 0;
			
			for(int j = 0;j<count;j++){
				for(int k = 0;k<temp->sameSliceCtr[j];k++)
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
	allAddrCtr=allAddrCtr - temp->sameSliceCtr[count];
	count++;

  }
 
  temp->totalSliceCtr = count;

}

int* gpuNonCacheSetIndDetFunc(datatype *gpubuff,CSList *gpuEvictSet,int nodeInd,int *retIndCnt){
	
	printf("Node index arg val: %d\n",nodeInd);
	int totalIndCnt;
	CSList *temp = gpuEvictSet;
	CSList *temp1 = gpuEvictSet;
	totalIndCnt = 0;
	FILE *fp;
	
	fp = fopen("testOutput","w+");
	for(int i=0;i<nodeInd;i++)
		temp1 = temp1->next;
	
	while(temp){
		if(temp1->cacheSet!=temp->cacheSet){
			for(int i =0;i<temp->totalSliceCtr;i++)
				totalIndCnt+=temp->sameSliceCtr[i];
		}
		temp = temp->next;
	}
	
	int *retInd = (int *)malloc(totalIndCnt*sizeof(int));
	
	temp = gpuEvictSet;
	totalIndCnt = 0;
	while(temp){
		
		int totalIndCntPrev = totalIndCnt;
		
		if(temp1->cacheSet!=temp->cacheSet){
			
			for(int i =0;i<temp->totalSliceCtr;i++){
				for(int j=0;j<temp->sameSliceCtr[i];j++){
					retInd[totalIndCnt] = temp->selectedIndex[i][j];
					totalIndCnt++;
				}
			}
			
			fprintf(fp,"////////////////////////////////////////////////////////////////////////////////////////////\n");
			fprintf(fp,"cache set: %"PRIx64"\t addrInSetCtr: %d \t totalIndCntPrev: %d \t totalIndCnt: %d\n",temp->cacheSet,temp->addrInSetCtr,totalIndCntPrev,totalIndCnt);
			
			for(int i =totalIndCntPrev;i<totalIndCnt;i++)
				fprintf(fp,"%d\t%d\t%p\n",i,retInd[i],&gpubuff[retInd[i]]);
			
			
		}
		temp = temp->next;
	}
	
	fclose(fp);
	*retIndCnt = totalIndCnt;
	
	return retInd;
}

int gpuL3EvictRangeDetFunc(datatype *gpuBuff,int *diffCacheSetIndBuff,int totNumDiffInd){
	
	int rangeLimit = 0;
	//int locTotNumDiffInd = totNumDiffInd;
	
	cl_mem diffCacheSetIndBuffDev = clCreateBuffer(ctx,CL_MEM_READ_WRITE,totNumDiffInd*sizeof(int),NULL,&err);
	if(err!=CL_SUCCESS){
		printf("Err in creating device index buffer: %d\n",err);
		return EXIT_FAILURE;
	}
	
	err = clEnqueueWriteBuffer(cQ,diffCacheSetIndBuffDev,CL_TRUE,0,totNumDiffInd*sizeof(int),diffCacheSetIndBuff,0,NULL,NULL);
	if(err!=CL_SUCCESS){
		printf("Err in copying buffer from host to device\n");
		return EXIT_FAILURE;
	}
	
	size_t buffSz = MB(buffInMBSize);
	
	cl_mem gpuBuffDev = clCreateBuffer(ctx,CL_MEM_READ_WRITE|CL_MEM_USE_HOST_PTR,buffSz,gpuBuff,&err);
	if(err!=CL_SUCCESS){
	   printf("Err in creating device buffer : %d\n",err);
	   return EXIT_FAILURE;
	}

	//mapping of the device buffer	
	gpuBuff = (datatype *)clEnqueueMapBuffer(cQ,gpuBuffDev,CL_TRUE,CL_MAP_READ|CL_MAP_WRITE,0,buffSz,0,NULL,NULL,&err);
        if(err!=CL_SUCCESS){
	 printf("Error in mapping the host buffer: %d\n",err);
	 return EXIT_FAILURE;
        }
	
/*	cl_kernel kernRE = clCreateKernel(program,"gpuL3RE",&err);*/
/*	if(err!=CL_SUCCESS){*/
/*		printf("Error in kernel creation");*/
/*		exit(EXIT_FAILURE);*/
/*	}*/

	
	err = clSetKernelArg(kernRE,0,sizeof(cl_mem),(void *)&gpuBuffDev);
	if(err != CL_SUCCESS){
		printf("Error in setting the kernel argument 0\n");
		return EXIT_FAILURE;
	}
	
/*	err = clSetKernelArgSVMPointer (kern,0,(void *)dataHost);*/
/*	if(err != CL_SUCCESS){*/
/*		printf("Error in setting the kernel argument 0\n");*/
/*		return EXIT_FAILURE;*/
/*	}*/
	
	err = clSetKernelArg(kernRE,1,sizeof(cl_mem),(void *)&diffCacheSetIndBuffDev);
	if(err != CL_SUCCESS){
		printf("Error in setting the kernel argument 1\n");
		return EXIT_FAILURE;
	}

/*	err = clSetKernelArg(kernRE,2,sizeof(int),(void *)&locTotNumDiffInd);*/
/*	if(err != CL_SUCCESS){*/
/*		printf("Error in setting the kernel argument 2\n");*/
/*		return EXIT_FAILURE;*/
/*	}*/


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////Start of the range determination////////////////////////////////////////////////////////////////////////	

	int rndIdxLimit = 1;
	int numRnd = 2;
	int numElsPerPage = (KB(4)/sizeof(datatype));
	//printf("Here\n");
	
	//////////////////////////////////////////warm up ////////////////////////////////////////////////////
	
	err = clSetKernelArg(kernRE,2,sizeof(int),(void *)&rndIdxLimit);
	if(err != CL_SUCCESS){
		printf("Error in setting the kernel argument 2: %d\n",err);
		return EXIT_FAILURE;
	}

	err = clSetKernelArg(kernRE,3,sizeof(int),(void *)&numRnd);
	if(err != CL_SUCCESS){
		printf("Error in setting the kernel argument 3: %d\n",err);
		return EXIT_FAILURE;
	}

	err = clSetKernelArg(kernRE,4,sizeof(int),(void *)&numElsPerPage);
	if(err != CL_SUCCESS){
		printf("Error in setting the kernel argument 4\n");
		return EXIT_FAILURE;
	}


	size_t globalSize = 1;//4096+32;	
	size_t localSize = 1;//kern_pref_wgsize_mult*2;//
	cl_event event;
	cl_ulong tStart,tEnd;

	//testing the cache inclusiveness;
	#define numRun 100
	double execTimePerRnd[numRnd][numRun] ;
	

	err = clEnqueueNDRangeKernel(cQ,kernRE,1,NULL,&globalSize,&localSize,0,NULL,&event);
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
	double averageTime[totNumDiffInd][numRnd];
	double rateOfChngBuff[totNumDiffInd];
	memset(averageTime,0.0,totNumDiffInd*numRnd*sizeof(double));
	memset(averageTime,0.0,totNumDiffInd*sizeof(double));
	
	fp1 = fopen("CCUncached","w");
	fp2 = fopen("CCCached","w");
	fp3 = fopen("CachedUncachedDiff","w");
	
	while(rndIdxLimit<1024){//	numPages//totNumDiffInd
	//while(rndIdxLimit<numEl768KB){	
	
		err = clSetKernelArg(kernRE,2,sizeof(int),&rndIdxLimit);
		if(err != CL_SUCCESS){
			printf("Error in setting the kernel argument 2: %d\n",err);
			return EXIT_FAILURE;
		}


		for(int i = 0;i<numRnd;i++){

			int currRnd = i+1;

			//printf("It is this one\n");
			err = clSetKernelArg(kernRE,3,sizeof(int),(void *)&currRnd);
			if(err != CL_SUCCESS){
				printf("Error in setting the kernel argument 3: %d\n",err);
				return EXIT_FAILURE;
			}

			for(int k = 0;k<numRun;k++){

	/*			for(int i = 0; i < rndIdxLimit ;i++ ){*/
	/*				//printf("Index #: %d \t %p\n",ptChaseRandIndxBuff[i],(dataHost + numElsPerPage*ptChaseRandIndxBuff[i]));*/
	/*				flush((void *)(dataHost + numElsPerPage*ptChaseRandIndxBuff[i]) );*/
	/*			}*/
		
				err = clEnqueueNDRangeKernel(cQ,kernRE,1,NULL,&globalSize,&localSize,0,NULL,&event);
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
		
		//printf("%d => average access time #1: %0.3f \t average access time #2: %0.3f\t cache difference: %0.4f\n",rndIdxLimit,averageTime[rndIdxLimit-1][0],averageTime[rndIdxLimit-1][1],averageTime[rndIdxLimit-1][1] - averageTime[rndIdxLimit-1][0]);
		
		rndIdxLimit++;
	}
	
	
	FILE *fp4;
	fp4 = fopen("CCROC","w");
	
	for(int k =1;k<totNumDiffInd;k++){
		fprintf(fp4,"%0.3f \t",rateOfChngBuff[k]- rateOfChngBuff[k-1]);
	}
		
	fclose(fp1);
	fclose(fp2);
	fclose(fp3);
	fclose(fp4);
	
	err = clReleaseMemObject(gpuBuffDev);
	if(err!=CL_SUCCESS){
		printf("Error in releasing gpu device memory object in the L3 evict  func: %d\n",err);
	}
	clReleaseKernel(kernRE);
	return rangeLimit;	
	
}
