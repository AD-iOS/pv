/*
 * State management functions.
 *
 * Copyright 2002-2008, 2010, 2012-2015, 2017, 2021, 2023-2025 Andrew Wood
 *
 * License GPLv3+: GNU GPL version 3 or later; see `docs/COPYING'.
 */

#include "config.h"
#include "pv.h"
#include "pv-internal.h"

/* We do not set this because it breaks "dd" - see below. */
/* #undef MAKE_OUTPUT_NONBLOCKING */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>


/*
 * Allocate or reallocate the history buffer for calculating average
 * transfer rate.
 */
static void pv_alloc_calc_history(pvtransfercalc_t calc)
{
	if (NULL != calc->history)
		free(calc->history);
	calc->history = NULL;

	calc->history = calloc((size_t) (calc->history_len), sizeof(calc->history[0]));
	if (NULL == calc->history) {
		/*@-mustfreefresh@ */
		/*
		 * splint note: the gettext calls made by _() cause memory
		 * leak warnings, but in this case it's unavoidable, and
		 * mitigated by the fact we only translate each string once.
		 */
		pv_error("%s: %s", _("history structure allocation failed"), strerror(errno));
		/*@+mustfreefresh@ */
		return;
	}

	calc->history_first = calc->history_last = 0;
	calc->history[0].elapsed_sec = 0.0; /* to be safe, memset() not recommended for doubles */
}


/*
 * Set the "calc" state sub-structure's history length appropriately for the
 * given average rate window setting, allocate an appropriate history, and
 * return the appropriate history interval to use.
 */
unsigned int pv_update_calc_average_rate_window(pvtransfercalc_t calc, unsigned int val)
{
	unsigned int history_interval;

	if (val < 1)
		val = 1;

	if (val >= 20) {
		calc->history_len = (size_t) (val / 5 + 1);
		history_interval = 5;
	} else {
		calc->history_len = (size_t) (val + 1);
		history_interval = 1;
	}

	pv_alloc_calc_history(calc);

	return history_interval;
}


/*
 * Clear the "calc" part of a state structure.
 */
void pv_reset_calc(pvtransfercalc_t calc)
{
	if (NULL == calc)
		return;
	/*
	 * Explicitly set important floating point values to 0, as memset()
	 * is not recommended for this.
	 */
	calc->transfer_rate = 0.0;
	calc->average_rate = 0.0;
	calc->prev_elapsed_sec = 0.0;
	calc->prev_rate = 0.0;
	calc->prev_trans = 0.0;
	calc->current_avg_rate = 0.0;
	calc->rate_min = 0.0;
	calc->rate_max = 0.0;
	calc->rate_sum = 0.0;
	calc->ratesquared_sum = 0.0;
	calc->measurements_taken = 0;
	calc->prev_transferred = 0;
	calc->percentage = 0.0;
	calc->history_first = calc->history_last = 0;
	if (NULL != calc->history) {
		calc->history[0].elapsed_sec = 0.0;
	}
}


/*
 * Clear the "transfer" part of a state structure.
 */
void pv_reset_transfer(pvtransferstate_t transfer)
{
	if (NULL == transfer)
		return;

	transfer->elapsed_seconds = 0.0;
	transfer->read_position = 0;
	transfer->write_position = 0;
	transfer->to_write = 0;
	transfer->written = 0;
	transfer->total_bytes_read = 0;
	transfer->total_written = 0;
	transfer->written_but_not_consumed = 0;
	transfer->read_errors_in_a_row = 0;
	transfer->last_read_skip_fd = 0;
#ifdef HAVE_SPLICE
	transfer->splice_failed_fd = -1;
#endif				/* HAVE_SPLICE */

	transfer->line_positions_length = 0;
	transfer->line_positions_head = 0;
	transfer->last_output_position = 0;
	transfer->output_not_seekable = false;
}


/*
 * Reset the calculated parts of the "flags" state structure.
 */
void pv_reset_flags(pvtransientflags_t flags)
{
	if (NULL == flags)
		return;
	flags->reparse_display = 1;
}


/*
 * Reset the calculated parts of a "display" state structure.
 */
void pv_reset_display(pvdisplay_t display)
{
	if (NULL == display)
		return;
	display->initial_offset = 0;
	display->output_produced = false;
}


