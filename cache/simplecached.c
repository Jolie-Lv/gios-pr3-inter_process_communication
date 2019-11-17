#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <printf.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/signal.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/stat.h>

#include "cache-student.h"
#include "gfserver.h"
#include "shm_channel.h"
#include "simplecache.h"

#if !defined(CACHE_FAILURE)
#define CACHE_FAILURE (-1)
#endif // CACHE_FAILURE

#define MAX_CACHE_REQUEST_LEN 6200
#define THREAD_NUM 100

int req_mq_id, res_mq_id;

pthread_t cache_threadid [THREAD_NUM];
int threadpoolcount = 0;
int threadcount = 1;
pthread_mutex_t scmchseg_qc_lock, data_lock;

steque_t *shmch_seg_cachequeue;

void init_thread_constructs();
void init_thread_pool(int nthreads);
void cache_datatransfer();
void cache_writeshmch(cache_reqstruct* cache_request);
void destroy_threads();
void init_synchqueue();


static void _sig_handler(int signo){
	if (signo == SIGINT || signo == SIGTERM){
		/* Unlink IPC mechanisms here*/
		destroy_threads();
		exit(signo);
	}
}

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  simplecached [options]\n"                                                  \
"options:\n"                                                                  \
"  -c [cachedir]       Path to static files (Default: ./)\n"                  \
"  -t [thread_count]   Thread count for work queue (Default: 3, Range: 1-1024)\n"      \
"  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"cachedir",           required_argument,      NULL,           'c'},
  {"nthreads",           required_argument,      NULL,           't'},
  {"help",               no_argument,            NULL,           'h'},
  {"hidden",			 no_argument,			 NULL,			 'i'}, /* server side */
  {NULL,                 0,                      NULL,             0}
};

void Usage() {
  fprintf(stdout, "%s", USAGE);
}
void init_threads(int nthreads){
	init_thread_constructs();
	init_thread_pool(nthreads);
}

void init_thread_constructs(){
	fprintf(stdout, "**INIT-Thread Constructs**\n");

	fprintf(stdout, "*INIT-Cache Queue*\n");
	shmch_seg_cachequeue = malloc(sizeof(steque_t));
	steque_init(shmch_seg_cachequeue);
	fprintf(stdout, "*SUCCESS-Cache Queue*\n");

	fprintf(stdout, "*INIT-Cache Queue Lock Mutex*\n");
	if (pthread_mutex_init(&scmchseg_qc_lock, NULL) != 0) {
		fprintf(stderr, "*ERROR-Cache Queue Lock Mutex*\n");
		exit(GF_ERROR);
	}
	fprintf(stdout, "*SUCCESS-Cache Queue Lock Mutex*\n");
	fprintf(stdout, "*INIT-Data Lock Mutex*\n");
	if (pthread_mutex_init(&data_lock, NULL) != 0)
	{
		fprintf(stderr, "*ERROR-Data Lock Mutex*\n");
		exit(GF_ERROR);
	}
	fprintf(stdout, "*SUCCESS-Data Lock Mutex*\n");
	fprintf(stdout, "**SUCCESS-Thread Constructs**\n");
}

void init_thread_pool(int nthreads){
	fprintf(stdout, "**INIT-Thread Pool**\n");
	int returncode = 0, threadpoolcounter;
	threadpoolcount = nthreads;

	for(threadpoolcounter=0;threadpoolcounter < threadpoolcount; threadpoolcounter++)
	{
		printf("Creating thread: %d\n", threadpoolcounter);

		returncode = pthread_create(&(cache_threadid[threadpoolcounter]), NULL, (void*)&cache_datatransfer, &(cache_threadid[threadpoolcounter]));
		if (returncode != 0)
		{
			fprintf(stderr, "*ERROR-Thread ID - %d, error code - %d*\n", threadpoolcounter, returncode);
			exit(GF_ERROR);
		}
		fprintf(stdout, "*INIT-Thread ID - %d*\n", threadpoolcounter);
	}
	fprintf(stdout, "**SUCCESS-Thread Pool**\n");

}

void cache_datatransfer() {
	fprintf(stdout, "**INIT-Cache Transfer**\n");
	while(threadcount == 1)
	{
		if(steque_isempty(shmch_seg_cachequeue))
			continue;
		pthread_mutex_lock(&scmchseg_qc_lock);
		cache_reqstruct* cache_request = steque_pop(shmch_seg_cachequeue);
		pthread_mutex_unlock(&scmchseg_qc_lock);
		if(cache_request != NULL)
		{
			cache_writeshmch(cache_request);
			free(cache_request);
		}
	}
	fprintf(stdout, "**SUCCESS Cache Transfer**\n");
	pthread_exit(NULL);
}

