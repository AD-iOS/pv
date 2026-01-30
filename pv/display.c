/*
 * Display functions.
 *
 * Copyright 2002-2008, 2010, 2012-2015, 2017, 2021, 2023-2025 Andrew Wood
 *
 * License GPLv3+: GNU GPL version 3 or later; see `docs/COPYING'.
 */

#include "config.h"
#include "pv.h"
#include "pv-internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#ifdef ENABLE_NCURSES
#ifdef HAVE_TERM_H
#include <term.h>
#endif
#endif

/*
 * If USE_POPEN_TPUTS is defined, then we call popen("tputs") if ncurses is
 * unavailable, when we need to know whether colours are supported.
 *
 * Since popen() is risky to call (in case $PATH has been altered), it is
 * safer to just assume colour is available if we can't check, so
 * USE_POPEN_TPUTS is undefined here by default.
 */
#undef USE_POPEN_TPUTS


/*
 * We need sys/ioctl.h for ioctl() regardless of whether TIOCGWINSZ is
 * defined in termios.h, so we no longer use AC_HEADER_TIOCGWINSZ in
 * configure.in, and just include both header files if they are available.
 * (GH#74, 2023-08-06)
 */
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

/*
 * The error prefix for messages (the program name); whether it has been
 * set; and whether any output has been displayed yet, indicating whether
 * any errors must be preceded by a newline.
 */
static char pv__error_prefix[64];	 /* flawfinder: ignore */
static bool pv__error_prefix_set = false;
static bool pv__output_produced = false;

/*
 * flawfinder rationale: zeroed before use, string copy is bounded to 1 less
 * than size so it always has \0 termination.  Not used unless initialised,
 * by checking pv__error_prefix_set.
 */

/*
 * Set the error message prefix.
 */
void pv_set_error_prefix( /*@unique@ */ const char *prefix)
{
	if (NULL == prefix)
		return;
	memset(pv__error_prefix, 0, sizeof(pv__error_prefix));
	strncpy(pv__error_prefix, prefix, sizeof(pv__error_prefix) - 1);	/* flawfinder: ignore */
	pv__error_prefix_set = true;
	/*
	 * flawfinder rationale: strncpy's pointers are as valid as we can
	 * make them since the first is a static buffer and the second is
	 * caller-supplied.  The caller must \0-terminate the string but in
	 * any case it is bounded to 1 less than the size of the
	 * destination.  The destination is zeroed before use so the result
	 * is guaranteed to be \0-terminated.
	 */
}

/*
 * Output an error message.  If we've displayed anything to the terminal
 * already, then put a newline before our error so we don't write over what
 * we've written.
 */
void pv_error(char *format, ...)
{
	va_list ap;
	if (pv__output_produced)
		fprintf(stderr, "\n");
	if (pv__error_prefix_set)
		fprintf(stderr, "%s: ", pv__error_prefix);
	va_start(ap, format);
	(void) vfprintf(stderr, format, ap);	/* flawfinder: ignore */
	va_end(ap);
	fprintf(stderr, "\n");
	/*
	 * flawfinder: this function relies on callers always having a
	 * static format string, not directly subject to outside influences.
	 */
}


/*
 * Return true if we are the foreground process on the terminal, or if we
 * aren't outputting to a terminal; false otherwise.
 */
bool pv_in_foreground(void)
{
	pid_t our_process_group;
	pid_t tty_process_group;

	if (0 == isatty(STDERR_FILENO)) {
		debug("true: %s", "not a tty");
		return true;
	}

	/*@-type@ *//* __pid_t vs pid_t, not significant */
	our_process_group = getpgrp();
	tty_process_group = tcgetpgrp(STDERR_FILENO);
	/*@+type@ */

	if (tty_process_group == -1 && errno == ENOTTY) {
		debug("true: %s", "tty_process_group is -1, errno is ENOTTY");
		return true;
	}

	if (our_process_group == tty_process_group) {
		debug("true: %s (%d)", "our_process_group == tty_process_group", our_process_group);
		return true;
	}

	/*
	 * If the terminal process group ID doesn't match our own, assume
	 * we're in the background.
	 */
	debug("false: our_process_group=%d, tty_process_group=%d", our_process_group, tty_process_group);

	return false;
}


/*
 * Write the given buffer to the given file descriptor, retrying until all
 * bytes have been written or an error has occurred.
 */
void pv_write_retry(int fd, const char *buf, size_t count)
{
	while (count > 0) {
		ssize_t nwritten;

		nwritten = write(fd, buf, count);

		if (nwritten < 0) {
			if ((EINTR == errno) || (EAGAIN == errno)) {
				continue;
			}
			return;
		}
		if (nwritten < 1)
			return;

		count -= nwritten;
		buf += nwritten;
	}
}


/*
 * Write the given buffer to the terminal, like pv_write_retry(), unless
 * stderr is suspended.
 */
void pv_tty_write(readonly_pvtransientflags_t flags, const char *buf, size_t count)
{
	while (0 == flags->suspend_stderr && count > 0) {
		ssize_t nwritten;

		nwritten = write(STDERR_FILENO, buf, count);

		if (nwritten < 0) {
			if ((EINTR == errno) || (EAGAIN == errno)) {
				continue;
			}
			return;
		}
		if (nwritten < 1)
			return;

		count -= nwritten;
		buf += nwritten;
	}
}


/*
 * Fill in *width and *height with the current terminal size,
 * if possible.
 */
