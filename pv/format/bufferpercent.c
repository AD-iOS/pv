/*
 * Formatter function for transfer buffer percentage utilisation.
 *
 * Copyright 2024-2025 Andrew Wood
 *
 * License GPLv3+: GNU GPL version 3 or later; see `docs/COPYING'.
 */

#include "config.h"
#include "pv.h"
#include "pv-internal.h"


/*
 * Percentage transfer buffer utilisation.
 */
pvdisplay_bytecount_t pv_formatter_buffer_percent(pvformatter_args_t args)
{
	char content[16];		 /* flawfinder: ignore - always bounded */

	content[0] = '\0';

	if (0 == args->buffer_size)
		return 0;

	if (args->transfer->buffer_size > 0) {
		double pct_used = pv_percentage((off_t)
						(args->transfer->read_position - args->transfer->write_position),
						(off_t)
						(args->transfer->buffer_size));
		(void) pv_snprintf(content, sizeof(content), "{%3.0f%%}", pct_used);
	}
#ifdef HAVE_SPLICE
	if (args->transfer->splice_used)
		(void) pv_snprintf(content, sizeof(content), "{%s}", "----");
#endif

	return pv_formatter_segmentcontent(content, args);
}
