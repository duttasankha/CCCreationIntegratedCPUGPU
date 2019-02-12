

__kernel void testKern(__global int *buff,int numEls){//__global volatile int *syncVar,
	
	int dummy1 = 0;
	int dummy2 = 0;
	
//	int pp = 0;
//	
//	while(pp<5){
		
		for(int k =0;k<0xffff;k++){
			dummy1 = 0;
			for(int i = 0 ;i<numEls;i++){
				
				dummy1+=buff[i];	
			}
			dummy2 += dummy1;
			
			//if(!(k%0xff))
			//	printf("gpu: %d\n",k);
		}
		
//		printf("GPU: %d\n",pp);
//		pp++;
		
//		syncVar[0] = 0;
//		while(syncVar[0]!=0);
//		
//	}	

	buff[0] = dummy2;
	
}