void pv_screensize(unsigned int *width, unsigned int *height)
{
#ifdef TIOCGWINSZ
	struct winsize wsz;

	memset(&wsz, 0, sizeof(wsz));

	if (0 != isatty(STDERR_FILENO)) {
		if (0 == ioctl(STDERR_FILENO, TIOCGWINSZ, &wsz)) {
			*width = wsz.ws_col;
			*height = wsz.ws_row;
		}
	}
#endif
}


/*
 * Return the original value x so that it has been clamped between
 * [min..max]
 */
long pv_bound_long(long x, long min, long max)
{
	return x < min ? min : x > max ? max : x;
}


/*
 * Given how many bytes have been transferred, the total byte count to
 * transfer, and the current average transfer rate, return the estimated
 * number of seconds until completion.
 */
long pv_seconds_remaining(const off_t so_far, const off_t total, const long double rate)
{
	long double amount_left;

	if ((so_far < 1) || (rate < 0.001))
		return 0;

	amount_left = (long double) (total - so_far) / rate;

	return (long) amount_left;
}

/*
 * Given a long double value, it is divided or multiplied by the ratio until
 * a value in the range 1.0 to 999.999... is found.  The string "prefix" to
 * is updated to the corresponding SI prefix.
 *
 * If the count type is PV_TRANSFERCOUNT_BYTES, then the second byte of
 * "prefix" is set to "i" to denote MiB etc (IEEE1541).  Thus "prefix"
 * should be at least 3 bytes long (to include the terminating null).
 */
void pv_si_prefix(long double *value, char *prefix, const long double ratio, pvtransfercount_t count_type)
{
	static char *pfx_000 = NULL;	 /* kilo, mega, etc */
	static char *pfx_024 = NULL;	 /* kibi, mibi, etc */
	static char const *pfx_middle_000 = NULL;
	static char const *pfx_middle_024 = NULL;
	char *pfx;
	char const *pfx_middle;
	char const *pfx_ptr;
	long double cutoff;

	prefix[0] = ' ';		    /* Make the prefix start blank. */
	prefix[1] = '\0';

	/*
	 * The prefix list strings have a space (no prefix) in the middle;
	 * moving right from the space gives the prefix letter for each
	 * increasing multiple of 1000 or 1024 - such as kilo, mega, giga -
	 * and moving left from the space gives the prefix letter for each
	 * decreasing multiple - such as milli, micro, nano.
	 */

	/*
	 * Prefix list for multiples of 1000.
	 */
	if (NULL == pfx_000) {
		/*@-onlytrans@ */
		pfx_000 = _("yzafpnum kMGTPEZY");
		/*
		 * splint: this is only looked up once in the program's run,
		 * so the memory leak is negligible.
		 */
		/*@+onlytrans@ */
		if (NULL == pfx_000) {
			debug("%s", "prefix list was NULL");
			return;
		}
		pfx_middle_000 = strchr(pfx_000, ' ');
	}

	/*
	 * Prefix list for multiples of 1024.
	 */
	if (NULL == pfx_024) {
		/*@-onlytrans@ */
		pfx_024 = _("yzafpnum KMGTPEZY");
		/*@+onlytrans@ *//* splint: see above. */
		if (NULL == pfx_024) {
			debug("%s", "prefix list was NULL");
			return;
		}
		pfx_middle_024 = strchr(pfx_024, ' ');
	}

	pfx = pfx_000;
	pfx_middle = pfx_middle_000;
	if (count_type == PV_TRANSFERCOUNT_BYTES) {
		/* bytes - multiples of 1024 */
		pfx = pfx_024;
		pfx_middle = pfx_middle_024;
	}

	pfx_ptr = pfx_middle;
	if (NULL == pfx_ptr) {
		debug("%s", "prefix middle was NULL");
		return;
	}

	/*
	 * Force an empty prefix if the value is almost zero, to avoid
	 * "0yB".  NB we don't compare directly with zero because of
	 * potential floating-point inaccuracies.
	 *
	 * See the "count_type" check below for the reason we add another
	 * space in bytes mode.
	 */
	if ((*value > -0.00000001) && (*value < 0.00000001)) {
		if (count_type == PV_TRANSFERCOUNT_BYTES) {
			prefix[1] = ' ';
			prefix[2] = '\0';
		}
		return;
	}

	/*
	 * Cut-off for moving to the next prefix - a little less than the
	 * ratio (970 for ratio=1000, 993 for ratio=1024).
	 */
	cutoff = ratio * 0.97;

	/*
	 * Divide by the ratio until the value is a little below the ratio,
	 * moving along the prefix list with each division to get the
	 * associated prefix letter, so that for example 20000 becomes 20
	 * with a "k" (kilo) prefix.
	 */

	if (*value > 0) {
		/* Positive values */

		while ((*value > cutoff) && (*(pfx_ptr += 1) != '\0')) {
			*value /= ratio;
			prefix[0] = *pfx_ptr;
		}
	} else {
		/* Negative values */

		cutoff = 0 - cutoff;
		while ((*value < cutoff) && (*(pfx_ptr += 1) != '\0')) {
			*value /= ratio;
			prefix[0] = *pfx_ptr;
		}
	}

	/*
	 * Multiply by the ratio until the value is at least 1, moving in
	 * the other direction along the prefix list to get the associated
	 * prefix letter - so for example a value of 0.5 becomes 500 with a
	 * "m" (milli) prefix.
	 */

	if (*value > 0) {
		/* Positive values */
		while ((*value < 1.0) && ((pfx_ptr -= 1) != (pfx - 1))) {
			*value *= ratio;
			prefix[0] = *pfx_ptr;
		}
	} else {
		/* Negative values */
		while ((*value > -1.0) && ((pfx_ptr -= 1) != (pfx - 1))) {
			*value *= ratio;
			prefix[0] = *pfx_ptr;
		}
	}

	/*
	 * Byte prefixes (kibi, mebi, etc) are of the form "KiB" rather than
	 * "KB", so that's two characters, not one - meaning that for just
	 * "B", the prefix is two spaces, not one.
	 */
	if (count_type == PV_TRANSFERCOUNT_BYTES) {
		prefix[1] = (prefix[0] == ' ' ? ' ' : 'i');
		prefix[2] = '\0';
	}
}