/*
 * Clear the calculated parts of a state structure.
 */
void pv_state_reset(pvstate_t state)
{
	if (NULL == state)
		return;

	pv_reset_flags(&(state->flags));
	state->status.current_input_file = -1;

	pv_reset_display(&(state->display));
	pv_reset_display(&(state->extra_display));

	pv_reset_calc(&(state->calc));
	pv_reset_transfer(&(state->transfer));
}


/*
 * Create a new state structure, and return it, or 0 (NULL) on error.
 */
pvstate_t pv_state_alloc(void)
{
	pvstate_t state;

	state = calloc(1, sizeof(*state));
	if (NULL == state)
		return NULL;
	memset(state, 0, sizeof(*state));

	state->watchfd.count = 0;
	state->control.output_fd = -1;
#ifdef HAVE_IPC
	state->cursor.shmid = -1;
	state->cursor.pvcount = 1;
#endif				/* HAVE_IPC */
	state->cursor.lock_fd = -1;

	pv_state_reset(state);

	/*
	 * Get the current working directory, if possible, as a base for
	 * showing relative filenames with --watchfd.
	 */
	if (NULL == getcwd(state->status.cwd, PV_SIZEOF_CWD - 1)) {
		/* failed - will always show full path */
		state->status.cwd[0] = '\0';
	}
	if ('\0' == state->status.cwd[1]) {
		/* CWD is root directory - always show full path */
		state->status.cwd[0] = '\0';
	}
	state->status.cwd[PV_SIZEOF_CWD - 1] = '\0';

	return state;
}


/*
 * Free dynamic contents of a display structure.
 */
void pv_freecontents_display(pvdisplay_t display)
{
	if (NULL != display->display_buffer)
		free(display->display_buffer);
	display->display_buffer = NULL;
}


/*
 * Free dynamic contents of a transfer state structure.
 */
void pv_freecontents_transfer(pvtransferstate_t transfer)
{
	/*@-keeptrans@ */
	if (NULL != transfer->transfer_buffer)
		free(transfer->transfer_buffer);
	transfer->transfer_buffer = NULL;
	/*@+keeptrans@ */
	/* splint - explicitly freeing this structure, so free() here is OK. */

	if (NULL != transfer->line_positions)
		free(transfer->line_positions);
	transfer->line_positions = NULL;
}


/*
 * Free dynamic contents of a calculated transfer state structure.
 */
void pv_freecontents_calc(pvtransfercalc_t calc)
{
	if (NULL != calc->history)
		free(calc->history);
	calc->history = NULL;
}


/*
 * Free the contents of a watchfd watched-items array.
 */
void pv_freecontents_watchfd_items(struct pvwatcheditem_s *watching, unsigned int count)
{
	unsigned int watch_idx;

	if (NULL == watching)
		return;

	/*@-compdestroy@ */
	for (watch_idx = 0; watch_idx < count; watch_idx++) {
		int info_idx;
		if (NULL == watching[watch_idx].info_array)
			continue;
		for (info_idx = 0; info_idx < watching[watch_idx].array_length; info_idx++) {
			pv_freecontents_watchfd(&(watching[watch_idx].info_array[info_idx]));
		}
		free(watching[watch_idx].info_array);
		watching[watch_idx].info_array = NULL;
		watching[watch_idx].array_length = 0;
	}

	return;
	/*@+compdestroy @ */
	/*
	 * splint warns that watching[].info_array->transfer.transfer_buffer
	 * and other deep structures may not be deallocated and so leak
	 * memory, but that's what pv_freecontents_watchfd() takes care of.
	 */
}


/*
 * Truncate the output file descriptor to its current position, if it's a
 * valid fd, we're in sparse output mode, and no lseek() failed.
 */
static void pv_truncate_output(pvstate_t state)
{
	off_t current_offset;

	if (!state->control.sparse_output)
		return;
	if (state->transfer.output_not_seekable)
		return;
	if (state->control.output_fd < 0)
		return;

	current_offset = (off_t) lseek(state->control.output_fd, (off_t) 0, SEEK_CUR);
	if (current_offset == (off_t) - 1)
		return;

	debug("%s: %ld", "truncating to current offset", (long) current_offset);

	/*@+longintegral@ */
	/*
	 * splint has trouble with off_t / __off_t.
	 */
	if (0 != ftruncate(state->control.output_fd, current_offset)) {
		debug("%s: %s", "output ftruncate() failed", strerror(errno));
	}
	/*@-longintegral@ */
}


