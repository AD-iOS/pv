/*
 * Formatter function for the current rate of transfer.
 *
 * Copyright 2024-2025 Andrew Wood
 *
 * License GPLv3+: GNU GPL version 3 or later; see `docs/COPYING'.
 */

#include "config.h"
#include "pv.h"
#include "pv-internal.h"


/*
 * Transfer rate.
 */
pvdisplay_bytecount_t pv_formatter_rate(pvformatter_args_t args)
{
	char content[128];		 /* flawfinder: ignore - always bounded */

	args->display->showing_rate = true;

	content[0] = '\0';

	if (0 == args->buffer_size)
		return 0;

	/*@-mustfreefresh@ */
	if (args->control->numeric) {
		/* numeric - raw value without suffix. */
		(void) pv_snprintf(content, sizeof(content),
				   "%.4Lf", ((args->control->bits ? 8.0 : 1.0) * args->calc->transfer_rate));
	} else if (args->control->bits && !args->control->linemode) {
		/* bits per second */
		pv_describe_amount(content, sizeof(content), "[%s]",
				   8 * args->calc->transfer_rate, "", _("b/s"), args->display->count_type);
	} else {
		/* bytes or lines per second */
		pv_describe_amount(content, sizeof(content),
				   "[%s]", args->calc->transfer_rate, _("/s"), _("B/s"), args->display->count_type);
	}
	/*@+mustfreefresh@ *//* splint: see above. */

	return pv_formatter_segmentcontent(content, args);
}