/*
 * Put a string in "buffer" (max length "bufsize") containing "amount"
 * formatted such that it's 3 or 4 digits followed by an SI suffix and then
 * whichever of "suffix_basic" or "suffix_bytes" is appropriate (whether
 * "count_type" is PV_TRANSFERTYPE_LINES for non-byte amounts or
 * PV_TRANSFERTYPE_BYTES for byte amounts).  If "count_type" is
 * PV_TRANSFERTYPE_BYTES then the SI units are KiB, MiB etc and the divisor
 * is 1024 instead of 1000.
 *
 * The "format" string is in sprintf format and must contain exactly one %
 * parameter (a %s) which will expand to the string described above.
 */
void pv_describe_amount(char *buffer, size_t bufsize, char *format,
			long double amount, char *suffix_basic, char *suffix_bytes, pvtransfercount_t count_type)
{
	char sizestr_buffer[256];	 /* flawfinder: ignore */
	char si_prefix[8];		 /* flawfinder: ignore */
	long double divider;
	long double display_amount;
	char *suffix;

	/*
	 * flawfinder: sizestr_buffer and si_prefix are explicitly zeroed;
	 * sizestr_buffer is only ever used with pv_snprintf() along with
	 * its buffer size; si_prefix is only populated by pv_snprintf()
	 * along with its size, and by pv_si_prefix() which explicitly only
	 * needs 3 bytes.
	 */

	memset(sizestr_buffer, 0, sizeof(sizestr_buffer));
	memset(si_prefix, 0, sizeof(si_prefix));

	(void) pv_snprintf(si_prefix, sizeof(si_prefix), "%s", "  ");

	if (count_type == PV_TRANSFERCOUNT_BYTES) {
		suffix = suffix_bytes;
		divider = 1024.0;
	} else if (count_type == PV_TRANSFERCOUNT_DECBYTES) {
		suffix = suffix_bytes;
		divider = 1000.0;
	} else {
		suffix = suffix_basic;
		divider = 1000.0;
	}

	display_amount = amount;

	pv_si_prefix(&display_amount, si_prefix, divider, count_type);

	/* Make sure we don't overrun our buffer. */
	if (display_amount > 100000)
		display_amount = 100000;
	if (display_amount < -100000)
		display_amount = -100000;

	/* Fix for display of "1.01e+03" instead of "1010" */
	if ((display_amount > 99.9) || (display_amount < -99.9)) {
		(void) pv_snprintf(sizestr_buffer, sizeof(sizestr_buffer),
				   "%4ld%.2s%.16s", (long) display_amount, si_prefix, suffix);
	} else {
		/*
		 * AIX blows up with %4.3Lg%.2s%.16s for some reason, so we
		 * write display_amount separately first.
		 */
		char str_disp[64];	 /* flawfinder: ignore - only used with pv_snprintf(). */
		memset(str_disp, 0, sizeof(str_disp));
		/* # to get 13.0GB instead of 13GB (#1477) */
		(void) pv_snprintf(str_disp, sizeof(str_disp), "%#4.3Lg", display_amount);
		(void) pv_snprintf(sizestr_buffer, sizeof(sizestr_buffer), "%s%.2s%.16s", str_disp, si_prefix, suffix);
	}

	(void) pv_snprintf(buffer, bufsize, format, sizestr_buffer);
}


/*
 * Add a null-terminated string to the buffer if there is room for it,
 * updating the segment's offset and bytes values and returning the bytes
 * value, or treating the byte count as zero if there's insufficient space.
 */
pvdisplay_bytecount_t pv_formatter_segmentcontent(char *content, pvformatter_args_t formatter_info)
{
	pvdisplay_bytecount_t bytes;

	bytes = strlen(content);	    /* flawfinder: ignore */
	/* flawfinder - caller is required to null-terminate the string. */

	if (formatter_info->offset >= formatter_info->buffer_size)
		bytes = 0;
	if ((formatter_info->offset + bytes) >= formatter_info->buffer_size)
		bytes = 0;

	formatter_info->segment->offset = formatter_info->offset;
	formatter_info->segment->bytes = bytes;

	if (0 == bytes)
		return 0;

	memmove(formatter_info->buffer + formatter_info->offset, content, bytes);

	return bytes;
}


/*
 * Format sequence lookup table.
 */