/*
 * Free a state structure, after which it can no longer be used.
 */
void pv_state_free(pvstate_t state)
{
	if (0 == state)
		return;

	/*
	 * Close the output file first, so we can report any errors while we
	 * still know the program name and output filename.
	 */
	if (state->control.output_fd >= 0) {
		pv_truncate_output(state);
		if (STDOUT_FILENO != state->control.output_fd) {
			if (close(state->control.output_fd) < 0) {
				pv_error("%s: %s",
					 NULL == state->control.output_name ? "(null)" : state->control.output_name,
					 strerror(errno));
			}
		}
		state->control.output_fd = -1;
	}

	if (NULL != state->control.output_name) {
		free(state->control.output_name);
		state->control.output_name = NULL;
	}

	pv_freecontents_display(&(state->display));
	pv_freecontents_display(&(state->extra_display));

	if (NULL != state->control.name) {
		free(state->control.name);
		state->control.name = NULL;
	}

	if (NULL != state->control.default_bar_style) {
		free(state->control.default_bar_style);
		state->control.default_bar_style = NULL;
	}

	if (NULL != state->control.format_string) {
		free(state->control.format_string);
		state->control.format_string = NULL;
	}

	if (NULL != state->control.extra_display_spec) {
		free(state->control.extra_display_spec);
		state->control.extra_display_spec = NULL;
	}

	if (NULL != state->control.extra_format_string) {
		free(state->control.extra_format_string);
		state->control.extra_format_string = NULL;
	}

	pv_freecontents_transfer(&(state->transfer));

	pv_freecontents_calc(&(state->calc));

	if (NULL != state->files.filename) {
		unsigned int file_idx;
		for (file_idx = 0; file_idx < state->files.file_count; file_idx++) {
			/*@-unqualifiedtrans@ */
			free(state->files.filename[file_idx]);
			/*@+unqualifiedtrans@ */
			/* splint: see similar code below. */
		}
		free(state->files.filename);
		state->files.filename = NULL;
	}

	if (NULL != state->watchfd.watching) {
		pv_freecontents_watchfd_items(state->watchfd.watching, state->watchfd.count);
		free(state->watchfd.watching);
		state->watchfd.watching = NULL;
	}

	free(state);

	return;
}


/*
 * Set the formatting string, given a set of old-style formatting options.
 */
void pv_state_set_format(pvstate_t state, bool progress, bool timer, bool eta, bool fineta, bool rate, bool average_rate, bool bytes, bool bufpercent, size_t lastwritten,	/*@null@ */
			 const char *name)
{
#define PV_ADDFORMAT(x,y) if (x) { \
		if (state->control.default_format[0] != '\0') \
			(void) pv_strlcat(state->control.default_format, " ", sizeof(state->control.default_format)); \
		(void) pv_strlcat(state->control.default_format, y, sizeof(state->control.default_format)); \
	}

	state->control.format_option.progress = progress;
	state->control.format_option.timer = timer;
	state->control.format_option.eta = eta;
	state->control.format_option.fineta = fineta;
	state->control.format_option.rate = rate;
	state->control.format_option.average_rate = average_rate;
	state->control.format_option.bytes = bytes;
	state->control.format_option.bufpercent = bufpercent;
	state->control.format_option.lastwritten = lastwritten;

	state->control.default_format[0] = '\0';

	if (false == state->control.numeric) {
		/* Standard progress display mode (without "--numeric"). */

		/*
		 * Add the format strings for the enabled options in a
		 * standard order.
		 */
		PV_ADDFORMAT(name, "%N");
		PV_ADDFORMAT(bytes, "%b");
		PV_ADDFORMAT(bufpercent, "%T");
		PV_ADDFORMAT(timer, "%t");
		PV_ADDFORMAT(rate, "%r");
		PV_ADDFORMAT(average_rate, "%a");
		PV_ADDFORMAT(progress, "%p");
		PV_ADDFORMAT(eta, "%e");
		PV_ADDFORMAT(fineta, "%I");

		if (lastwritten > 0) {
			char buf[16];	 /* flawfinder: ignore */
			memset(buf, 0, sizeof(buf));
			(void) pv_snprintf(buf, sizeof(buf), "%%%uA", (unsigned int) lastwritten);
			PV_ADDFORMAT(lastwritten > 0, buf);
			/*
			 * flawfinder rationale: large enough for string,
			 * zeroed before use, only written to by
			 * pv_snprintf() with the right buffer length.
			 */
		}

	} else {
		/* Numeric mode has different behaviour. */

		PV_ADDFORMAT(timer, "%t");
		PV_ADDFORMAT(bytes, "%b");
		PV_ADDFORMAT(rate, "%r");
		PV_ADDFORMAT(!(bytes || rate), "%{progress-amount-only}");

	}

	debug("%s: [%s]", "default format set", state->control.default_format);

	/* Free any previously set name. */
	if (NULL != state->control.name) {
		free(state->control.name);
		state->control.name = NULL;
	}

	/* Set a new name if one was given. */
	if (NULL != name)
		state->control.name = pv_strdup(name);

	/* Tell pv_format() that the format has changed. */
	state->flags.reparse_display = 1;
}


