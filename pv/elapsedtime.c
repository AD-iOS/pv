/*
 * Functions relating to elapsed time.
 *
 * Copyright 2023-2025 Andrew Wood
 *
 * License GPLv3+: GNU GPL version 3 or later; see `docs/COPYING'.
 */

#include "config.h"
#include "pv.h"
#include "pv-internal.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>


/*
 * Read the current elapsed time, relative to an unspecified point in the
 * past, and store it in the given timespec buffer.  The time is guaranteed
 * to not go backwards and does not count time when the system was
 * suspended.  See clock_gettime(2) with CLOCK_MONOTONIC.
 *
 * The read should not fail; if it does, the program is aborted with exit
 * status 16 (transfer error).
 */
void pv_elapsedtime_read(struct timespec *return_time)
{
	/*@-unrecog@ *//* splint doesn't know clock_gettime */
	if (0 != clock_gettime(CLOCK_MONOTONIC, return_time)) {
		fprintf(stderr, "%s: %s: %s\n", PACKAGE_NAME, "clock_gettime", strerror(errno));
		/*@-exitarg@ *//* we explicitly want a special exit status */
		exit(PV_ERROREXIT_TRANSFER);
		/*@+exitarg@ */
	}
	/*@+unrecog@ */
}


/*
 * Set the time in the given timespec to zero.
 */
void pv_elapsedtime_zero(struct timespec *zero_time)
{
	if (NULL == zero_time)
		return;
	zero_time->tv_sec = 0;
	zero_time->tv_nsec = 0;
}


/*
 * Copy source_time into dest_time.  Analogous to strcpy(3).
 */
void pv_elapsedtime_copy(struct timespec *dest_time, const struct timespec *source_time)
{
	if (NULL == dest_time)
		return;
	if (NULL == source_time)
		return;
	dest_time->tv_sec = source_time->tv_sec;
	dest_time->tv_nsec = source_time->tv_nsec;
}


/*
 * Return -1, 0, or 1 depending on whether the first time is earlier than,
 * equal to, or later than the second time.  Analogous to strcmp(3).
 */
int pv_elapsedtime_compare(const struct timespec *first_time, const struct timespec *second_time)
{
	/* Treat NULL as a zero time. */
	if ((NULL == first_time) && (NULL == second_time))
		return 0;
	if ((NULL == first_time) && (NULL != second_time))
		return -1;
	if ((NULL != first_time) && (NULL == second_time))
		return 1;

	/* These should never happen, but check just in case. */
	if (NULL == first_time)
		return 0;
	if (NULL == second_time)
		return 0;

	/* Check the seconds part, first */
	if (first_time->tv_sec < second_time->tv_sec)
		return -1;
	if (first_time->tv_sec > second_time->tv_sec)
		return 1;

	/* Seconds are equal - compare nanoseconds. */
	if (first_time->tv_nsec < second_time->tv_nsec)
		return -1;
	if (first_time->tv_nsec > second_time->tv_nsec)
		return 1;

	/* Nanoseconds are also equal - times are equal. */
	return 0;
}


/*
 * Add first_time and second_time, writing the result to return_time.
 */
void pv_elapsedtime_add(struct timespec *return_time, const struct timespec *first_time,
			const struct timespec *second_time)
{
	long long seconds, nanoseconds;

	if (NULL == return_time)
		return;

	seconds = 0;
	nanoseconds = 0;

	if (NULL != first_time) {
		seconds += first_time->tv_sec;
		nanoseconds += first_time->tv_nsec;
	}

	if (NULL != second_time) {
		seconds += second_time->tv_sec;
		nanoseconds += second_time->tv_nsec;
	}

	seconds += nanoseconds / 1000000000;
	nanoseconds = nanoseconds % 1000000000;

	/*@-type@ */
	return_time->tv_sec = seconds;
	return_time->tv_nsec = nanoseconds;
	/*@+type@ */

	/*
	 * splint rationale: we know the types are different but should be
	 * large enough and are relying on the compiler to do the casting
	 * correctly, since the manual for timespec(3) states the types are
	 * implementation-defined.
	 */
}


/*
 * Add a number of nanoseconds to the given timespec.
 */
void pv_elapsedtime_add_nsec(struct timespec *return_time, long long add_nanoseconds)
{
	long long seconds, nanoseconds;

	if (NULL == return_time)
		return;

	seconds = return_time->tv_sec;
	nanoseconds = return_time->tv_nsec + add_nanoseconds;

	seconds += nanoseconds / 1000000000;
	nanoseconds = nanoseconds % 1000000000;

	/*@-type@ *//* see above */
	return_time->tv_sec = seconds;
	return_time->tv_nsec = nanoseconds;
	/*@+type@ */
}


/*
 * Set the return timespec to the first time minus the second time.
 */
void pv_elapsedtime_subtract(struct timespec *return_time, const struct timespec *first_time,
			     const struct timespec *second_time)
{
	long long seconds, nanoseconds;

	if (NULL == return_time)
		return;

	seconds = 0;
	nanoseconds = 0;

	if (NULL != first_time) {
		seconds += first_time->tv_sec;
		nanoseconds += first_time->tv_nsec;
	}

	if (NULL != second_time) {
		seconds -= second_time->tv_sec;
		nanoseconds -= second_time->tv_nsec;
	}

	seconds += nanoseconds / 1000000000;
	nanoseconds = nanoseconds % 1000000000;

	if (nanoseconds < 0) {
		seconds--;
		nanoseconds = 1000000000 + nanoseconds;
	}

	/*@-type@ *//* see above */
	return_time->tv_sec = seconds;
	return_time->tv_nsec = nanoseconds;
	/*@+type@ */
}


/*
 * Convert a timespec to seconds.
 */
long double pv_elapsedtime_seconds(const struct timespec *elapsed_time)
{
	long double seconds;

	if (NULL == elapsed_time)
		return 0.0;

	seconds = (long double) elapsed_time->tv_sec;
	seconds += (long double) (elapsed_time->tv_nsec) / 1000000000.0L;

	return seconds;
}


/*
 * Sleep for a number of nanoseconds.
 */
void pv_nanosleep(long long nanoseconds)
{
#if HAVE_NANOSLEEP
	struct timespec sleep_for, time_remaining;

	memset(&sleep_for, 0, sizeof(sleep_for));
	memset(&time_remaining, 0, sizeof(time_remaining));

	sleep_for.tv_sec = 0;
	/*@-type@ */
	sleep_for.tv_nsec = nanoseconds;
	/*@+type@ *//* splint rationale - best effort */
	/*@-unrecog@ */
	(void) nanosleep(&sleep_for, &time_remaining);
	/*@+unrecog@ *//* splint rationale - doesn't know of nanosleep() */
#else
	struct timeval tv;
	tv.tv_sec = 0;
	/*@-type@ */
	tv.tv_usec = nanoseconds / 1000;
	/*@+type@ *//* splint rationale - best effort */
	/*@-null@ */
	(void) select(0, NULL, NULL, NULL, &tv);
	/*@+null@ *//* splint doesn't know about select() */
#endif
}