/*@keep@ */ static struct pvdisplay_component_s *pv__format_components(void)
{
	/*@keep@ */ static struct pvdisplay_component_s format_component_array[] = {
		{ "p", &pv_formatter_progress, true },
		{ "{progress}", &pv_formatter_progress, true },
		{ "{progress-amount-only}", &pv_formatter_progress_amount_only, false },
		{ "{progress-bar-only}", &pv_formatter_bar_default, true },
		{ "{bar-plain}", &pv_formatter_bar_plain, true },
		{ "{bar-block}", &pv_formatter_bar_block, true },
		{ "{bar-granular}", &pv_formatter_bar_granular, true },
		{ "{bar-shaded}", &pv_formatter_bar_shaded, true },
		{ "t", &pv_formatter_timer, false },
		{ "{timer}", &pv_formatter_timer, false },
		{ "e", &pv_formatter_eta, false },
		{ "{eta}", &pv_formatter_eta, false },
		{ "I", &pv_formatter_fineta, false },
		{ "{fineta}", &pv_formatter_fineta, false },
		{ "r", &pv_formatter_rate, false },
		{ "{rate}", &pv_formatter_rate, false },
		{ "a", &pv_formatter_average_rate, false },
		{ "{average-rate}", &pv_formatter_average_rate, false },
		{ "b", &pv_formatter_bytes, false },
		{ "{bytes}", &pv_formatter_bytes, false },
		{ "{transferred}", &pv_formatter_bytes, false },
		{ "T", &pv_formatter_buffer_percent, false },
		{ "{buffer-percent}", &pv_formatter_buffer_percent, false },
		{ "A", &pv_formatter_last_written, false },
		{ "{last-written}", &pv_formatter_last_written, false },
		{ "L", &pv_formatter_previous_line, true },
		{ "{previous-line}", &pv_formatter_previous_line, true },
		{ "N", &pv_formatter_name, false },
		{ "{name}", &pv_formatter_name, false },
		{ "{sgr:colour,...}", &pv_formatter_sgr, false },
		{ NULL, NULL, false }
	};
	return format_component_array;
}


/*
 * Return a pointer to a malloc()ed string containing a space-separated list
 * of all supported format sequences.  The caller should free() it.
 */
/*@null@ */
char *pv_format_sequences(void)
{
	size_t component_idx, buffer_size, offset;
	struct pvdisplay_component_s *format_component_array;
	char *buffer;

	format_component_array = pv__format_components();

	buffer_size = 0;
	for (component_idx = 0; NULL != format_component_array[component_idx].match; component_idx++) {
		size_t component_sequence_length = strlen(format_component_array[component_idx].match);	/* flawfinder: ignore */
		/* flawfinder - static strings, guaranteed null-terminated. */
		buffer_size += 2 + component_sequence_length;	/* 2 for '%' + ' ' */
	}

	buffer = malloc(buffer_size + 1);
	if (NULL == buffer)
		return NULL;

	offset = 0;
	for (component_idx = 0; NULL != format_component_array[component_idx].match; component_idx++) {
		size_t component_sequence_length = strlen(format_component_array[component_idx].match);	/* flawfinder: ignore - as above */
		if (0 != offset)
			buffer[offset++] = ' ';
		buffer[offset++] = '%';
		memmove(buffer + offset, format_component_array[component_idx].match, component_sequence_length);
		offset += component_sequence_length;
	}

	buffer[offset] = '\0';
	return buffer;
}


/*
 * Initialise the output format structure, based on the current options.
 *
 * May update status->checked_colour_support and
 * status->terminal_supports_colour.
 */
