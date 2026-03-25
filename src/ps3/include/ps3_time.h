#include <ppu-types.h>
#include <stdbool.h>
#include <sys/systime.h>
#include <sys/types.h>

static inline int ps3_clock_gettime(struct timespec *ts)
{
    u64 sec = 0, nsec = 0;
    sysGetCurrentTime(&sec, &nsec);
    if (ts)
    {
        ts->tv_sec = (time_t)sec;
        ts->tv_nsec = (long)nsec;
    }
    return 0;
}