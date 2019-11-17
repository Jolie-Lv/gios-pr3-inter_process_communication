#include <curl/curl.h>
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
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "gfserver.h"
#include "shm_channel.h"
#include "cache-student.h"
#include "steque.h"

#define BUFSIZE (6200)

int req_mq_id, res_mq_id;
static long req_id = 1;

pthread_mutex_t scmchseg_q_lock, gfs_lock;

steque_t *shmch_seg_queue;

void init_shmchseg_pool(unsigned int shmch_segno, size_t shmch_segsize);
void init_synchqueue_pool();
int checkcachestatus(cache_reqstruct *cache_request);
void destroy_shmchseg_pool();
void destroy_synchqueue_pool();

//Replace with an implementation of handle_with_cache and any other
//functions you may need.
void init_ipc_mechanism(unsigned int shmch_segno, size_t shmch_segsize) {
    init_shmchseg_pool(shmch_segno, shmch_segsize);
    init_synchqueue_pool();

}

void destroy_ipc_mechanism() {
    destroy_shmchseg_pool();
    destroy_synchqueue_pool();
}

void init_shmchseg_pool(unsigned int shmch_segno, size_t shmch_segsize) {
    int shmch_segcounter;
    fprintf(stdout, "**INIT-Shared Memory Pool**\n");

    fprintf(stdout, "*INIT-Shared Memory Segment Queue*\n");
    shmch_seg_queue = malloc(sizeof(steque_t));
    steque_init(shmch_seg_queue);
    fprintf(stdout, "*SUCCESS-Shared Memory Segment Queue*\n");


    fprintf(stdout, "*INIT-Shared Memory Segment Lock Mutex*\n");
    if (pthread_mutex_init(&scmchseg_q_lock, NULL) != 0) {
        fprintf(stderr, "*ERROR-Shared Memory Segment Lock Mutex*\n");
        exit(GF_ERROR);
    }
    fprintf(stdout, "*SUCCESS-Shared Memory Segment Lock Mutex*\n");

    fprintf(stdout, "*INIT-Shared Memory Segment Initialization*\n");
    for (shmch_segcounter = 0; shmch_segcounter < shmch_segno; shmch_segcounter++) {

        int shmch_segid = create_shmch_seg(shmch_segsize, shmch_segcounter);

        shmch_segstruct *shmch_segstruct = malloc(sizeof(shmch_segno) + shmch_segsize);
        shmch_segstruct->shmch_segid = shmch_segid;
        shmch_segstruct->shmch_segsize = shmch_segsize;

        steque_enqueue(shmch_seg_queue, shmch_segstruct);

        fprintf(stdout, "Shared Memory Segment ID - %d\n", shmch_segid);
    }
    fprintf(stdout, "*INIT-Shared Memory Segment Initialization\n");
    fprintf(stdout, "**SUCCESS-Shared Memory Pool**\n");
}

void destroy_shmchseg_pool() {

    fprintf(stdout, "**INIT-Shared Memory Pool Destroy**\n");
    while (!steque_isempty(shmch_seg_queue)) {
        shmch_segstruct *shmch_segstruct = steque_pop(shmch_seg_queue);
        destroy_shmch_seg(shmch_segstruct->shmch_segid);
        fprintf(stdout, "**SUCCESS-Shared Memory Segment ID Destroy - %d**\n", shmch_segstruct->shmch_segid);
        free(shmch_segstruct);
    }
    steque_destroy(shmch_seg_queue);
    free(shmch_seg_queue);
    fprintf(stdout, "**SUCCESS-Shared Memory Pool Destroy**\n");
}

