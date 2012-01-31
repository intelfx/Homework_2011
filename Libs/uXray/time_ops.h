#ifndef _TIME_OPS_H
#define _TIME_OPS_H

#include "build.h"
#include <time.h>

timespec clock_resolution, start_clock;
clockid_t target_clock;
static const unsigned long nano_divisor = 1000000000;
const char* action_string;

void timeops_calibrate_and_setup_clock (clockid_t clock = CLOCK_THREAD_CPUTIME_ID)
{
	timespec calibrate_source;
	target_clock = clock;

	int value = clock_getres (target_clock, &calibrate_source);
	__sassert (!value, "Clock resolution get failed: %s", strerror (errno));

	clock_resolution.tv_sec  = calibrate_source.tv_nsec / nano_divisor;
	clock_resolution.tv_nsec = calibrate_source.tv_nsec % nano_divisor;
	clock_resolution.tv_sec += calibrate_source.tv_sec;

	if (clock_resolution.tv_sec)
		smsg (E_WARNING, E_VERBOSE, "Clock resolution too big: %lu.%09lu",
			  clock_resolution.tv_sec, clock_resolution.tv_nsec);

	else
		smsg (E_INFO, E_DEBUG, "Clock resolution: .%09lu",
			  clock_resolution.tv_nsec);
}

void timeops_timespec_subtract (timespec& minuend, const timespec& subtrahend)
{
	while (minuend.tv_nsec < subtrahend.tv_nsec)
	{
		minuend.tv_nsec += nano_divisor;
		--minuend.tv_sec;
	}

	minuend.tv_nsec -= subtrahend.tv_nsec;
	minuend.tv_sec -= subtrahend.tv_sec;
}

inline timespec timeops_measure_once()
{
	timespec time_source;

	int value = clock_gettime (target_clock, &time_source);
	__sassert (!value, "Clock get failed: %s", strerror (errno));

	long nsec_error = clock_resolution.tv_nsec ? time_source.tv_nsec % clock_resolution.tv_nsec : 0;
	long sec_error  = clock_resolution.tv_sec  ? time_source.tv_sec  % clock_resolution.tv_sec  : 0;

	timespec errors = { sec_error, nsec_error };

	start_clock = time_source;
	timeops_timespec_subtract (start_clock, errors);

	if (nsec_error || sec_error)
		smsg (E_WARNING, E_VERBOSE, "Clock reading does not obey resolution: %lu.%09lu -> errors [%lu.%09lu]",
			  time_source.tv_sec, time_source.tv_nsec, sec_error, nsec_error);

	return time_source;
}

inline void timeops_measure_start (const char* message)
{
	timespec time_source;

	if (message)
		smsg (E_INFO, E_VERBOSE, "Starting measure: %s", message);

	action_string = message;

	int value = clock_gettime (target_clock, &time_source);
	__sassert (!value, "Clock get failed: %s", strerror (errno));

	long nsec_error = clock_resolution.tv_nsec ? time_source.tv_nsec % clock_resolution.tv_nsec : 0;
	long sec_error  = clock_resolution.tv_sec  ? time_source.tv_sec  % clock_resolution.tv_sec  : 0;

	timespec errors = { sec_error, nsec_error };

	start_clock = time_source;
	timeops_timespec_subtract (start_clock, errors);

	if (nsec_error || sec_error)
		smsg (E_WARNING, E_DEBUG, "Clock reading does not obey resolution: %lu.%09lu -> errors [%lu.%09lu]",
			  time_source.tv_sec, time_source.tv_nsec, sec_error, nsec_error);
}

inline double timeops_timespec_to_fp (const timespec& time)
{
	double result;
	result  = time.tv_nsec;
	result /= nano_divisor;
	result += time.tv_sec;
	return result;
}

inline timespec timeops_measure_end (bool show_message = 1)
{
	timespec time_source;

	int value = clock_gettime (target_clock, &time_source);
	__sassert (!value, "Clock get failed: %s", strerror (errno));

	long nsec_error = clock_resolution.tv_nsec ? time_source.tv_nsec % clock_resolution.tv_nsec : 0;
	long sec_error  = clock_resolution.tv_sec  ? time_source.tv_sec  % clock_resolution.tv_sec  : 0;

	timespec errors = { sec_error, nsec_error };

	timeops_timespec_subtract (time_source, errors);
	timeops_timespec_subtract (time_source, start_clock);

	if (nsec_error || sec_error)
		smsg (E_WARNING, E_DEBUG, "Clock reading does not obey resolution: %lu.%09lu -> errors [%lu.%09lu]",
			  time_source.tv_sec, time_source.tv_nsec, sec_error, nsec_error);

	if (show_message)
		smsg (E_INFO, E_VERBOSE, "%s took %lu.%09lu seconds",
			  action_string, time_source.tv_sec, time_source.tv_nsec);

	return time_source;
}

#endif // _TIME_OPS_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
