
__kernel void reKernel(__global int *dataBuff,__global unsigned int *indexBuff, int numPages,int thisRndIdxLimit,int numRnd,int numElsPerPage){
	
	int dummy = 100;
	/*for(int i = 0;i < thisRndIdxLimit;i++){
			printf("%p ",dataBuff + numElsPerPage*indexBuff[i]);
	}*/
	
	for(int m = 0; m < numRnd; m++){
		for(int i = 0;i < thisRndIdxLimit;i++){
			dummy +=*(dataBuff + numElsPerPage*indexBuff[i]);
			//dummy +=dataBuff[indexBuff[i]];
		}
	}
	
	//printf(" dummy: %d\n",dummy);
	dataBuff[0] = dummy;
}
	

