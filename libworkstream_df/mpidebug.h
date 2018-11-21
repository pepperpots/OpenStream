#ifndef __OPENSTREAM_MPIDEBUG_
#define __OPENSTREAM_MPIDEBUG_

static void debug_this ()
{
    int i = 0;
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    printf("PID %d on %s ready for attach\n", getpid(), hostname);
    fflush(stdout);
    while (0 == i)
        sleep(5);
}
#endif
