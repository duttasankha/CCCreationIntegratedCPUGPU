#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#define size 10

typedef struct test{
	int var_1;
	float var_2;
	struct test *next;	
}ts;

typedef struct pTest{
	ts *arg_1;
	int *arg_2;
	volatile int *argSync;
}pTs;

void* func_1(void *);
void* func_2(void *);

//static int commVar;
//pthread_mutex_t mutex;

int main(){

//////////////////////////////////////////////////////////////////////////////////////////
	
	int *k1 = (int *)malloc(size*sizeof(int));

	ts *temp1,*temp2;
	ts *head1 = NULL;

	for(int i =0 ;i<size;i++){

		if(!head1){
			head1 = (ts *)malloc(sizeof(ts));
			head1->var_1 = i;
			head1->var_2 = (float)(i/2);
			head1->next = NULL;						
		}
		else{
			temp1 = head1;
			while(temp1){
				temp2 = temp1;
				temp1 = temp1->next;
			}
			temp1 = (ts *)malloc(sizeof(ts));
			temp1->var_1 = i;
			temp1->var_2 = (float)(i/2);
			temp1->next = NULL;
			temp2->next = temp1;
		}

		k1[i] = i*10;
	}

/*	temp1 = head1;*/

/*	while(temp1){*/
/*		printf("%d \t %f\n",temp1->var_1,temp1->var_2);*/
/*		temp1 = temp1->next;*/
/*	}*/

//////////////////////////////////////////////////////////////////////////////////////////

	int *k2 = (int *)malloc(size*sizeof(int));

	ts *head2 = NULL;

	for(int i =0 ;i<size;i++){

		if(!head2){
			head2 = (ts *)malloc(sizeof(ts));
			head2->var_1 = size - i;
			head2->var_2 = (float)((size - i)/2);
			head2->next = NULL;						
		}
		else{
			temp1 = head2;
			while(temp1){
				temp2 = temp1;
				temp1 = temp1->next;
			}
			temp1 = (ts *)malloc(sizeof(ts));
			temp1->var_1 = size - i;
			temp1->var_2 = (float)(size - i/2);
			temp1->next = NULL;
			temp2->next = temp1;
		}

		k2[i] = i*100;
	}

/*	printf("\n\n");*/
/*	temp1 = head2;*/

/*	while(temp1){*/
/*		printf("%d \t %f\n",temp1->var_1,temp1->var_2);*/
/*		temp1 = temp1->next;*/
/*	}*/

/*	printf("\n\n");*/

//////////////////////////////////////////////////////////////////////////////////////////
	
	volatile int *sync = (int *)malloc(sizeof(int));
	if(!sync){
		printf("Err in allocating memory\n");
		return EXIT_FAILURE;
	}
	
	*sync = 0;
	
	pTs *th1Arg,*th2Arg;

	th1Arg = (pTs *)malloc(sizeof(pTs));
	th2Arg = (pTs *)malloc(sizeof(pTs));

	th1Arg->arg_1 = head1;
	th1Arg->arg_2 = k1;
	th1Arg->argSync = sync;
	
	th2Arg->arg_1 = head2;
	th2Arg->arg_2 = k2;
	th2Arg->argSync = sync;
	
	pthread_t thr_1,thr_2;

	int err;
/*	if (pthread_mutex_init(&mutex, NULL) != 0) {                                  */
/*		perror("pthread_mutex_init() error");                                       */
/*    		return EXIT_FAILURE;                                                                    */
/*	} */

	err = pthread_create(&thr_1,NULL,func_1,(void *)th1Arg);
	if(err){
		printf("Error in creating pthread 1\n");
		return EXIT_FAILURE;
	}

	err = pthread_create(&thr_2,NULL,func_2,(void *)th2Arg);
	if(err){
		printf("Error in creating pthread 1\n");
		return EXIT_FAILURE;
	}
	
	for(int k=0;k<0xffff;k++);
	
	*sync =1;
	
	pthread_join(thr_1,NULL);
	pthread_join(thr_2,NULL);
	printf("back\n");
	free(k1);
	free(k2);
	free(sync);
	return 0;
}

void* func_1(void *arg){

	pTs *localArg = (pTs *)arg;

	ts *temp = localArg->arg_1;
	int *lInt = localArg->arg_2;
	volatile int *lSync = localArg->argSync;
	
	printf("th 1 waiting\n");
	while(!(*lSync));
	
	printf("th 1 progressed but still waiting from th 2 \n");
	while(*lSync);
	printf("th 1 progressed\n");
	for(int i = 0;i<size;i++){
		printf("thr 1 : %d \t %d \t %f \t %d \n",i,temp->var_1,temp->var_2,lInt[i]);
		temp = temp->next;		
	}
	
	*lSync =1;
}


