
__kernel void reKernel(__global int *dataBuff,__global unsigned int *indexBuff, int numPages,int thisRndIdxLimit,int numRnd){
	
	int dummy = 100;
	
	for(int m = 0; m < numRnd; m++){
		for(int i = 0;i < thisRndIdxLimit;i++){
			dummy +=dataBuff[indexBuff[i]];
		}
	}
	
	dataBuff[0] = dummy;
}
	