static void pv__format_init(pvprogramstatus_t status, readonly_pvcontrol_t control, readonly_pvtransferstate_t transfer,
			    readonly_pvtransfercalc_t calc,
			    /*@null@ */ const char *format_supplied, pvdisplay_t display)
{
	struct pvdisplay_component_s *format_component_array;
	const char *display_format;
	size_t strpos;
	size_t segment;

	if (NULL == status)
		return;
	if (NULL == control)
		return;
	if (NULL == transfer)
		return;
	if (NULL == calc)
		return;
	if (NULL == display)
		return;

	format_component_array = pv__format_components();

	display->format_segment_count = 0;
	memset(display->format, 0, PV_FORMAT_ARRAY_MAX * sizeof(display->format[0]));

	display->showing_timer = false;
	display->showing_bytes = false;
	display->showing_rate = false;
	display->showing_last_written = false;
	display->showing_previous_line = false;
	display->format_uses_colour = false;

	display_format = NULL == format_supplied ? control->default_format : format_supplied;

	if (NULL == display_format)
		return;

	/*
	 * Split the format string into static strings and calculated
	 * components - a calculated component is is what replaces a
	 * placeholder sequence like "%b".
	 *
	 * A "static string" is part of the original format string that is
	 * copied to the display verbatim.  Its width is calculated here.
	 *
	 * Each segment's contents are stored in either the format string
	 * (if a static string) or an internal temporary buffer, starting at
	 * "offset" and extending for "bytes" bytes.
	 *
	 * Later, in pv_format(), segments whose components are dynamic and
	 * which aren't constrained to a fixed size are calculated after
	 * first populating all the other components referenced by the
	 * format segments.
	 *
	 * Then, that function generates the output string by sticking all
	 * of these segments together.
	 */
	segment = 0;
	for (strpos = 0; display_format[strpos] != '\0' && segment < PV_FORMAT_ARRAY_MAX; strpos++, segment++) {
		pvdisplay_component_t component_type, component_idx;
		size_t str_start, str_bytes, chosen_size;
		const char *string_parameter = NULL;
		size_t string_parameter_bytes = 0;

		str_start = strpos;
		str_bytes = 0;

		chosen_size = 0;

		if ('%' == display_format[strpos]) {
			unsigned long number_prefix;
			size_t percent_sign_offset, sequence_start, sequence_length, sequence_colon_offset;
#if HAVE_STRTOUL
			char *number_end_ptr;
#endif

			percent_sign_offset = strpos;
			strpos++;

			/*
			 * Check for a numeric prefix between the % and the
			 * format character - currently only used with "%A"
			 * and "%L".
			 */
#if HAVE_STRTOUL
			number_end_ptr = NULL;
			number_prefix = strtoul(&(display_format[strpos]), &number_end_ptr, 10);
			if ((NULL == number_end_ptr) || (number_end_ptr[0] == '\0')) {
				number_prefix = 0;
			} else if (number_end_ptr > &(display_format[strpos])) {
				strpos += (number_end_ptr - &(display_format[strpos]));
			}
#else				/* !HAVE_STRTOUL */
			while (pv_isdigit(display_format[strpos])) {
				number_prefix = number_prefix * 10;
				number_prefix += display_format[strpos] - '0';
				strpos++;
			}
#endif				/* !HAVE_STRTOUL */

			sequence_start = strpos;
			sequence_length = 0;
			sequence_colon_offset = 0;
			if ('\0' != display_format[strpos])
				sequence_length = 1;
			if ('{' == display_format[strpos]) {
				while ('\0' != display_format[strpos] && '}' != display_format[strpos]
				       && '%' != display_format[strpos]) {
					if (':' == display_format[strpos])
						sequence_colon_offset = sequence_length;
					strpos++;
					sequence_length++;
				}
			}

			component_type = -1;
			for (component_idx = 0; NULL != format_component_array[component_idx].match; component_idx++) {
				size_t component_sequence_length = strlen(format_component_array[component_idx].match);	/* flawfinder: ignore */
				char *component_colon_pointer =
				    strchr(format_component_array[component_idx].match, (int) ':');

				/* flawfinder - static strings, guaranteed null-terminated. */

				if ((component_sequence_length == sequence_length)
				    && (0 ==
					strncmp(format_component_array[component_idx].match,
						&(display_format[sequence_start]), sequence_length))
				    ) {
					component_type = component_idx;
					break;
				}

				if (sequence_colon_offset > 0 && NULL != component_colon_pointer) {
					size_t component_colon_offset =
					    (size_t) (1 + component_colon_pointer -
						      format_component_array[component_idx].match);
					if ((component_colon_offset == sequence_colon_offset)
					    && (0 ==
						strncmp(format_component_array[component_idx].match,
							&(display_format[sequence_start]), sequence_colon_offset))
					    ) {
						component_type = component_idx;
						string_parameter =
						    &(display_format[sequence_start + sequence_colon_offset]);
						string_parameter_bytes = sequence_length - sequence_colon_offset;
						if (string_parameter_bytes > 0)
							string_parameter_bytes--;	/* the closing '}' */
						if (string_parameter_bytes > PVDISPLAY_BYTECOUNT_MAX)
							string_parameter_bytes = PVDISPLAY_BYTECOUNT_MAX;
						break;
					}
				}
			}

			if (-1 == component_type) {
				/* Unknown sequence - pass it through verbatim. */
				str_start = percent_sign_offset;
				str_bytes = sequence_length + sequence_start - percent_sign_offset;

				if (2 == str_bytes && '%' == display_format[percent_sign_offset + 1]) {
					/* Special case: "%%" => "%". */
					str_bytes = 1;
				} else if (str_bytes > 1 && '%' == display_format[strpos]) {
					/* Special case: "%{foo%p" => "%{foo" and go back one. */
					str_bytes--;
					strpos--;
				} else if (str_bytes == 0 && '\0' == display_format[strpos]) {
					/* Special case: "%" at end of string = "%". */
					str_bytes = 1;
				}
			} else {
				chosen_size = (size_t) number_prefix;
			}

		} else {
			const char *searchptr;
			int foundlength;

			searchptr = strchr(&(display_format[strpos]), '%');
			if (NULL == searchptr) {
				foundlength = (int) strlen(&(display_format[strpos]));	/* flawfinder: ignore */
				/* flawfinder: display_format is explicitly \0-terminated. */
			} else {
				foundlength = searchptr - &(display_format[strpos]);
			}

			component_type = -1;
			str_start = strpos;
			str_bytes = (size_t) foundlength;

			strpos += foundlength - 1;
		}

		if (chosen_size > PVDISPLAY_WIDTH_MAX)
			chosen_size = PVDISPLAY_WIDTH_MAX;

		display->format[segment].type = component_type;
		display->format[segment].chosen_size = chosen_size;
		display->format[segment].string_parameter = string_parameter;
		display->format[segment].string_parameter_bytes = string_parameter_bytes;

		if (-1 == component_type) {
			if (0 == str_bytes)
				continue;

			display->format[segment].offset = str_start;
			display->format[segment].bytes = str_bytes;
			display->format[segment].width = pv_strwidth(&(display_format[str_start]), str_bytes);

			debug("format[%d]:[%.*s], length=%d, width=%d", segment, str_bytes, display_format + str_start,
			      str_bytes, display->format[segment].width);

		} else {
			char dummy_buffer[4];	/* flawfinder: ignore - unused. */
			struct pvformatter_args_s formatter_info;

			display->format[segment].offset = 0;
			display->format[segment].bytes = 0;

			/*
			 * Run the formatter function with a zero-sized
			 * buffer, to invoke its side effects such as
			 * setting display->showing_timer.
			 *
			 * These side effects are required for other parts
			 * of the program to understand what is required,
			 * such as the transfer functions knowning to track
			 * the previous line, or numeric mode knowing which
			 * additional display options are enabled.
			 */
			memset(&formatter_info, 0, sizeof(formatter_info));
			dummy_buffer[0] = '\0';

			formatter_info.display = display;
			formatter_info.segment = &(display->format[segment]);
			formatter_info.status = status;
			formatter_info.control = control;
			formatter_info.transfer = transfer;
			formatter_info.calc = calc;
			formatter_info.buffer = dummy_buffer;
			formatter_info.buffer_size = 0;
			formatter_info.offset = 0;

			/*@-compmempass@ */
			(void) format_component_array[component_type].function(&formatter_info);
			/*@+compmempass@ */
			/*
			 * splint - the buffer we point formatter_info to is
			 * on the stack so doesn't match the "dependent"
			 * annotation, but there's no other appropriate
			 * annotation that doesn't make splint think there's
			 * a leak here.
			 */
		}

		display->format_segment_count++;
	}

	if (display->format_uses_colour && !status->checked_colour_support) {
		status->checked_colour_support = true;
#ifdef ENABLE_NCURSES
		/*
		 * If we have terminal info support, check whether the
		 * current terminal supports colour - or just assume it's
		 * supported if we're forcing output.
		 */
		if (true == control->force) {
			status->terminal_supports_colour = true;
			debug("%s", "force mode - assuming terminal supports colour");
		} else {
			char *term_env = NULL;

			term_env = getenv("TERM");	/* flawfinder: ignore */
			/*
			 * flawfinder - here we pass responsibility to the
			 * ncurses library to behave OK with $TERM.
			 */
			status->terminal_supports_colour = false;
			if (NULL != term_env) {
				int setup_err = 0;

				if ((0 == setupterm(term_env, STDERR_FILENO, &setup_err))
				    && (tigetnum("colors") > 1)
				    ) {
					status->terminal_supports_colour = true;
					debug("%s: %s", term_env, "terminal supports colour");
				} else {
					status->terminal_supports_colour = false;
					debug("%s: %s", term_env, "terminal does not support colour");
				}
			} else {
				/* If TERM is unset, disable colour. */
				status->terminal_supports_colour = false;
				debug("%s", "no TERM variable - disabling colour support");
			}
		}
#else				/* ! ENABLE_NCURSES */
#ifdef USE_POPEN_TPUTS			    /* (! ENABLE_NCURSES) && (USE_POPEN_TPUTS) */
		/*
		 * Without terminal info support, try running "tput colors"
		 * to determine whether colour is available, unless --force
		 * was supplied, in which case colour support is assumed.
		 */
		if (true == control->force) {
			status->terminal_supports_colour = true;
			debug("%s", "force mode - assuming terminal supports colour");
		} else {
			FILE *command_fptr;

			/*@-unrecog@ *//* splint doesn't know popen(). */
			command_fptr = popen("tput colors 2>/dev/null", "r");	/* flawfinder: ignore */
			/*@+unrecog@ */

			/*
			 * flawfinder - we acknowledge that popen() is risky
			 * to call, though we have the mitigation that we're
			 * calling it with a static string.
			 */

			if (NULL == command_fptr) {
				status->terminal_supports_colour = false;
				debug("%s (%s)", "popen failed - disabling colour support", strerror(errno));
			} else {
				int colour_count;
				if (1 == fscanf(command_fptr, "%d", &colour_count)) {
					if (colour_count > 1) {
						status->terminal_supports_colour = true;
						debug("%s (%d)", "terminal supports colour", colour_count);
					} else {
						status->terminal_supports_colour = false;
						debug("%s (%d)",
						      "fewer than 2 colours available - disabling colour support");
					}
				} else {
					status->terminal_supports_colour = false;
					debug("%s", "tput did not produce a number - disabling colour support");
				}
				/*@-unrecog@ *//* splint doesn't know pclose(). */
				(void) pclose(command_fptr);
				/*@+unrecog@ */
			}
		}
#else				/* (! ENABLE_NCURSES) && (! USE_POPEN_TPUTS) */
		/*
		 * Without terminal info support, just assume colour is
		 * available.
		 */
		status->terminal_supports_colour = true;
		debug("%s", "terminal info support not compiled in - assuming colour support");
#endif				/* (! ENABLE_NCURSES) && (! USE_POPEN_TPUTS) */
#endif				/* ! ENABLE_NCURSES */
	}
}


