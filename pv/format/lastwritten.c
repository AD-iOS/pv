/*
 * Formatter function for showing the last bytes written.
 *
 * Copyright 2024-2025 Andrew Wood
 *
 * License GPLv3+: GNU GPL version 3 or later; see `docs/COPYING'.
 */

#include "config.h"
#include "pv.h"
#include "pv-internal.h"


/*
 * Display the last few bytes written.
 *
 * As a side effect, this sets args->display->lastwritten_bytes to
 * the segment's chosen_size, if it was previously smaller than that.
 */
pvdisplay_bytecount_t pv_formatter_last_written(pvformatter_args_t args)
{
	pvdisplay_bytecount_t bytes_to_show, read_offset, write_offset, remaining;

	args->display->showing_last_written = true;

	bytes_to_show = args->segment->chosen_size;
	if (0 == bytes_to_show)
		bytes_to_show = args->segment->width;
	if (0 == bytes_to_show)
		return 0;

	if (bytes_to_show > PV_SIZEOF_LASTWRITTEN_BUFFER)
		bytes_to_show = PV_SIZEOF_LASTWRITTEN_BUFFER;
	if (bytes_to_show > args->display->lastwritten_bytes)
		args->display->lastwritten_bytes = bytes_to_show;

	if (0 == args->buffer_size)
		return 0;

	if (args->offset + bytes_to_show >= args->buffer_size)
		return 0;

	args->segment->offset = args->offset;
	args->segment->bytes = bytes_to_show;

	read_offset = args->display->lastwritten_bytes - bytes_to_show;
	write_offset = args->offset;
	for (remaining = bytes_to_show; remaining > 0; remaining--) {
		char display_char = args->display->lastwritten_buffer[read_offset++];
		args->buffer[write_offset++] = pv_isprint(display_char) ? display_char : '.';
	}

	return bytes_to_show;
}
