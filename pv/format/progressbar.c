/*
 * Formatter functions for progress bars.
 *
 * Copyright 2024-2025 Andrew Wood
 *
 * License GPLv3+: GNU GPL version 3 or later; see `docs/COPYING'.
 */

#include "config.h"
#include "pv.h"
#include "pv-internal.h"

#include <string.h>

#if HAVE_MATH_H
#include <math.h>
#endif

/* Convenience macro for appending a string to the buffer. */
#define append_to_buffer(x) { \
  if (buffer_offset < (buffer_size - x.bytes)) { \
      memcpy(&(buffer[buffer_offset]), x.string, x.bytes); /* flawfinder: ignore */ \
      buffer_offset += x.bytes; \
  } \
}
/*
 * flawfinder - we are checking that there is room in the destination
 * buffer, given its size and our current offset.
 */


/*
 * Write a progress bar to a buffer, in known-size or rate-gauge mode - a
 * bar, and a percentage (size) or max rate (gauge).  The total width of the
 * content is bounded to the given width.  Returns the number of bytes
 * written to the buffer.
 *
 * If "bar_sides" is false, only the bar itself is rendered, not the opening
 * and closing characters.
 *
 * If "include_bar" is false, the bar is omitted entirely.
 *
 * If "include_amount" is false, the percentage or rate after the bar is
 * omitted.
 *
 * This is only called by pv_formatter_progress().
 */
static pvdisplay_bytecount_t pv_formatter_progress_knownsize(pvformatter_args_t args, char *buffer,
							     pvdisplay_bytecount_t buffer_size, bool bar_sides,
							     bool include_bar, bool include_amount)
{
	char after_bar[32];		 /* flawfinder: ignore - only populated by pv_snprintf(). */
	pvdisplay_bytecount_t after_bar_bytes, buffer_offset;
	pvdisplay_width_t after_bar_width;
	pvdisplay_width_t bar_area_width, filled_bar_width, pad_count;
	double bar_percentage;
	pvbarstyle_t style;
	pvdisplay_bytecount_t full_cell_index;
	bool has_tip = false;

	buffer[0] = '\0';

	if (args->segment->parameter > 0 && args->segment->parameter <= PV_BARSTYLE_MAX) {
		style = &(args->display->barstyle[args->segment->parameter - 1]);
	} else {
		style = &(args->display->barstyle[0]);
	}

	full_cell_index = style->filler_entries;
	if (full_cell_index > 0)
		full_cell_index--;
	if (1 == full_cell_index && style->tip.width > 0)
		has_tip = true;

	memset(after_bar, 0, sizeof(after_bar));

	if (args->control->size > 0) {
		/* Percentage of data transferred. */
		bar_percentage = (double) (args->calc->percentage);
		(void) pv_snprintf(after_bar, sizeof(after_bar), " %3ld%%", (int) bar_percentage);
	} else {
		/* Current rate vs max rate. */
		bar_percentage = 0.0;
		if (args->calc->rate_max > 0) {
			bar_percentage = (double) (100.0 * args->calc->transfer_rate / args->calc->rate_max);
		}

		/*@-mustfreefresh@ */
		if (args->control->bits && !args->control->linemode) {
			/* bits per second */
			pv_describe_amount(after_bar, sizeof(after_bar), "/%s",
					   8.0 * args->calc->rate_max, "", _("b/s"), args->display->count_type);
		} else {
			/* bytes or lines per second */
			pv_describe_amount(after_bar, sizeof(after_bar),
					   "/%s", args->calc->rate_max, _("/s"), _("B/s"), args->display->count_type);
		}
		/*@+mustfreefresh@ *//* splint: see above about gettext(). */
	}

	if (!include_amount)
		after_bar[0] = '\0';

	after_bar_bytes = strlen(after_bar);	/* flawfinder: ignore */
	/* flawfinder: always \0-terminated by pv_snprintf() and the earlier memset(). */
	after_bar_width = pv_strwidth(after_bar, after_bar_bytes);

	if (!include_bar) {
		/*
		 * Only returning the "after bar" portion - the amount
		 * (progress or max rate).
		 */
		if (buffer_size < after_bar_bytes)
			return 0;
		if (after_bar_bytes > 1) {
			/* NB we skip the leading space. */
			memmove(buffer, after_bar + 1, after_bar_bytes - 1);
			buffer[after_bar_bytes - 1] = '\0';
			return after_bar_bytes - 1;
		}
		return 0;
	}

	if (bar_sides) {
		if (args->segment->width < (after_bar_width + 2))
			return 0;
		bar_area_width = args->segment->width - after_bar_width - 2;
	} else {
		if (args->segment->width < after_bar_width)
			return 0;
		bar_area_width = args->segment->width - after_bar_width;
	}

	filled_bar_width = (pvdisplay_width_t) ((bar_area_width * bar_percentage) / 100);
	/* Leave room for the tip of the bar. */
	if (has_tip && filled_bar_width > 0)
		filled_bar_width -= style->tip.width;

	debug("percentage=%.2f width=%d bar_area_width=%d filled_bar_width=%d after_bar_width=%d", bar_percentage,
	      args->segment->width, bar_area_width, filled_bar_width, after_bar_width);

	buffer_offset = 0;

	if (bar_sides) {
		/* The opening of the bar area. */
		buffer[buffer_offset++] = '[';
	}

	/* The bar portion. */
	pad_count = 0;
	while (pad_count < filled_bar_width && pad_count < bar_area_width) {
		append_to_buffer(style->filler[full_cell_index]);
		pad_count += style->filler[full_cell_index].width;
		if (0 == style->filler[full_cell_index].width)
			pad_count++;
	}

	/* The tip of the bar, if not at 100%. */
	if (has_tip && pad_count < bar_area_width) {
		append_to_buffer(style->tip);
		pad_count += style->tip.width;
	}

	/* A partial cell, if there are intermediates and we're not at 100%. */
	if (pad_count < bar_area_width && full_cell_index > 1 && !has_tip) {
		double exact_width = (((double) bar_area_width) * bar_percentage) / 100.0;
		double cell_portion = exact_width - (double) filled_bar_width;
		double cell_index_double = ((double) full_cell_index) * cell_portion;
		size_t cell_index = (size_t) cell_index_double;

		if (cell_index > full_cell_index)
			cell_index = full_cell_index;

		append_to_buffer(style->filler[cell_index]);
		pad_count += style->filler[cell_index].width;
		if (0 == style->filler[cell_index].width)
			pad_count++;
	}

	/* The spaces after the bar. */
	while (pad_count < bar_area_width) {
		append_to_buffer(style->filler[0]);
		pad_count += style->filler[0].width;
		if (0 == style->filler[0].width)
			pad_count++;
	}

	if (bar_sides) {
		/* The closure of the bar area. */
		if (buffer_offset < buffer_size - 1)
			buffer[buffer_offset++] = ']';
	}

	/* The percentage. */
	if (after_bar_bytes > 0 && after_bar_bytes < (buffer_size - 1 - buffer_offset)) {
		memmove(buffer + buffer_offset, after_bar, after_bar_bytes);
		buffer_offset += after_bar_bytes;
	}

	buffer[buffer_offset] = '\0';

	return buffer_offset;
}


