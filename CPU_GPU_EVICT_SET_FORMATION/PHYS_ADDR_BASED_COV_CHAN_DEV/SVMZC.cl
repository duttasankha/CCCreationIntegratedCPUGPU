
#define eachRndNumMessage 100000
#define cacheClearNumRnds 80000
#define mainDataAccessNumThread 32

__kernel void svmZcKern(__global int *mainBuff,__global int* indexBuff,int sameSliceCtr){//__global int *cacheClearBuff,

	int dummy_1 = 0;
	int dummy_2 = 0;

	for(int i=0;i<eachRndNumMessage;i++){
		
		for(int j=0;j<sameSliceCtr;j++)
			dummy_1 +=mainBuff[indexBuff[j]];

		mem_fence(CLK_GLOBAL_MEM_FENCE);
			
		dummy_2 += dummy_1-i;
		dummy_1 = 0;
	}
	
	mainBuff[indexBuff[0]] += (mainBuff[indexBuff[0]]-dummy_2+dummy_1);//dummy_2// - dummyVar2);//dummyVar2;//abs	
	barrier(CLK_GLOBAL_MEM_FENCE);	
	
}


__kernel void gpuL3RE(__global int *dataBuff,__global int *indexBuff,int thisRndIdxLimit,int numRnd,int numElsPerPage){
	
	int dummy = 100;
	
	for(int m = 0; m < numRnd; m++)
		for(int i = 0;i < thisRndIdxLimit;i++)
			dummy +=*(dataBuff + numElsPerPage*indexBuff[i]);

	dataBuff[0] = dummy;
}
	

//	int dummyVar1=0,dummyVar2=0;
//	
//	printf("sameSliceCtr: %d\n",sameSliceCtr);
//	int thglobID = 	get_global_id(0);
//	
//	int restNumTh = totalNumThr - mainDataAccessNumThread;
//	
//	volatile __local bool globalBreak,eachRndBreak;
//	printf("Thread %d\n restNumTh: %d\n",thglobID,restNumTh);
//	
//	volatile __local int syncVarMemtoCtr[2];
//   	volatile __local int syncVarCtrtoMem[2];
//	
//	if(thglobID==0){
//		globalBreak = false;
//		eachRndBreak = false;
//	}

//	if(thglobID==0){
//	    syncVarMemtoCtr[0] = 0;
//	    syncVarMemtoCtr[1] = 0;
//	    
//	    syncVarCtrtoMem[0] = 0;
//	    syncVarCtrtoMem[1] = 0;
//		
//	}
//	
//	barrier(CLK_LOCAL_MEM_FENCE);
//	
//	if(thglobID<mainDataAccessNumThread){
//		
//		for(int i=0;i<eachRndNumMessage;i++){
//			
//			for(int j=0;j<sameSliceCtr;j++)
//				dummy_1 +=mainBuff[indexBuff[j]];

//			mem_fence(CLK_GLOBAL_MEM_FENCE);
//				
//			dummy_2 += dummy_1-i;
//			dummy_1 = 0;
//			
//			syncVarMemtoCtr[0] =1;
//			mem_fence(CLK_LOCAL_MEM_FENCE);
//      
//			while(1){
//				if(syncVarCtrtoMem[0]==1)
//					break;
//			}

//			eachRndBreak = true;
//			mem_fence(CLK_LOCAL_MEM_FENCE);
//			printf("thread : %d \t if part waiting",thglobID);
//			while(eachRndBreak);
//		}
//		
//		syncVarMemtoCtr[0] =1;
//		globalBreak = true;
//		mem_fence(CLK_LOCAL_MEM_FENCE);		
//	}
//	else
//	{
//		int shiftedThreadID = thglobID - mainDataAccessNumThread;
//		
//		for(int j=0;j<cacheClearNumRnds;j++){while(1){
//			
//			dummyVar1 = 0;
//			while(!eachRndBreak);
//			while(1){
//				if(syncVarMemtoCtr[0]==1)
//				break;
//			}
//			
//			for(int i =0;i<cacheClrBuffNumEls;i+=restNumTh)
//				dummyVar1+=cacheClearBuff[i*restNumTh + shiftedThreadID];

//			mem_fence(CLK_GLOBAL_MEM_FENCE);
//			
//			dummyVar2+=dummyVar1;			
//			eachRndBreak = false;
//			mem_fence(CLK_LOCAL_MEM_FENCE);
//			if(syncVarMemtoCtr[0]==1)if(globalBreak)
//				break;
//		}
//		
//					
//	}
