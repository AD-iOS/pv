/*
 * Formatter function for bytes or lines transferred.
 *
 * Copyright 2024-2025 Andrew Wood
 *
 * License GPLv3+: GNU GPL version 3 or later; see `docs/COPYING'.
 */

#include "config.h"
#include "pv.h"
#include "pv-internal.h"


/*
 * Number of bytes or lines transferred.
 */
pvdisplay_bytecount_t pv_formatter_bytes(pvformatter_args_t args)
{
	char content[128];		 /* flawfinder: ignore - always bounded */

	args->display->showing_bytes = true;

	if (0 == args->buffer_size)
		return 0;

	content[0] = '\0';

	/*@-mustfreefresh@ */
	if (args->control->numeric) {
		/* Numeric mode - raw values only, no suffix. */
		(void) pv_snprintf(content, sizeof(content),
				   "%lld", (long long) ((args->control->bits ? 8 : 1) * args->transfer->transferred));
	} else if (args->control->bits && !args->control->linemode) {
		pv_describe_amount(content, sizeof(content), "%s",
				   (long double) (args->transfer->transferred * 8), "", _("b"),
				   args->display->count_type);
	} else {
		pv_describe_amount(content, sizeof(content), "%s",
				   (long double) (args->transfer->transferred), "", _("B"), args->display->count_type);
	}
	/*@+mustfreefresh@ *//* splint - false positive from gettext(). */

	return pv_formatter_segmentcontent(content, args);
}
