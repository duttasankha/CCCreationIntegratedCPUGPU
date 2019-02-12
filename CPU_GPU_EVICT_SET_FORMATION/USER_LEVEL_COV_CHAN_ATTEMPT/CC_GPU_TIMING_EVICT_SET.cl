
/*__kernel void CCGPUTimingKernLLC_RE(__global int *devBuff,__global long int *ctrVal,int buffLen,int warpSize){


   size_t lid = get_local_id(0);
   size_t lSize = get_local_size(0);

   volatile __local int syncVarMemtoCtr[2];
   volatile __local int syncVarCtrtoMem[2];

   __local long int ctrValLocal[2];

   if(lid==0){
    syncVarMemtoCtr[0] = 0;
    syncVarMemtoCtr[1] = 0;
    
    syncVarCtrtoMem[0] = 0;
    syncVarCtrtoMem[1] = 0;

    printf("local size: %lu \t warpSize: %d\n\n",lSize,warpSize);
   }

   barrier(CLK_LOCAL_MEM_FENCE);

   if(lid<(lSize-1)){
     
      int numLoops  = buffLen/warpSize;
      int temp;

      for(int i=0;i<numLoops;i++)
	temp +=  devBuff[(i*warpSize) + lid]; 

      mem_fence(CLK_GLOBAL_MEM_FENCE);
	      
      syncVarMemtoCtr[0] =1;
      //mem_fence(CLK_LOCAL_MEM_FENCE);
      
      while(1){
	if(syncVarCtrtoMem[0]==1)
	   break;
      }

      for(int i=0;i<numLoops;i++)
	temp +=  devBuff[(i*warpSize) + lid];     
      
      mem_fence(CLK_GLOBAL_MEM_FENCE);

      syncVarMemtoCtr[1] =1;
      //mem_fence(CLK_LOCAL_MEM_FENCE);
      
      devBuff[0] = temp/devBuff[0];
   }
   else{

     int ctr=0;
     while(1){
       ctr++;
       if(syncVarMemtoCtr[0]==1)
	break;
     }
     
     ctrValLocal[0] = ctr;		
     ctr = 0;
     
     syncVarCtrtoMem[0]=1;
     //mem_fence(CLK_LOCAL_MEM_FENCE);

     while(1){
       ctr++;
       if(syncVarMemtoCtr[1]==1)
	break;
     }
     ctrValLocal[1] = ctr;
     
     ctrVal[0] = ctrValLocal[0];
     ctrVal[1] = ctrValLocal[1];
     //mem_fence(CLK_GLOBAL_MEM_FENCE);
   }
   barrier(CLK_GLOBAL_MEM_FENCE);

}*/


__kernel void CCGPUEvictionSet(__global unsigned short *buffA,__global unsigned short *buffB,
			       __global unsigned int *index_buff,__global int* junkBuff,
		               int numPages){

   size_t lid = get_local_id(0);
   size_t lSize = get_local_size(0);

   volatile __local int syncVarMemtoCtr[2];
   volatile __local int syncVarCtrtoMem[2];

   __local long int ctrValLocal[2];

   if(lid<lSize-1){
      
       
	devBuff_1[lid];

   }
   else{

	
   }

}