void init_synchqueue_pool() {
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

void destroy_synchqueue_pool() {
    fprintf(stdout, "**INIT-Synchronization Queue Pool Destroy**\n");

    if (msgctl(req_mq_id, IPC_RMID, NULL) == -1) {
        fprintf(stderr, "*ERROR-Request Message Queue Destroy*\n");
        exit(GF_ERROR);
    }

    if (msgctl(res_mq_id, IPC_RMID, NULL) == -1) {
        fprintf(stderr, "*ERROR-Response Message Queue Destroy*\n");
        exit(GF_ERROR);
    }
    fprintf(stdout, "**SUCCESS-Synchronization Queue Pool Destroy**\n");
}

shmch_segstruct getshmch_segment() {
    while (1) {
        if (!steque_isempty(shmch_seg_queue)) {
            pthread_mutex_lock(&scmchseg_q_lock);
            if (!steque_isempty(shmch_seg_queue)) {
                shmch_segstruct *shmch_segment = steque_pop(shmch_seg_queue);
                return *shmch_segment;
            }
 	    //fprintf(stdout, "*INIT-Shared Memory Segment ID popped - %d*\n", shmch_segment->shmch_segid);
            pthread_mutex_unlock(&scmchseg_q_lock);
        }
    }
}

int checkcachestatus(cache_reqstruct *cache_request){

    size_t size = sizeof(cache_request) - sizeof(long);
    //ssize_t reqmsg_size = msgsnd(req_mq_id, cache_request, size, 0);
    msgsnd(req_mq_id, cache_request, size, 0);

    while(msgrcv(res_mq_id, cache_request, sizeof(cache_request), cache_request->reqid, 0) <= 0)
    {
        sleep(1);
    }
    return cache_request->cachestatus;
}

ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void *arg) {

    fprintf(stdout, "**INIT-Shared Memory Segment**\n");
    shmch_segstruct shmch_segment = getshmch_segment();

    cache_reqstruct *cache_request = malloc(sizeof(shmch_segment) + sizeof(path));
    cache_request->shmch_segment = shmch_segment;
    cache_request->Path = path;
    cache_request->reqid = req_id;
    req_id = req_id+1;
    fprintf(stdout, "**SUCCESS-Shared Memory Segment**\n");

    ssize_t fileSize = 0;

    int cachestatus = checkcachestatus(cache_request);

    //Step 2 SHMCH connected
    if (cachestatus == 0) {
        shmch_transtruct *data_shmchtransfer = connect_shmch_seg(cache_request->shmch_segment.shmch_segid);
        while (data_shmchtransfer->datatransstatus == 100);
        fileSize = data_shmchtransfer->datasize;

        pthread_mutex_lock(&gfs_lock);
        gfs_sendheader(ctx, GF_OK, (size_t) fileSize);
        pthread_mutex_unlock(&gfs_lock);

        //Step 3 SHMCH transfer
        ssize_t data_trans = 0;
        while (data_trans < fileSize) {
            sem_wait(data_shmchtransfer->datasema);

            if (data_shmchtransfer->datatransstatus == 300) {
                pthread_mutex_lock(&gfs_lock);
                ssize_t sentBytes = gfs_send(ctx, data_shmchtransfer->data, cache_request->shmch_segment.shmch_segsize);
                if (sentBytes <= 0) {
                    fprintf(stderr, "ERROR In transfer to client\n");
                    pthread_mutex_unlock(&gfs_lock);
                    exit(GF_ERROR);
                }
                pthread_mutex_unlock(&gfs_lock);
                data_trans += sentBytes;
                data_shmchtransfer->datatransstatus = 400;
            } else if (data_shmchtransfer->datatransstatus == 500) {
                pthread_mutex_lock(&gfs_lock);
                ssize_t sentBytes = gfs_send(ctx, data_shmchtransfer->data, (size_t) fileSize - data_trans);
                if (sentBytes < 0) {
                    fprintf(stderr, "ERROR In transfer to client\n");
                    pthread_mutex_unlock(&gfs_lock);
                    exit(GF_ERROR);
                }
                pthread_mutex_unlock(&gfs_lock);
                data_trans += sentBytes;
            }

            sem_post(data_shmchtransfer->datasema);
        }
        data_shmchtransfer->datatransstatus = 100;
        data_shmchtransfer->datasize = 0;
        disconnect_shmch_seg(data_shmchtransfer);
    } else {
        return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
    }
    return fileSize;
}