/*
 * Write a progress bar to a buffer, in unknown-size mode - just a moving
 * indicator.  The total width of the content is bounded to the given width. 
 * Returns the number of bytes written to the buffer.
 *
 * If "bar_sides" is false, only the bar itself is rendered, not the opening
 * and closing characters.
 *
 * This is only called by pv_formatter_progress().
 */
static pvdisplay_bytecount_t pv_formatter_progress_unknownsize(pvformatter_args_t args, char *buffer,
							       pvdisplay_bytecount_t buffer_size, bool bar_sides)
{
	pvdisplay_bytecount_t buffer_offset;
	pvdisplay_width_t bar_area_width, pad_count;
	double indicator_position, padding_width;
	pvbarstyle_t style;

	buffer[0] = '\0';

	if (args->segment->parameter > 0 && args->segment->parameter <= PV_BARSTYLE_MAX) {
		style = &(args->display->barstyle[args->segment->parameter - 1]);
	} else {
		style = &(args->display->barstyle[0]);
	}

	if (bar_sides) {
		if (args->segment->width < (style->indicator.width + 3))
			return 0;
		bar_area_width = args->segment->width - (style->indicator.width + 2);
	} else {
		if (args->segment->width < (style->indicator.width + 2))
			return 0;
		bar_area_width = args->segment->width - style->indicator.width;
	}

	/*
	 * Note that pv_calculate_transfer_rate() sets the percentage when
	 * the size is unknown to a value that goes 0 - 200 and resets, so
	 * here we make values above 100 send the indicator back down again,
	 * so it moves back and forth.
	 */
	indicator_position = args->calc->percentage;
	if (indicator_position > 200.0)
#if HAVE_FMOD
		indicator_position = fmod(indicator_position, 200.0);
#else
	{
		while (indicator_position > 200.0)
			indicator_position -= 200.0;
	}
#endif
	if (indicator_position > 100.0)
		indicator_position = 200.0 - indicator_position;
	if (indicator_position < 0.0)
		indicator_position = 0.0;

	buffer_offset = 0;

	if (bar_sides) {
		/* The opening of the bar area. */
		buffer[buffer_offset++] = '[';
	}

	/* The spaces before the indicator. */
	pad_count = 0;
	padding_width = (((double) bar_area_width) * indicator_position) / 100.0;
	while (pad_count < bar_area_width && pad_count < (pvdisplay_width_t) padding_width) {
		append_to_buffer(style->filler[0]);
		pad_count += style->filler[0].width;
		if (0 == style->filler[0].width)
			pad_count++;
	}

	/* The indicator. */
	if (buffer_offset < buffer_size - style->indicator.bytes) {
		append_to_buffer(style->indicator);
	}

	/* The spaces after the indicator. */
	while (pad_count < bar_area_width) {
		append_to_buffer(style->filler[0]);
		pad_count += style->filler[0].width;
		if (0 == style->filler[0].width)
			pad_count++;
	}

	if (bar_sides) {
		/* The closure of the bar area. */
		if (buffer_offset < buffer_size - 1)
			buffer[buffer_offset++] = ']';
	}

	buffer[buffer_offset] = '\0';

	return buffer_offset;
}