/*
 * Update display->display_buffer with status information formatted
 * according to the state held within the given structures.
 *
 * If "reinitialise" is true, the format string is reparsed first.  This
 * should be true for the first call, and true whenever the format is
 * changed.
 *
 * If "final" is true, this is the final update so the rate is given as an
 * an average over the whole transfer; otherwise the current rate is shown.
 *
 * Returns true if the display buffer can be used, false if not.
 *
 * When returning true, this function will have also set
 * display->display_string_len to the length of the string in
 * display->display_buffer, in bytes.
 *
 * Updates status->exit_status if buffer allocation fails.
 *
 * See pv__format_init for the adjustments that may be made to "status".
 */
bool pv_format(pvprogramstatus_t status, readonly_pvcontrol_t control, readonly_pvtransferstate_t transfer,
	       readonly_pvtransfercalc_t calc, /*@null@ */ const char *format_supplied, pvdisplay_t display,
	       bool reinitialise, bool final)
{
	struct pvdisplay_component_s *format_component_array;
	char display_segments[PV_SIZEOF_FORMAT_SEGMENTS_BUF];	/* flawfinder: ignore - always bounded */
	size_t segment_idx, dynamic_segment_count;
	const char *display_format;
	size_t static_portion_width, dynamic_segment_width;
	size_t display_buffer_offset, display_buffer_remaining;
	size_t new_display_string_bytes, new_display_string_width;
	struct pvformatter_args_s formatter_info;

	memset(&formatter_info, 0, sizeof(formatter_info));
	display_segments[0] = '\0';

	/* Quick safety check for null pointers. */
	if (NULL == status)
		return false;
	if (NULL == control)
		return false;
	if (NULL == transfer)
		return false;
	if (NULL == calc)
		return false;
	if (NULL == display)
		return false;

	formatter_info.display = display;
	formatter_info.buffer = display_segments;
	formatter_info.buffer_size = sizeof(display_segments);
	formatter_info.offset = 0;
	formatter_info.status = status;
	formatter_info.control = control;
	formatter_info.transfer = transfer;
	formatter_info.calc = calc;

	format_component_array = pv__format_components();

	/* Populate the display's "final" flag, for formatters. */
	display->final_update = final;

	/* Reinitialise if we were asked to. */
	if (reinitialise)
		pv__format_init(status, control, transfer, calc, format_supplied, display);

	/* The format string is needed for the static segments. */
	display_format = NULL == format_supplied ? control->default_format : format_supplied;
	if (NULL == display_format)
		return false;

	/* Determine the type of thing being counted for transfer, rate, etc. */
	display->count_type = PV_TRANSFERCOUNT_BYTES;
	if (control->linemode)
		display->count_type = PV_TRANSFERCOUNT_LINES;
	else if (control->decimal_units)
		display->count_type = PV_TRANSFERCOUNT_DECBYTES;

	/*
	 * Reallocate the output buffer if the display width changes.
	 */
	if (display->display_buffer != NULL && display->display_buffer_size < (size_t) ((control->width * 4))) {
		free(display->display_buffer);
		display->display_buffer = NULL;
		display->display_buffer_size = 0;
	}

	/*
	 * Allocate output buffer if there isn't one.
	 */
	if (NULL == display->display_buffer) {
		char *new_buffer;
		size_t new_size;

		new_size = (size_t) ((4 * control->width) + 80);
		if (NULL != control->name)
			new_size += strlen(control->name);	/* flawfinder: ignore */
		/* flawfinder: name is always set by pv_strdup(), which bounds with a \0. */

		new_buffer = malloc(new_size + 16);
		if (NULL == new_buffer) {
			pv_error("%s: %s", _("buffer allocation failed"), strerror(errno));
			status->exit_status |= PV_ERROREXIT_MEMORY;
			display->display_buffer = NULL;
			return false;
		}

		display->display_buffer = new_buffer;
		display->display_buffer_size = new_size;
		display->display_buffer[0] = '\0';
	}

	/* Clear the SGR active codes flag, for the SGR formatter. */
	display->sgr_code_active = false;

	/*
	 * Populate the internal segments buffer with each component's
	 * output, in two passes.
	 */

	/* First pass - all components with a fixed width. */

	static_portion_width = 0;
	dynamic_segment_count = 0;

	for (segment_idx = 0; segment_idx < display->format_segment_count; segment_idx++) {
		pvdisplay_segment_t segment;
		struct pvdisplay_component_s *component;
		size_t bytes_added;
		bool fixed_width;

		segment = &(display->format[segment_idx]);
		if (-1 == segment->type) {
			static_portion_width += segment->width;
			continue;
		}
		component = &(format_component_array[segment->type]);

		fixed_width = true;
		if (component->dynamic && 0 == segment->chosen_size)
			fixed_width = false;

		if (!fixed_width) {
			dynamic_segment_count++;
			continue;
		}

		segment->width = segment->chosen_size;

		formatter_info.segment = segment;
		/*@-compmempass@ */
		bytes_added = component->function(&formatter_info);
		/*@+compmempass@ *//* see previous ->function() note. */

		segment->width = 0;
		if (bytes_added > 0) {
			segment->width = pv_strwidth(&(display_segments[formatter_info.offset]), bytes_added);
		}

		formatter_info.offset += bytes_added;
		static_portion_width += segment->width;
	}

	/*
	 * Second pass, now the remaining width is known - all components
	 * with a dynamic width.
	 */

	dynamic_segment_width = 0;
	if (control->width > static_portion_width)
		dynamic_segment_width = control->width - static_portion_width;

	/*
	 * Divide the total remaining screen space by the number of dynamic
	 * segments, so that multiple dynamic segments will share the space.
	 */
	if (dynamic_segment_count > 1)
		dynamic_segment_width /= dynamic_segment_count;

	debug("control->width=%d static_portion_width=%d dynamic_segment_width=%d dynamic_segment_count=%d",
	      control->width, static_portion_width, dynamic_segment_width, dynamic_segment_count);

	for (segment_idx = 0; segment_idx < display->format_segment_count; segment_idx++) {
		pvdisplay_segment_t segment;
		struct pvdisplay_component_s *component;
		size_t bytes_added;
		bool fixed_width;

		segment = &(display->format[segment_idx]);
		if (-1 == segment->type) {
			static_portion_width += segment->width;
			continue;
		}
		component = &(format_component_array[segment->type]);

		fixed_width = true;
		if (component->dynamic && 0 == segment->chosen_size)
			fixed_width = false;

		if (fixed_width)
			continue;

		segment->width = dynamic_segment_width;

		formatter_info.segment = segment;
		/*@-compmempass@ */
		bytes_added = component->function(&formatter_info);
		/*@+compmempass@ *//* see earlier ->function() note. */

		formatter_info.offset += bytes_added;
	}

	/*
	 * Populate the display buffer from the segments.
	 */

	memset(display->display_buffer, 0, display->display_buffer_size);
	display_buffer_offset = 0;
	display_buffer_remaining = display->display_buffer_size - 1;
	new_display_string_bytes = 0;
	new_display_string_width = 0;

	for (segment_idx = 0; segment_idx < display->format_segment_count; segment_idx++) {
		pvdisplay_segment_t segment;
		const char *content_buffer = display_format;

		segment = &(display->format[segment_idx]);
		if (0 == segment->bytes)
			continue;
		if (segment->bytes > display_buffer_remaining)
			continue;

		if (-1 == segment->type) {
			content_buffer = display_format;
		} else {
			content_buffer = display_segments;
		}

		memmove(display->display_buffer + display_buffer_offset, content_buffer + segment->offset,
			segment->bytes);
		display_buffer_offset += segment->bytes;
		display_buffer_remaining -= segment->bytes;

		new_display_string_bytes += segment->bytes;
		new_display_string_width += segment->width;

		debug("segment[%d]: bytes=%d, width=%d: [%.*s]", segment_idx, segment->bytes, segment->width,
		      segment->bytes, display->display_buffer + display_buffer_offset - segment->bytes);
	}

	/* If the SGR active codes flag is set, we need to emit an SGR reset. */
	if (display->sgr_code_active) {
		debug("%s", "SGR codes still active - adding reset");
		(void) pv_strlcat(display->display_buffer, "\033[m", display->display_buffer_size);
		new_display_string_bytes += 3;
		if (new_display_string_bytes > display->display_buffer_size)
			new_display_string_bytes = display->display_buffer_size;
		display->sgr_code_active = false;
	}

	debug("%s: %d", "new display string length in bytes", (int) new_display_string_bytes);
	debug("%s: %d", "new display string width", (int) new_display_string_width);

	/*
	 * If the width of our output shrinks, we need to keep appending
	 * spaces at the end, so that we don't leave dangling bits behind.
	 */
	if ((new_display_string_width < display->display_string_width)
	    && (control->width >= display->prev_screen_width)) {
		char spaces[32];	 /* flawfinder: ignore - terminated, bounded */
		int spaces_to_add;

		spaces_to_add = (int) (display->display_string_width - new_display_string_width);
		/* Upper boundary on number of spaces */
		if (spaces_to_add > 15) {
			spaces_to_add = 15;
		}
		new_display_string_bytes += spaces_to_add;
		new_display_string_width += spaces_to_add;
		spaces[spaces_to_add] = '\0';
		while (--spaces_to_add >= 0) {
			spaces[spaces_to_add] = ' ';
		}
		(void) pv_strlcat(display->display_buffer, spaces, display->display_buffer_size);
	}

	display->display_string_bytes = new_display_string_bytes;
	display->display_string_width = new_display_string_width;
	display->prev_screen_width = control->width;

	return true;
}


