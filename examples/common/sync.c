#include "sync.h"
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif
#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

int create_sync_shm(struct profiler_sync* sync, int create)
{
	key_t key;
	int flags;

	if(!sync->filename) {
		fputs("Synchronization filename not set.\n", stderr);
		return 1;
	}

	/* Generate IPC key from filename and some magic value */
	if((key = ftok(sync->filename, 145)) == -1) {
		perror("Could not create key");
		return 1;
	}

	sync->owner = create;
	flags = S_IRUSR | S_IWUSR;

	if(sync->owner)
		flags |= IPC_CREAT | IPC_EXCL;

	/* Create or retrieve the shared memory segment's identifier */
	if((sync->shmid = shmget(key, sizeof(sync->status), flags)) == -1) {
		perror("Could not create shared memory");
		return 1;
	}

	/* Attach it to sync->status */
	if((sync->status = shmat(sync->shmid, NULL, 0)) == (void*)-1) {
		perror("Could not attach shared memory");

		if(sync->owner)
			shmctl(sync->shmid, IPC_RMID, NULL);

		return 1;
	}

	return 0;
}

int detach_sync_shm(struct profiler_sync* sync)
{
	/* Detach shared the memory segment */
	if(shmdt((void*)sync->status) == -1) {
		fputs("Could not detach shared memory.\n", stderr);
		return 1;
	}

	/* If this is the owner then delete the segment */
	if(sync->owner) {
		if(shmctl(sync->shmid, IPC_RMID, NULL) == -1) {
			perror("Could not delete shared memory");
			return 1;
		}
	}

	return 0;
}
