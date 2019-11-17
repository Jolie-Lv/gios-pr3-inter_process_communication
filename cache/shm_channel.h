#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <semaphore.h>

#include "steque.h"

#define INPUT (1024)

typedef struct shmch_segstruct {
    int shmch_segid;
    size_t shmch_segsize;
} shmch_segstruct;

typedef struct cache_reqstruct {
    long reqid;
    int cachestatus;
    char *Path;
    shmch_segstruct shmch_segment;
} cache_reqstruct;

typedef struct shmch_transtruct {
    int datatransstatus;
    ssize_t datasize;
    sem_t* datasema;
    char *data;
} shmch_transtruct;

int create_shmch_seg(size_t shmch_segsize, int shmch_segcounter);

void destroy_shmch_seg(int shmid);

shmch_transtruct *connect_shmch_seg(int shmch_segid);

void disconnect_shmch_seg(shmch_transtruct *shmch_transtruct);

sem_t* create_sema();


