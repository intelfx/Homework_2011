#ifndef _TIME_OPS_H
#define _TIME_OPS_H

#include "build.h"
#include <time.h>

timespec clock_resolution, start_clock;
static const unsigned long nano_divisor = 1000000000;

void calibrate_and_setup_clock()
{
    timespec calibrate_source;

    int value = clock_getres (CLOCK_THREAD_CPUTIME_ID, &calibrate_source);
    __sassert (!value, "Clock resolution get failed: %s", strerror (errno));

    clock_resolution.tv_sec  = calibrate_source.tv_nsec / nano_divisor;
    clock_resolution.tv_nsec = calibrate_source.tv_nsec % nano_divisor;
    clock_resolution.tv_sec += calibrate_source.tv_sec;

    if (clock_resolution.tv_sec)
        smsg (E_WARNING, E_VERBOSE, "Clock resolution too big: %lu.%09lu",
              clock_resolution.tv_sec, clock_resolution.tv_nsec);

        else
            smsg (E_INFO, E_VERBOSE, "Clock resolution: .%09lu",
                  clock_resolution.tv_nsec);
}

inline void clock_measure_start (const char* message)
{
    timespec time_source;

    smsg (E_INFO, E_VERBOSE, "Starting measure on \"%s\"", message);

    int value = clock_gettime (CLOCK_THREAD_CPUTIME_ID, &time_source);
    __sassert (!value, "Clock get failed: %s", strerror (errno));

    long nsec_error = clock_resolution.tv_nsec ? time_source.tv_nsec % clock_resolution.tv_nsec : 0;
    long sec_error  = clock_resolution.tv_sec  ? time_source.tv_sec  % clock_resolution.tv_sec  : 0;

    start_clock.tv_nsec = time_source.tv_nsec - nsec_error;
    start_clock.tv_sec  = time_source.tv_sec - sec_error;

    if (nsec_error || sec_error)
        smsg (E_WARNING, E_VERBOSE, "Clock reading does not obey resolution: %lu.%09lu -> errors [%lu.%09lu]",
              time_source.tv_sec, time_source.tv_nsec, sec_error, nsec_error);
}

inline double timespec_to_fp (const timespec& time)
{
    double result;
    result  = time.tv_nsec;
    result /= nano_divisor;
    result += time.tv_sec;
    return result;
}

inline timespec clock_measure_end()
{
    timespec time_source;

    int value = clock_gettime (CLOCK_THREAD_CPUTIME_ID, &time_source);
    __sassert (!value, "Clock get failed: %s", strerror (errno));

    long nsec_error = clock_resolution.tv_nsec ? time_source.tv_nsec % clock_resolution.tv_nsec : 0;
    long sec_error  = clock_resolution.tv_sec  ? time_source.tv_sec  % clock_resolution.tv_sec  : 0;

    time_source.tv_nsec -= nsec_error;
    time_source.tv_sec  -= sec_error;

    time_source.tv_nsec -= start_clock.tv_nsec;
    time_source.tv_sec  -= start_clock.tv_sec;

    if (nsec_error || sec_error)
        smsg (E_WARNING, E_VERBOSE, "Clock reading does not obey resolution: %lu.%09lu -> errors [%lu.%09lu]",
              time_source.tv_sec, time_source.tv_nsec, sec_error, nsec_error);

        smsg (E_INFO, E_VERBOSE, "Action has taken %lu.%09lu seconds",
              time_source.tv_sec, time_source.tv_nsec);

        return time_source;
}

#endif // _TIME_OPS_H