void cache_writeshmch(cache_reqstruct* cache_request) {
	fprintf(stdout, "**INIT-Cache To SHared Memory Write**\n");
	int filedescriptor = simplecache_get(cache_request->Path);
	ssize_t filesize = lseek(filedescriptor, 0, SEEK_END);

	//Step 2 SHMCH connected
	shmch_transtruct* data_shmchtransfer = connect_shmch_seg(cache_request->shmch_segment.shmch_segid);
	data_shmchtransfer->datasize = filesize;
	data_shmchtransfer->datatransstatus = 200;

	//Step 3 SHMCH transfer
	pthread_mutex_lock(&data_lock);
	ssize_t data_trans = 0;
	while(data_trans < filesize)
	{
		sem_wait(data_shmchtransfer->datasema);

		if(data_shmchtransfer->datatransstatus == 300 || data_shmchtransfer->datatransstatus == 400)
		{
			ssize_t readbytes = pread(filedescriptor, data_shmchtransfer->data, cache_request->shmch_segment.shmch_segsize, data_trans);
			if (readbytes <= 0)
			{
				fprintf(stderr,"ERROR In reading from shared memory\n");
				pthread_mutex_unlock(&data_lock);
				exit(GF_ERROR);
			}
			data_trans += readbytes;

			if(data_trans < filesize)
				data_shmchtransfer->datatransstatus = 300;
			else
				data_shmchtransfer->datatransstatus= 500;
		}

		sem_post(data_shmchtransfer->datasema);
	}
	pthread_mutex_unlock(&data_lock);
	fprintf(stdout, "**SUCCESS-Cache To SHared Memory Write**\n");
}

void destroy_threads() {
	fprintf(stdout, "**INIT-Thread Destroy**\n");
	threadcount = 0;
	steque_destroy(shmch_seg_cachequeue);
	free(shmch_seg_cachequeue);
	fprintf(stdout, "**SUCCESS-Thread Destroy**\n");
}

void init_synchqueue() {
	fprintf(stdout, "**INIT-Synchronization Queue Pool**\n");

	key_t req_mqkey, res_mqkey;

	if ((req_mqkey = ftok(".", 'A')) == -1) {
		fprintf(stderr, "*ERROR-Request Message Queue Key*\n");
		exit(GF_ERROR);
	}

	if ((req_mq_id = msgget(req_mqkey, 0666 | IPC_CREAT)) == -1) {
		fprintf(stderr, "*ERROR-Request Message Queue*\n");
		exit(GF_ERROR);
	}

	if ((res_mqkey = ftok(".", 'B')) == -1) {
		fprintf(stderr, "*ERROR-Response Message Queue Key*\n");
		exit(GF_ERROR);
	}

	if ((res_mq_id = msgget(res_mqkey, 0666 | IPC_CREAT)) == -1) {
		fprintf(stderr, "*ERROR-Response Message Queue*\n");
		exit(GF_ERROR);
	}

	fprintf(stdout, "**SUCCESS-Synchronization Queue Pool**\n");
}

void serve_requests() {
	while(1) {
		cache_reqstruct *cache_request = malloc(sizeof(cache_reqstruct));
		ssize_t reqmsg_size = msgrcv(req_mq_id, cache_request, sizeof(cache_reqstruct), 0, 0);
		if(reqmsg_size > 0)
		{
			if(simplecache_get(cache_request->Path) > 0)
			{
				cache_request->cachestatus = 0;
				steque_enqueue(shmch_seg_cachequeue, cache_request);
			}
			else
				cache_request->cachestatus = 1;

			size_t size = sizeof(cache_request) - sizeof(long);
			msgsnd(res_mq_id, cache_request, size, 0);
		}
	}
}

int main(int argc, char **argv) {
	int nthreads = 3;
	char *cachedir = "locals.txt";
	char option_char;
	int status = 0;

	/* disable buffering to stdout */
	setbuf(stdout, NULL);

	while ((option_char = getopt_long(argc, argv, "ic:ht:x", gLongOptions, NULL)) != -1) {
		switch (option_char) {
			default:
				status = 1;
			case 'h': // help
				Usage();
				exit(status);
			case 'c': //cache directory
				cachedir = optarg;
				break;
			case 't': // thread-count
				nthreads = atoi(optarg);
				break;   
			case 'i': // server side usage
				break;
			case 'x': // experimental
				break;
		}
	}

	if ((nthreads>6200) || (nthreads < 1)) {
		fprintf(stderr, "Invalid number of threads\n");
		exit(__LINE__);
	}

	if (SIG_ERR == signal(SIGINT, _sig_handler)){
		fprintf(stderr,"Unable to catch SIGINT...exiting.\n");
		exit(CACHE_FAILURE);
	}

	if (SIG_ERR == signal(SIGTERM, _sig_handler)){
		fprintf(stderr,"Unable to catch SIGTERM...exiting.\n");
		exit(CACHE_FAILURE);
	}

	/* Cache initialization */
	simplecache_init(cachedir);

	/* Thread initialization */
	init_threads(nthreads);

	/* Queue initialization */
	init_synchqueue();

	/* Serve requests */
	/* Note: this loops forever */
	serve_requests();

	/* Thread destruction */
	destroy_threads();

	/* this code probably won't execute */
	return 0;
}
