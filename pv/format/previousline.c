/*
 * Formatter function for the most recently written line.
 *
 * Copyright 2024-2025 Andrew Wood
 *
 * License GPLv3+: GNU GPL version 3 or later; see `docs/COPYING'.
 */

#include "config.h"
#include "pv.h"
#include "pv-internal.h"


/*
 * Display the previously written line.
 */
pvdisplay_bytecount_t pv_formatter_previous_line(pvformatter_args_t args)
{
	pvdisplay_bytecount_t bytes_to_show, read_offset, write_offset, remaining;

	args->display->showing_previous_line = true;

	if (0 == args->buffer_size)
		return 0;

	bytes_to_show = args->segment->chosen_size;
	if (0 == bytes_to_show)
		bytes_to_show = args->segment->width;
	if (0 == bytes_to_show)
		return 0;

	if (bytes_to_show > PV_SIZEOF_PREVLINE_BUFFER)
		bytes_to_show = PV_SIZEOF_PREVLINE_BUFFER;

	if (args->offset + bytes_to_show >= args->buffer_size)
		return 0;

	args->segment->offset = args->offset;
	args->segment->bytes = bytes_to_show;

	read_offset = 0;
	write_offset = args->offset;
	for (remaining = bytes_to_show; remaining > 0; remaining--) {
		char display_char = args->display->previous_line[read_offset++];
		args->buffer[write_offset++] = pv_isprint(display_char) ? display_char : ' ';
	}

	return bytes_to_show;
}
