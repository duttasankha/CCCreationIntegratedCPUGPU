
 typedef int type_1;

__kernel void CCGPUKernel(__global type_1 *inBuff,__global type_1* outBuff,int loopNums){

	int gTid = get_global_id(0);
	size_t NDRange = get_global_size(0);
	
	//int start  = clock();

	for(int i=0;i<loopNums;i++)
	  outBuff[NDRange*i + gTid] = inBuff[NDRange*i + gTid]*100;
}

