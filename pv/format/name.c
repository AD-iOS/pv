/*
 * Formatter function for showing the name of the transfer.
 *
 * Copyright 2024-2025 Andrew Wood
 *
 * License GPLv3+: GNU GPL version 3 or later; see `docs/COPYING'.
 */

#include "config.h"
#include "pv.h"
#include "pv-internal.h"

#include <string.h>


/*
 * Display the transfer's name.
 */
pvdisplay_bytecount_t pv_formatter_name(pvformatter_args_t args)
{
	char string_format[32];		 /* flawfinder: ignore - always bounded */
	char content[512];		 /* flawfinder: ignore - always bounded */
	pvdisplay_bytecount_t field_width;

	if (0 == args->buffer_size)
		return 0;

	field_width = args->segment->chosen_size;
	if (field_width < 1)
		field_width = 9;
	if (field_width > 500)
		field_width = 500;

	memset(string_format, 0, sizeof(string_format));
	(void) pv_snprintf(string_format, sizeof(string_format), "%%%d.500s:", field_width);

	content[0] = '\0';
	if (NULL != args->control->name) {
		(void) pv_snprintf(content, sizeof(content), string_format, args->control->name);
	}

	return pv_formatter_segmentcontent(content, args);
}
