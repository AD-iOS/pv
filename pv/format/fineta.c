/*
 * Formatter function for the local time at which the transfer is expected
 * to complete.
 *
 * Copyright 2024-2025 Andrew Wood
 *
 * License GPLv3+: GNU GPL version 3 or later; see `docs/COPYING'.
 */

#include "config.h"
#include "pv.h"
#include "pv-internal.h"

#include <string.h>
#include <time.h>


/*
 * Estimated local time of completion.
 */
pvdisplay_bytecount_t pv_formatter_fineta(pvformatter_args_t args)
{
	char content[128];		 /* flawfinder: ignore - always bounded */
	time_t now, then;
	struct tm *time_ptr;
	long eta;
	const char *time_format;
	bool show_fineta;

	content[0] = '\0';

	/*
	 * Don't try to calculate this if the size is not known.
	 */
	if (args->control->size < 1)
		return 0;

	if (0 == args->buffer_size)
		return 0;

	now = time(NULL);
	show_fineta = true;
	time_format = NULL;

	/*
	 * The completion clock time may be hidden by a failed localtime
	 * lookup.
	 */

	eta = pv_seconds_remaining(args->transfer->transferred - args->display->initial_offset,
				   args->control->size - args->display->initial_offset, args->calc->current_avg_rate);

	/* Bounds check - see pv_formatter_eta(). */
	eta = pv_bound_long(eta, 0, (long) 360000000L);

	/*
	 * Only include the date if the ETA is more than 6 hours
	 * away.
	 */
	if (eta > (long) (6 * 3600)) {
		time_format = "%Y-%m-%d %H:%M:%S";
	} else {
		time_format = "%H:%M:%S";
	}

	then = now + eta;
	time_ptr = localtime(&then);

	if (NULL == time_ptr) {
		show_fineta = false;
	} else {
		/*
		 * The localtime() function keeps data stored in a static
		 * buffer that gets overwritten by time functions.
		 */
		struct tm time = *time_ptr;
		size_t content_bytes;

		/*@-mustfreefresh@ */
		(void) pv_snprintf(content, sizeof(content), "%.16s ", _("FIN"));
		/*@+mustfreefresh@ *//* splint: see above. */
		content_bytes = strlen(content);	/* flawfinder: ignore */
		/* flawfinder: always bounded with \0 by pv_snprintf(). */
		(void) strftime(content + content_bytes, sizeof(content) - 1 - content_bytes, time_format, &time);
	}

	if (!show_fineta) {
		size_t erase_idx;
		for (erase_idx = 0; erase_idx < sizeof(content) && content[erase_idx] != '\0'; erase_idx++) {
			content[erase_idx] = ' ';
		}
	}

	return pv_formatter_segmentcontent(content, args);
}