void pv_state_force_set(pvstate_t state, bool val)
{
	state->control.force = val;
}

void pv_state_cursor_set(pvstate_t state, bool val)
{
	state->control.cursor = val;
}

void pv_state_show_stats_set(pvstate_t state, bool val)
{
	state->control.show_stats = val;
}

void pv_state_numeric_set(pvstate_t state, bool val)
{
	state->control.numeric = val;
}

void pv_state_wait_set(pvstate_t state, bool val)
{
	state->control.wait = val;
}

void pv_state_delay_start_set(pvstate_t state, double val)
{
	state->control.delay_start = val;
}

void pv_state_rate_gauge_set(pvstate_t state, bool val)
{
	state->control.rate_gauge = val;
}

void pv_state_linemode_set(pvstate_t state, bool val)
{
	state->control.linemode = val;
}

void pv_state_bits_set(pvstate_t state, bool bits)
{
	state->control.bits = bits;
}

void pv_state_decimal_units_set(pvstate_t state, bool decimal_units)
{
	state->control.decimal_units = decimal_units;
}

void pv_state_null_terminated_lines_set(pvstate_t state, bool val)
{
	state->control.null_terminated_lines = val;
}

void pv_state_no_display_set(pvstate_t state, bool val)
{
	state->control.no_display = val;
}

void pv_state_skip_errors_set(pvstate_t state, unsigned int val)
{
	state->control.skip_errors = val;
}

void pv_state_error_skip_block_set(pvstate_t state, off_t val)
{
	state->control.error_skip_block = val;
}

void pv_state_stop_at_size_set(pvstate_t state, bool val)
{
	state->control.stop_at_size = val;
}

void pv_state_sync_after_write_set(pvstate_t state, bool val)
{
	state->control.sync_after_write = val;
}

void pv_state_direct_io_set(pvstate_t state, bool val)
{
	state->control.direct_io = val;
	state->control.direct_io_changed = true;
}

void pv_state_sparse_output_set(pvstate_t state, bool val)
{
	state->control.sparse_output = val;
}

void pv_state_discard_input_set(pvstate_t state, bool val)
{
	state->control.discard_input = val;
}

void pv_state_rate_limit_set(pvstate_t state, off_t val)
{
	state->control.rate_limit = val;
}

void pv_state_target_buffer_size_set(pvstate_t state, size_t val)
{
	state->control.target_buffer_size = val;
}

void pv_state_no_splice_set(pvstate_t state, bool val)
{
	state->control.no_splice = val;
}

void pv_state_size_set(pvstate_t state, off_t val)
{
	state->control.size = val;
}

void pv_state_interval_set(pvstate_t state, double val)
{
	state->control.interval = val;
}

void pv_state_width_set(pvstate_t state, unsigned int val, bool was_set_manually)
{
	if (val > PVDISPLAY_WIDTH_MAX)
		val = PVDISPLAY_WIDTH_MAX;
	state->control.width = (pvdisplay_width_t) val;
	state->control.width_set_manually = was_set_manually;
}

void pv_state_height_set(pvstate_t state, unsigned int val, bool was_set_manually)
{
	state->control.height = val;
	state->control.height_set_manually = was_set_manually;
}