void* func_2(void *arg){

	pTs *localArg = (pTs *)arg;

	ts *temp = localArg->arg_1;
	int *lInt = localArg->arg_2;
	volatile int *lSync = localArg->argSync;
	
	printf("th 2 waiting\n");
	while(!(*lSync));
	
	printf("th 2 progressed\n");
	
	for(int i = 0;i<size;i++){
		printf("thr 2 : %d \t %d \t %f \t %d \n",i,temp->var_1,temp->var_2,lInt[i]);
		temp = temp->next;		
	}
	
	*lSync = 0;
	printf("th 2 waiting again\n");
	while(!(*lSync));
	
	for(int i = 0;i<size;i++)
		printf("This is BS\n");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 *	File	: rw.c
 *
 *	Title	: Demo Readers/Writer.
 *
 *	Short	: A solution to the multi-reader's, one writer problem.
 *
 *	Long	:
 *
 *	Author	: Andrae Muys
 *
 *	Date	: 18 September 1997
 *
 *	Revised	:
 */

/*#include <pthread.h>*/
/*#include <stdio.h>*/
/*#include <unistd.h>*/
/*#include <stdlib.h>*/

/*#define MAXCOUNT 5*/

/*#define READER1  50000*/
/*#define READER2 100000*/
/*#define READER3	400000*/
/*#define READER4 800000*/
/*#define WRITER1  150000*/

/*typedef struct {*/
/*	pthread_mutex_t *mut;*/
/*	int writers;*/
/*	int readers;*/
/*	int waiting;*/
/*	pthread_cond_t *writeOK, *readOK;*/
/*} rwl;*/

/*rwl *initlock (void);*/
/*void readlock (rwl *lock, int d);*/
/*void writelock (rwl *lock, int d);*/
/*void readunlock (rwl *lock);*/
/*void writeunlock (rwl *lock);*/
/*void deletelock (rwl *lock);*/

/*typedef struct {*/
/*	rwl *lock;*/
/*	int id;*/
/*	long delay;*/
/*} rwargs;*/

/*rwargs *newRWargs (rwl *l, int i, long d);*/
/*void *reader (void *args);*/
/*void *writer (void *args);*/

/*static int data = 1;*/

/*int main ()*/
/*{*/
/*	pthread_t r1, r2, r3, r4, w1;*/
/*	rwargs *a1, *a2, *a3, *a4, *a5;*/
/*	rwl *lock;*/

/*	lock = initlock ();*/
/*	a1 = newRWargs (lock, 1, WRITER1);*/
/*	pthread_create (&w1, NULL, writer, a1);*/
/*	a2 = newRWargs (lock, 1, READER1);*/
/*	pthread_create (&r1, NULL, reader, a2);*/
/*	a3 = newRWargs (lock, 2, READER2);*/
/*	pthread_create (&r2, NULL, reader, a3);*/
/*	a4 = newRWargs (lock, 3, READER3);*/
/*	pthread_create (&r3, NULL, reader, a4);*/
/*	a5 = newRWargs (lock, 4, READER4);*/
/*	pthread_create (&r4, NULL, reader, a5);*/
/*	pthread_join (w1, NULL);*/
/*	pthread_join (r1, NULL);*/
/*	pthread_join (r2, NULL);*/
/*	pthread_join (r3, NULL);*/
/*	pthread_join (r4, NULL);*/
/*	free (a1); free (a2); free (a3); free (a4); free (a5);*/

/*	return 0;*/
/*}*/

/*rwargs *newRWargs (rwl *l, int i, long d)*/
/*{*/
/*	rwargs *args;*/

/*	args = (rwargs *)malloc (sizeof (rwargs));*/
/*	if (args == NULL) return (NULL);*/
/*	args->lock = l; args->id = i; args->delay = d;*/
/*	return (args);*/
/*}*/

/*void *reader (void *args)*/
/*{*/
/*	*/
/*	*/
/*	rwargs *a;*/
/*	int d;*/

/*	a = (rwargs *)args;*/
/*	printf("Reader %d thread enter\n",a->id);*/
/*	do {*/
/*		readlock (a->lock, a->id);*/
/*		d = data;*/
/*		usleep (a->delay);*/
/*		readunlock (a->lock);*/
/*		printf ("Reader %d : Data = %d\n", a->id, d);*/
/*		usleep (a->delay);*/
/*	} while (d != 0);*/
/*	printf ("Reader %d: Finished.\n", a->id);*/

/*	return (NULL);*/
/*}*/

/*void *writer (void *args)*/
/*{*/
/*	rwargs *a;*/
/*	int i;*/

/*	a = (rwargs *)args;*/
/*	printf("Writer %d thread enter\n",a->id);*/

/*	for (i = 2; i < MAXCOUNT; i++) {*/
/*		writelock (a->lock, a->id);*/
/*		data = i;*/
/*		usleep (a->delay);*/
/*		writeunlock (a->lock);*/
/*		printf ("Writer %d: Wrote %d\n", a->id, i);*/
/*		usleep (a->delay);*/
/*	}*/
/*	printf ("Writer %d: Finishing...\n", a->id);*/
/*	writelock (a->lock, a->id);*/
/*	data = 0;*/
/*	writeunlock (a->lock);*/
/*	printf ("Writer %d: Finished.\n", a->id);*/

/*	return (NULL);*/
/*}*/

/*rwl *initlock (void)*/
/*{*/
/*	rwl *lock;*/

/*	lock = (rwl *)malloc (sizeof (rwl));*/
/*	if (lock == NULL) return (NULL);*/
/*	lock->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));*/
/*	if (lock->mut == NULL) { free (lock); return (NULL); }*/
/*	lock->writeOK = */
/*		(pthread_cond_t *) malloc (sizeof (pthread_cond_t));*/
/*	if (lock->writeOK == NULL) { free (lock->mut); free (lock); */
/*		return (NULL); }*/
/*	lock->readOK = */
/*		(pthread_cond_t *) malloc (sizeof (pthread_cond_t));*/
/*	if (lock->writeOK == NULL) { free (lock->mut); free (lock->writeOK); */
/*		free (lock); return (NULL); }*/
/*	*/
/*	pthread_mutex_init (lock->mut, NULL);*/
/*	pthread_cond_init (lock->writeOK, NULL);*/
/*	pthread_cond_init (lock->readOK, NULL);*/
/*	lock->readers = 0;*/
/*	lock->writers = 0;*/
/*	lock->waiting = 0;*/

/*	return (lock);*/
/*}*/

/*void readlock (rwl *lock, int d)*/
/*{*/
/*	printf("Enter read lock thread %d\n",d);*/
/*	pthread_mutex_lock (lock->mut);*/
/*	if (lock->writers || lock->waiting) {*/
/*		do {*/
/*			printf ("reader %d blocked.\n", d);*/
/*			pthread_cond_wait (lock->readOK, lock->mut);*/
/*			printf ("reader %d unblocked.\n", d);*/
/*		} while (lock->writers);*/
/*	}*/
/*	lock->readers++;*/
/*	pthread_mutex_unlock (lock->mut);*/
/*	printf("Enter read unlock thread %d\n",d);*/
/*	return;*/
/*}*/

/*void writelock (rwl *lock, int d)*/
/*{*/
/*	printf("Enter  writer lock thread %d\n",d);*/
/*	pthread_mutex_lock (lock->mut);*/
/*	lock->waiting++;*/
/*	while (lock->readers || lock->writers) {*/
/*		printf ("writer %d blocked.\n", d);*/
/*		pthread_cond_wait (lock->writeOK, lock->mut);*/
/*		printf ("writer %d unblocked.\n", d);*/
/*	}*/
/*	lock->waiting--;*/
/*	lock->writers++;*/
/*	pthread_mutex_unlock (lock->mut);*/
/*	printf("Exit writer lock thread %d\n",d);*/
/*	return;*/
/*}*/

/*void readunlock (rwl *lock)*/
/*{*/
/*	printf("Enter read unlock\n");*/
/*	pthread_mutex_lock (lock->mut);*/
/*	lock->readers--;*/
/*	pthread_cond_signal (lock->writeOK);*/
/*	pthread_mutex_unlock (lock->mut);*/
/*	printf("Exit write unlock\n");*/
/*}*/

/*void writeunlock (rwl *lock)*/
/*{*/
/*	printf("Enter write unlock\n");*/
/*	pthread_mutex_lock (lock->mut);*/
/*	lock->writers--;*/

/*	pthread_cond_broadcast (lock->readOK);*/
/*	pthread_mutex_unlock (lock->mut);*/
/*	printf("Exit write unlock\n");*/
/*}*/

/*void deletelock (rwl *lock)*/
/*{*/
/*	pthread_mutex_destroy (lock->mut);*/
/*	pthread_cond_destroy (lock->readOK);*/
/*	pthread_cond_destroy (lock->writeOK);*/
/*	free (lock);*/

/*	return;*/
/*}*/