/*
 * Progress bar.
 */
pvdisplay_bytecount_t pv_formatter_progress(pvformatter_args_t args)
{
	char content[4096];		 /* flawfinder: ignore - always bounded */
	pvdisplay_bytecount_t bytes;

	content[0] = '\0';

	if (0 == args->segment->parameter) {
		const char *default_name;
		default_name = args->control->default_bar_style;
		/*@-branchstate@ */
		if (NULL == default_name)
			default_name = "plain";
		/*@+branchstate@ */
		/* splint - it doesn't matter that default_name may be static */
		args->segment->parameter = 1 + pv_display_barstyle_index(args, default_name);
	}

	if (0 == args->buffer_size)
		return 0;

	if (args->control->size > 0 || args->control->rate_gauge) {
		/* Known size or rate gauge - bar with percentage. */
		bytes = pv_formatter_progress_knownsize(args, content, sizeof(content), true, true, true);
	} else {
		/* Unknown size - back-and-forth moving indicator. */
		bytes = pv_formatter_progress_unknownsize(args, content, sizeof(content), true);
	}

	content[bytes] = '\0';

	return pv_formatter_segmentcontent(content, args);
}


/*
 * Progress bar, without sides and without a number afterwards.
 */
pvdisplay_bytecount_t pv_formatter_progress_bar_only(pvformatter_args_t args)
{
	char content[4096];		 /* flawfinder: ignore - always bounded */
	pvdisplay_bytecount_t bytes;

	content[0] = '\0';

	if (0 == args->segment->parameter) {
		const char *default_name;
		default_name = args->control->default_bar_style;
		/*@-branchstate@ */
		if (NULL == default_name)
			default_name = "plain";
		/*@+branchstate@ */
		/* splint - it doesn't matter that default_name may be static */
		args->segment->parameter = 1 + pv_display_barstyle_index(args, default_name);
	}

	if (0 == args->buffer_size)
		return 0;

	if (args->control->size > 0 || args->control->rate_gauge) {
		/* Known size or rate gauge - bar with percentage. */
		bytes = pv_formatter_progress_knownsize(args, content, sizeof(content), false, true, false);
	} else {
		/* Unknown size - back-and-forth moving indicator. */
		bytes = pv_formatter_progress_unknownsize(args, content, sizeof(content), false);
	}

	content[bytes] = '\0';

	return pv_formatter_segmentcontent(content, args);
}


/*
 * The number after the progress bar.
 */
pvdisplay_bytecount_t pv_formatter_progress_amount_only(pvformatter_args_t args)
{
	char content[256];		 /* flawfinder: ignore - always bounded */
	pvdisplay_bytecount_t bytes;

	memset(content, 0, sizeof(content));

	if (0 == args->buffer_size)
		return 0;

	if (args->control->numeric) {
		/* Numeric mode - percentage as a rounded integer with no suffix. */
		(void) pv_snprintf(content, sizeof(content), "%.0f", args->calc->percentage);
		bytes = strlen(content);    /* flawfinder: ignore */
		/* flawfinder: always \0-terminated by pv_snprintf() and the earlier memset(). */
	} else if (args->control->size > 0 || args->control->rate_gauge) {
		/* Known size or rate gauge - percentage or rate. */
		bytes = pv_formatter_progress_knownsize(args, content, sizeof(content), false, false, true);
	} else {
		/* Unknown size - no number. */
		return 0;
	}

	content[bytes] = '\0';

	return pv_formatter_segmentcontent(content, args);
}