void pv_state_name_set(pvstate_t state, /*@null@ */ const char *val)
{
	if (NULL != state->control.name) {
		free(state->control.name);
		state->control.name = NULL;
	}
	if (NULL != val)
		state->control.name = pv_strdup(val);
}

void pv_state_default_bar_style_set(pvstate_t state, /*@null@ */ const char *val)
{
	if (NULL != state->control.default_bar_style) {
		free(state->control.default_bar_style);
		state->control.default_bar_style = NULL;
	}
	if (NULL != val)
		state->control.default_bar_style = pv_strdup(val);
}

void pv_state_format_string_set(pvstate_t state, /*@null@ */ const char *val)
{
	if (NULL != state->control.format_string) {
		free(state->control.format_string);
		state->control.format_string = NULL;
	}
	if (NULL != val)
		state->control.format_string = pv_strdup(val);
}

void pv_state_extra_display_set(pvstate_t state, /*@null@ */ const char *val)
{
	const char *word_start;
	size_t offset;

	if (NULL != state->control.extra_display_spec) {
		free(state->control.extra_display_spec);
		state->control.extra_display_spec = NULL;
	}

	if (NULL != state->control.extra_format_string) {
		free(state->control.extra_format_string);
		state->control.extra_format_string = NULL;
	}

	state->control.extra_displays = 0;
	if (NULL == val)
		return;

	if (NULL != val)
		state->control.extra_display_spec = pv_strdup(val);

	word_start = val;
	while (NULL != word_start && '\0' != word_start[0]) {
		offset = 0;
		while ('\0' != word_start[offset] && ',' != word_start[offset] && ':' != word_start[offset])
			offset++;
		if (((11 == offset) && (0 == strncmp(word_start, "windowtitle", 11)))
		    || ((6 == offset) && (0 == strncmp(word_start, "window", 6)))
		    ) {
			debug("%s", "enabling windowtitle");
			state->control.extra_displays |= PV_DISPLAY_WINDOWTITLE;
		} else if (((12 == offset) && (0 == strncmp(word_start, "processtitle", 12)))
			   || ((9 == offset) && (0 == strncmp(word_start, "proctitle", 9)))
			   || ((7 == offset) && (0 == strncmp(word_start, "process", 7)))
			   || ((4 == offset) && (0 == strncmp(word_start, "proc", 4)))
		    ) {
			debug("%s", "enabling processtitle");
			state->control.extra_displays |= PV_DISPLAY_PROCESSTITLE;
		}
		switch (word_start[offset]) {
		case ',':
			offset++;
			break;
		case ':':
			offset++;
			debug("%s: [%s]", "setting extra_format_string", word_start + offset);
			state->control.extra_format_string = pv_strdup(word_start + offset);
			word_start = NULL;
			break;
		default:
			word_start = NULL;
			break;
		}
		if (NULL != word_start)
			word_start += offset;
	}
}

void pv_state_output_set(pvstate_t state, int fd, const char *name)
{
	/*
	 * Close any previous output file first, so we can report any errors
	 * before we store the new output filename.
	 */
	pv_truncate_output(state);
	if (state->control.output_fd >= 0 && state->control.output_fd != STDOUT_FILENO) {
		if (close(state->control.output_fd) < 0) {
			pv_error("%s: %s",
				 NULL == state->control.output_name ? "(null)" : state->control.output_name,
				 strerror(errno));
		}
	}
	if (NULL != state->control.output_name)
		free(state->control.output_name);
	state->control.output_fd = fd;
	state->control.output_name = pv_strdup(name);
#ifdef MAKE_OUTPUT_NONBLOCKING
	/*
	 * Try and make the output use non-blocking I/O.
	 *
	 * Note that this can cause problems with (broken) applications
	 * such as dd when used in a pipeline.
	 */
	fcntl(state->control.output_fd, F_SETFL, O_NONBLOCK | fcntl(state->control.output_fd, F_GETFL));
#endif				/* MAKE_OUTPUT_NONBLOCKING */

	/*
	 * In sparse output mode, if the output is in append mode (>>),
	 * explicitly lseek() to the end of the file.  Otherwise, the file
	 * offset is not set until the first write(), which means that if
	 * the input starts with null bytes, when we lseek() past them
	 * relative to the current position, the "current position" is 0
	 * rather than the end of the file, and the file gets truncated on
	 * exit to the wrong size.
	 */
	if (state->control.sparse_output && 0 != (fcntl(fd, F_GETFL) & O_APPEND)) {
		debug("%s", "sparse output mode, and appending - seeking output to the end");
		if ((off_t) lseek(fd, 0, SEEK_END) == (off_t) - 1) {
			debug("%s: %s", "lseek failed", strerror(errno));
		}
	}
}

