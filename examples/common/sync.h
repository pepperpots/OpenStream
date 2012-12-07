#ifndef SYNC_H
#define SYNC_H

/**
 * Contains routines that allow a benchmark to synchronize
 * with a profiling process through a shared memory segment.
 */

/**
 * Structure used for shared memory segment creation and
 * synchronization.
 */
struct profiler_sync {
	/* Name of the file that is used to generate the
	   IPC key that identified the sahred memory segment */
	const char* filename;

	/* Synchronization variable */
	volatile int* status;

	/* Shared memory segment identifier */
	int shmid;

	/* Flag indicating if the shared memory segment was
	   created by this process */
	int owner;
};

/**
 * Values for field status in struct profiler_sync
 */
#define SYNC_INIT 0
#define SYNC_RECORD 1
#define SYNC_PAUSE 2
#define SYNC_STOP 3

/**
 * Attach to a shared memory segment. If create != 0, create
 * the segment if it does not exist.
 * Returns 0 on success or a value different from 0 on failure
 */
int create_sync_shm(struct profiler_sync* sync, int create);

/**
 * Detach the shared memory segment. If the segment was created
 * by the process, it is automatically destroyed.
 * Returns 0 on success or a value different from 0 on failure
 */
int detach_sync_shm(struct profiler_sync* sync);

#ifdef NOTIFY_PROFILER
#define PROFILER_NOTIFY_PREPARE(sync)					\
	do {								\
		(sync)->filename = getenv("SYNC_FILE");		\
		if(create_sync_shm(sync, 0)) {				\
			fputs("Could not attach shared memory segment"	\
			      " for profiler notification\n", stderr);	\
			exit(1);					\
		}							\
	} while(0)

#define PROFILER_NOTIFY_FINISH(sync)					\
	do {								\
		if(detach_sync_shm(sync))				\
			fputs("Could not detach shared memory segment"	\
			      " for profiler notification\n", stderr);	\
		exit(1);						\
	} while(0)

#define PROFILER_STATUS(sync, st)			\
	do {						\
		*((sync)->status) = st;		\
	} while(0)

#define PROFILER_NOTIFY_RECORD(sync)			\
	PROFILER_STATUS(sync, SYNC_RECORD)
#define PROFILER_NOTIFY_PAUSE(sync)			\
	PROFILER_STATUS(sync, SYNC_PAUSE)
#define PROFILER_NOTIFY_STOP(sync)			\
	PROFILER_STATUS(sync, SYNC_STOP)
#else
#define PROFILER_NOTIFY_RECORD(sync)
#define PROFILER_NOTIFY_PAUSE(sync)
#define PROFILER_NOTIFY_STOP(sync)

#define PROFILER_NOTIFY_PREPARE(sync)
#define PROFILER_NOTIFY_FINISH(sync)
#endif

#endif
