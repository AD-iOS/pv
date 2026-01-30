/*
 * Formatter function for the elapsed transfer time.
 *
 * Copyright 2024-2025 Andrew Wood
 *
 * License GPLv3+: GNU GPL version 3 or later; see `docs/COPYING'.
 */

#include "config.h"
#include "pv.h"
#include "pv-internal.h"


/*
 * Elapsed time.
 */
pvdisplay_bytecount_t pv_formatter_timer(pvformatter_args_t args)
{
	char content[128];		 /* flawfinder: ignore - always bounded */
	long double elapsed_seconds;

	args->display->showing_timer = true;

	content[0] = '\0';

	if (0 == args->buffer_size)
		return 0;

	elapsed_seconds = args->transfer->elapsed_seconds;

	/*
	 * Bounds check, so we don't overrun the prefix buffer.  This does
	 * mean that the timer will stop at a 100,000 hours, but since
	 * that's 11 years, it shouldn't be a problem.
	 */
	if (elapsed_seconds > (long double) 360000000.0L)
		elapsed_seconds = (long double) 360000000.0L;

	/* Also check it's not negative. */
	if (elapsed_seconds < 0.0)
		elapsed_seconds = 0.0;

	if (args->control->numeric) {
		/* Numeric mode - show the number of seconds, unformatted. */
		(void) pv_snprintf(content, sizeof(content), "%.4Lf", elapsed_seconds);
	} else if (elapsed_seconds > (long double) 86400.0L) {
		/*
		 * If the elapsed time is more than a day, include a day count as
		 * well as hours, minutes, and seconds.
		 */
		(void) pv_snprintf(content,
				   sizeof(content),
				   "%ld:%02ld:%02ld:%02ld",
				   ((long) (elapsed_seconds)) / 86400,
				   (((long) (elapsed_seconds)) / 3600) %
				   24, (((long) (elapsed_seconds)) / 60) % 60, ((long) (elapsed_seconds)) % 60);
	} else {
		(void) pv_snprintf(content,
				   sizeof(content),
				   "%ld:%02ld:%02ld",
				   ((long) (elapsed_seconds)) / 3600,
				   (((long) (elapsed_seconds)) / 60) % 60, ((long) (elapsed_seconds)) % 60);
	}

	return pv_formatter_segmentcontent(content, args);
}