void pv_state_average_rate_window_set(pvstate_t state, unsigned int val)
{
	if (val < 1)
		val = 1;
	state->control.average_rate_window = val;
	state->control.history_interval = pv_update_calc_average_rate_window(&(state->calc), val);
}

void pv_state_set_terminal_supports_utf8(pvstate_t state, bool val)
{
	state->status.terminal_supports_utf8 = val;
}

/*
 * Set the array of input files.
 */
void pv_state_inputfiles(pvstate_t state, unsigned int input_file_count, const char **input_files)
{
	unsigned int file_idx;
	/*@only@ */ nullable_string_t *new_array;

	/* Free the old array and its contents, if there was one. */
	if (NULL != state->files.filename) {
		for (file_idx = 0; file_idx < state->files.file_count; file_idx++) {
			free(state->files.filename[file_idx]);
		}
		free(state->files.filename);
		state->files.filename = NULL;
		state->files.file_count = 0;
	}

	/* Allocate an empty new array of the right size. */
	new_array = calloc((size_t) (input_file_count + 1), sizeof(char *));
	if (NULL == new_array) {
		/*@-mustfreefresh@ *//* see similar _() issue above */
		pv_error("%s: %s", _("file list allocation failed"), strerror(errno));
		/*@+mustfreefresh@ */
		return;
	}
	state->files.filename = new_array;

	/* Populate the new array with copies of the filenames. */
	for (file_idx = 0; file_idx < input_file_count; file_idx++) {
		/*@only@ */ char *new_string;
		new_string = pv_strdup(input_files[file_idx]);
		if (NULL == new_string) {
			/*@-mustfreefresh@ *//* see similar _() issue above */
			pv_error("%s: %s", _("file list allocation failed"), strerror(errno));
			/*@+mustfreefresh@ */
			return;
		}
		state->files.filename[file_idx] = new_string;
	}
	state->files.file_count = input_file_count;
}

/*
 * Set the arrays of watchfd process IDs and file descriptors.
 */
void pv_state_watchfds(pvstate_t state, unsigned int watchfd_count, const pid_t * pids, const int *fds)
{
	unsigned int item_idx;
	/*@only@ */ struct pvwatcheditem_s *new_array = NULL;

	/* Free the old arrays, if there were any. */
	if (NULL != state->watchfd.watching) {
		/*@-compdestroy@ */
		pv_freecontents_watchfd_items(state->watchfd.watching, state->watchfd.count);
		free(state->watchfd.watching);
		state->watchfd.watching = NULL;
		/*@+compdestroy@ */
		/*
		 * splint warns about deep structures not being deallocated,
		 * but that's what pv_freecontents_watchfd_items() does.
		 */
	}
	state->watchfd.count = 0;
	state->watchfd.multiple_pids = false;

	/* Allocate an empty new array of the right size. */
	new_array = malloc((1 + watchfd_count) * sizeof(*new_array));
	if (NULL == new_array) {
		/*@-mustfreefresh@ *//* see similar _() issue above */
		pv_error("%s: %s", _("buffer allocation failed"), strerror(errno));
		/*@+mustfreefresh@ */
		return;
	}
	memset(new_array, 0, (1 + watchfd_count) * sizeof(*new_array));
	state->watchfd.watching = new_array;

	/* Populate the new array with the values supplied. */
	for (item_idx = 0; item_idx < watchfd_count; item_idx++) {
		state->watchfd.watching[item_idx].pid = pids[item_idx];
		state->watchfd.watching[item_idx].fd = fds[item_idx];
		if ((item_idx > 0) && (pids[item_idx] != pids[item_idx - 1]))
			state->watchfd.multiple_pids = true;
	}
	state->watchfd.count = watchfd_count;

	debug("%s=%d, %s=%s", "watchfd.count", state->watchfd.count, "multiple_pids",
	      state->watchfd.multiple_pids ? "true" : "false");
}