/*
 * Output status information on standard error.
 *
 * If "final" is true, this is the final update, so the rate is given as an
 * an average over the whole transfer; otherwise the current rate is shown.
 */
void pv_display(pvprogramstatus_t status, readonly_pvcontrol_t control, pvtransientflags_t flags,
		readonly_pvtransferstate_t transfer, pvtransfercalc_t calc, pvcursorstate_t cursor, pvdisplay_t display,
		/*@null@ */ pvdisplay_t extra_display, bool final)
{
	bool reinitialise = false;

	if (NULL == status)
		return;
	if (NULL == control)
		return;
	if (NULL == flags)
		return;
	if (NULL == transfer)
		return;
	if (NULL == calc)
		return;
	if (NULL == cursor)
		return;
	if (NULL == display)
		return;

	pv_sig_checkbg();

	pv_calculate_transfer_rate(calc, transfer, control, display, final);

	/*
	 * Enable colour on the main display, and disable it on the extra
	 * display (process title, window title).
	 */
	display->colour_permitted = true;
	if (NULL != extra_display)
		extra_display->colour_permitted = false;

	/*
	 * If the display options need reparsing, do so to generate new
	 * formatting parameters.
	 */
	if (0 != flags->reparse_display) {
		reinitialise = true;
		flags->reparse_display = 0;
	}

	if (!pv_format(status, control, transfer, calc, control->format_string, display, reinitialise, final))
		return;

	if ((NULL != extra_display) && (0 != control->extra_displays)) {
		if (!pv_format
		    (status, control, transfer, calc, control->extra_format_string, extra_display, reinitialise, final))
			return;
	}

	if (NULL == display->display_buffer)
		return;

	if (control->numeric) {
		pv_tty_write(flags, display->display_buffer, display->display_string_bytes);
		pv_tty_write(flags, "\n", 1);
	} else if (control->cursor) {
		if (control->force || pv_in_foreground()) {
			pv_crs_update(cursor, control, flags, display->display_buffer);
			display->output_produced = true;
			pv__output_produced = true;
		}
	} else {
		if (control->force || pv_in_foreground()) {
			pv_tty_write(flags, display->display_buffer, display->display_string_bytes);
			pv_tty_write(flags, "\r", 1);
			display->output_produced = true;
			pv__output_produced = true;
		}
	}

	debug("%s: [%s]", "display", display->display_buffer);

	if ((0 != (PV_DISPLAY_WINDOWTITLE & control->extra_displays))
	    && (control->force || pv_in_foreground())
	    && (NULL != extra_display)
	    && (NULL != extra_display->display_buffer)
	    ) {
		pv_tty_write(flags, "\033]2;", 4);
		pv_tty_write(flags, extra_display->display_buffer, extra_display->display_string_bytes);
		pv_tty_write(flags, "\033\\", 2);
		extra_display->output_produced = true;
		debug("%s: [%s]", "windowtitle display", extra_display->display_buffer);
	}

	if ((0 != (PV_DISPLAY_PROCESSTITLE & control->extra_displays))
	    && (NULL != extra_display)
	    && (NULL != extra_display->display_buffer)
	    ) {
		setproctitle("%s -- %s", PACKAGE_NAME, extra_display->display_buffer);
		extra_display->output_produced = true;
		debug("%s: [%s]", "processtitle display", extra_display->display_buffer);
	}
}
