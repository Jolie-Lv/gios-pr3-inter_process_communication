#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "shm_channel.h"
#include "gfserver.h"

int create_shmch_seg(size_t shmch_segsize, int shmch_segcounter){
    key_t shmch_segkey;
    int shmch_segid = 0;

    if ((shmch_segkey = ftok(".", shmch_segcounter)) == -1)
    {
        fprintf(stderr, "*ERROR-Shared Memory Segment Ftok\n");
        exit(GF_ERROR);
    }

    if ((shmch_segid = shmget(shmch_segkey, shmch_segsize, 0666 | IPC_CREAT)) == -1)
    {
        fprintf(stderr, "*ERROR-Shared Memory Segment Create\n");
        exit(GF_ERROR);
    }
    //Step 1 SHMCH created
    shmch_transtruct *data_shmchtransfer = connect_shmch_seg(shmch_segid);
    data_shmchtransfer->datatransstatus= 100;
    data_shmchtransfer->datasema = create_sema();
    disconnect_shmch_seg(data_shmchtransfer);
    return shmch_segid;
}

void destroy_shmch_seg (int shmch_segid){
    if(shmctl(shmch_segid, IPC_RMID, NULL) == -1)
    {
        fprintf(stderr, "*ERROR-Shared Memory Segment Destroy\n");
        exit(GF_ERROR);
    }
}

shmch_transtruct* connect_shmch_seg(int shmch_segid){
    shmch_transtruct *data_shmchtransfer = shmat(shmch_segid, (void *)0, 0);
    if (data_shmchtransfer == (shmch_transtruct *)(-1))
    {
        fprintf(stderr, "*ERROR-Shared Memory Segment Connect\n");
        exit(GF_ERROR);
    }
    return data_shmchtransfer;
}

void disconnect_shmch_seg(shmch_transtruct *data_shmchtransfer){
    if (shmdt(data_shmchtransfer) == -1)
    {
        fprintf(stderr, "*ERROR-Shared Memory Segment Disconnect\n");
        exit(GF_ERROR);
    }

}

sem_t* create_sema(){
    sem_t *data_sema;
    data_sema = sem_open("data_sema", O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0);
    return data_sema;
}
