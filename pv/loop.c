/*
 * Functions providing the main transfer or file descriptor watching loop.
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
#include <string.h>
#include <errno.h>

#define _GNU_SOURCE 1
#include <limits.h>

#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#if HAVE_MATH_H
#include <math.h>
#endif

int pv_remote_transferstate_fetch(pvstate_t, pid_t, /*@null@ */ off_t *, bool);


#if HAVE_SQRTL
#else
/*
 * Square root of a long double.  Adapted from iputils ping/ping_common.c.
 */
static long ldsqrt(long double value)
{
	long double previous = (long double) LLONG_MAX;
	long double result = value;

	if (result > 0) {
		while (result < previous) {
			previous = result;
			result = (result + (value / result)) / 2;
		}
	}

	return result;
}
#endif


/*
 * If the flag is set to say that a terminal resize signal was received,
 * clear the flag and resize the display, and return true.
 */
static bool pv__resize_display_on_signal(pvstate_t state)
{
	unsigned int new_width, new_height;

	if (0 == state->flags.terminal_resized)
		return false;

	state->flags.terminal_resized = 0;

	new_width = (unsigned int) (state->control.width);
	new_height = state->control.height;
	pv_screensize(&new_width, &new_height);

	if (new_width > PVDISPLAY_WIDTH_MAX)
		new_width = PVDISPLAY_WIDTH_MAX;
	if (!state->control.width_set_manually)
		state->control.width = (pvdisplay_width_t) new_width;
	if (!state->control.height_set_manually)
		state->control.height = new_height;

	return true;
}


/*
 * Calculate and display the transfer statistics at the end of the transfer,
 * if stats are enabled and any measurements were taken.
 */
static void pv__show_stats(pvstate_t state)
{
	if (!state->control.show_stats)
		return;

	if (state->calc.measurements_taken > 0) {
		char stats_buf[256];	 /* flawfinder: ignore */
		long double rate_mean, rate_variance, rate_deviation;
		int stats_size;

		/* flawfinder: made safe by use of pv_snprintf() */

		rate_mean = state->calc.rate_sum / ((long double) (state->calc.measurements_taken));
		rate_variance =
		    (state->calc.ratesquared_sum / ((long double) (state->calc.measurements_taken))) -
		    (rate_mean * rate_mean);
#if HAVE_SQRTL
		rate_deviation = sqrtl(rate_variance);
#else
		rate_deviation = ldsqrt(rate_variance);
#endif

		debug("%s: %ld", "measurements taken", state->calc.measurements_taken);
		debug("%s: %.3Lf", "rate_sum", state->calc.rate_sum);
		debug("%s: %.3Lf", "ratesquared_sum", state->calc.ratesquared_sum);
		debug("%s: %.3Lf", "rate_mean", rate_mean);
		debug("%s: %.3Lf", "rate_variance", rate_variance);
		debug("%s: %.3Lf", "rate_deviation", rate_deviation);

		memset(stats_buf, 0, sizeof(stats_buf));
		stats_size =
		    pv_snprintf(stats_buf, sizeof(stats_buf), "%s = %.3Lf/%.3Lf/%.3Lf/%.3Lf %s\n",
				_("rate min/avg/max/mdev"), state->calc.rate_min, rate_mean, state->calc.rate_max,
				rate_deviation, state->control.bits ? _("b/s") : _("B/s"));

		if (stats_size > 0 && stats_size < (int) (sizeof(stats_buf)))
			pv_tty_write(&(state->flags), stats_buf, (size_t) stats_size);
	} else if (state->control.show_stats && state->calc.measurements_taken < 1) {
		char msg_buf[256];	 /* flawfinder: ignore */
		int msg_size;

		/* flawfinder: made safe by use of pv_snprintf() */

		memset(msg_buf, 0, sizeof(msg_buf));
		msg_size = pv_snprintf(msg_buf, sizeof(msg_buf), "%s\n", _("rate not measured"));

		if (msg_size > 0 && msg_size < (int) (sizeof(msg_buf)))
			pv_tty_write(&(state->flags), msg_buf, (size_t) msg_size);
	}
}


/*
 * Return the elapsed transfer time, given the time the transfer loop
 * started, the current time, and the total time spent stopped.
 */
static long double pv__elapsed_transfer_time(const struct timespec *loop_start_time,
					     const struct timespec *current_time,
					     const struct timespec *total_stoppage_time)
{
	struct timespec effective_start_time, transfer_elapsed;

	memset(&effective_start_time, 0, sizeof(effective_start_time));
	memset(&transfer_elapsed, 0, sizeof(transfer_elapsed));

	/*
	 * Calculate the effective start time: the time we actually started,
	 * plus the total time we spent stopped.
	 */
	pv_elapsedtime_add(&effective_start_time, loop_start_time, total_stoppage_time);

	/*
	 * Now get the effective elapsed transfer time - current time minus
	 * effective start time.
	 */
	pv_elapsedtime_subtract(&transfer_elapsed, current_time, &effective_start_time);

	return pv_elapsedtime_seconds(&transfer_elapsed);
}


/*
 * Pipe data from a list of files to standard output, giving information
 * about the transfer on standard error according to the given options.
 *
 * Returns nonzero on error.
 */
int pv_main_loop(pvstate_t state)
{
	long lineswritten;
	off_t cansend;
	ssize_t written;
	long double target;
	bool eof_in, eof_out, final_update;
	struct timespec start_time, next_update, next_ratecheck, cur_time;
	struct timespec next_remotecheck;
	int input_fd, output_fd;
	unsigned int file_idx;
	bool output_is_pipe;

	/*
	 * "written" is ALWAYS bytes written by the last transfer.
	 *
	 * "lineswritten" is the lines written by the last transfer,
	 * but is only updated in line mode.
	 *
	 * "state->transfer.total_written" is the total bytes written since
	 * the start, or in line mode, the total lines written since the
	 * start.
	 *
	 * The remaining variables are all unchanged by linemode.
	 */

	input_fd = -1;

	output_fd = state->control.output_fd;
	if (output_fd < 0)
		output_fd = STDOUT_FILENO;

	/* Determine whether the output is a pipe. */
	output_is_pipe = false;
	{
		struct stat sb;
		memset(&sb, 0, sizeof(sb));
		if (0 == fstat(output_fd, &sb)) {
			/*@-type@ */
			if ((sb.st_mode & S_IFMT) == S_IFIFO) {
				output_is_pipe = true;
				debug("%s", "output is a pipe");
			}
			/*@+type@ *//* splint says st_mode is __mode_t, not mode_t */
		} else {
			debug("%s(%d): %s", "fstat", output_fd, strerror(errno));
		}
	}

	/*
	 * Note that we could reduce the size of the output pipe buffer like
	 * this, to avoid the case where we write a load of data and it just
	 * goes into the buffer so we think we're done, but the consumer
	 * takes ages to process it:
	 *
	 *   fcntl(output_fd, F_SETPIPE_SZ, 4096);
	 *
	 * If we can peek at how much has been consumed by the other end, we
	 * don't need to.
	 */

	pv_crs_init(&(state->cursor), &(state->control), &(state->flags));

	eof_in = false;
	eof_out = false;
	state->transfer.total_written = 0;
	lineswritten = 0;
	state->display.initial_offset = 0;
	state->transfer.written_but_not_consumed = 0;

	memset(&cur_time, 0, sizeof(cur_time));
	memset(&start_time, 0, sizeof(start_time));

	pv_elapsedtime_read(&cur_time);
	pv_elapsedtime_copy(&start_time, &cur_time);

	memset(&next_ratecheck, 0, sizeof(next_ratecheck));
	memset(&next_remotecheck, 0, sizeof(next_remotecheck));
	memset(&next_update, 0, sizeof(next_update));

	pv_elapsedtime_copy(&next_ratecheck, &cur_time);
	pv_elapsedtime_copy(&next_remotecheck, &cur_time);
	pv_elapsedtime_copy(&next_update, &cur_time);
	if ((state->control.delay_start > 0)
	    && (state->control.delay_start > state->control.interval)) {
		pv_elapsedtime_add_nsec(&next_update, (long long) (1000000000.0 * state->control.delay_start));
	} else {
		pv_elapsedtime_add_nsec(&next_update, (long long) (1000000000.0 * state->control.interval));
	}

	target = 0;
	final_update = false;
	file_idx = 0;

	/*
	 * Open the first readable input file.
	 */
	input_fd = -1;
	while (input_fd < 0 && file_idx < state->files.file_count) {
		input_fd = pv_next_file(state, file_idx, -1);
		if (input_fd < 0)
			file_idx++;
	}

	/*
	 * Exit early if there was no readable input file.
	 */
	if (input_fd < 0) {
		if (state->control.cursor)
			pv_crs_fini(&(state->cursor), &(state->control), &(state->flags));
		return state->status.exit_status;
	}
#if HAVE_POSIX_FADVISE
	/* Advise the OS that we will only be reading sequentially. */
	(void) posix_fadvise(input_fd, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif

#ifdef O_DIRECT
	/*
	 * Set or clear O_DIRECT on the output.
	 */
	if (0 != fcntl(output_fd, F_SETFL, (state->control.direct_io ? O_DIRECT : 0) | fcntl(output_fd, F_GETFL))) {
		debug("%s: %s", "fcntl", strerror(errno));
	}
	state->control.direct_io_changed = false;
#endif				/* O_DIRECT */

#if HAVE_STRUCT_STAT_ST_BLKSIZE
	/*
	 * Set target buffer size if the initial file's block size can be
	 * read and we weren't given a target buffer size.
	 */
	if (0 == state->control.target_buffer_size) {
		struct stat sb;
		memset(&sb, 0, sizeof(sb));
		if (0 == fstat(input_fd, &sb)) {
			size_t sz;
			sz = (size_t) (sb.st_blksize * 32);
			if (sz > BUFFER_SIZE_MAX) {
				sz = BUFFER_SIZE_MAX;
			}
			state->control.target_buffer_size = sz;
		}
	}
#endif

	if (0 == state->control.target_buffer_size)
		state->control.target_buffer_size = BUFFER_SIZE;

	/*
	 * Repeat until eof_in is true, eof_out is true, and final_update is
	 * true.
	 */

	while ((!(eof_in && eof_out)) || (!final_update)) {

		cansend = 0;

		/*
		 * Check for remote messages from -R, -Q every short while.
		 */
		if (pv_elapsedtime_compare(&cur_time, &next_remotecheck) > 0) {
			(void) pv_remote_check(state);
			pv_elapsedtime_add_nsec(&next_remotecheck, REMOTE_INTERVAL);
		}

		if (1 == state->flags.trigger_exit)
			break;

		if (state->control.rate_limit > 0) {
			pv_elapsedtime_read(&cur_time);
			if (pv_elapsedtime_compare(&cur_time, &next_ratecheck) > 0) {
				target +=
				    ((long double) (state->control.rate_limit)) / (long double) (1000000000.0 /
												 (long double)
												 (RATE_GRANULARITY));
				long double burst_max = ((long double) (state->control.rate_limit * RATE_BURST_WINDOW));
				if (target > burst_max) {
					target = burst_max;
				}
				pv_elapsedtime_add_nsec(&next_ratecheck, RATE_GRANULARITY);
			}
			cansend = (off_t) target;
		}

		/*
		 * If we have to stop at "size" bytes, make sure we don't
		 * try to write more than we're allowed to.
		 */
		if ((0 < state->control.size) && (state->control.stop_at_size)) {
			if ((state->control.size < (state->transfer.total_written + cansend))
			    || ((0 == cansend)
				&& (0 == state->control.rate_limit))) {
				cansend = state->control.size - state->transfer.total_written;
				if (0 >= cansend) {
					debug("%s", "write limit reached (size explicitly set) - setting EOF flags");
					eof_in = true;
					eof_out = true;
				}
			}
		}

		if ((0 < state->control.size) && (state->control.stop_at_size)
		    && (0 >= cansend) && eof_in && eof_out) {
			written = 0;
		} else {
			written = pv_transfer(state, input_fd, &eof_in, &eof_out, cansend, &lineswritten);
		}

		/* End on write error. */
		if (written < 0) {
			debug("%s: %s", "write error from pv_transfer", strerror(errno));
			if (state->control.cursor)
				pv_crs_fini(&(state->cursor), &(state->control), &(state->flags));
			return state->status.exit_status;
		}

		if (state->control.linemode) {
			state->transfer.total_written += lineswritten;
			if (state->control.rate_limit > 0)
				target -= lineswritten;
		} else {
			state->transfer.total_written += written;
			if (state->control.rate_limit > 0)
				target -= written;
		}

#ifdef FIONREAD
		/*
		 * If writing to a pipe, look at how much is sitting in the
		 * pipe buffer waiting for the receiver to read.
		 */
		if (output_is_pipe) {
			int nbytes;
			nbytes = 0;
			if (0 != state->flags.pipe_closed) {
				if (0 != state->transfer.written_but_not_consumed)
					debug("%s",
					      "clearing written_but_not_consumed because the output pipe was closed");
				state->transfer.written_but_not_consumed = 0;
			} else if (0 == ioctl(output_fd, FIONREAD, &nbytes)) {
				if (nbytes >= 0) {
					if (((size_t) nbytes) != state->transfer.written_but_not_consumed)
						debug("%s: %d", "written_but_not_consumed is now", nbytes);
					state->transfer.written_but_not_consumed = (size_t) nbytes;
				} else {
					debug("%s: %d", "FIONREAD gave a negative byte count", nbytes);
					state->transfer.written_but_not_consumed = 0;
				}
			} else {
				debug("%s(%d,%s): %s", "ioctl", output_fd, "FIONREAD", strerror(errno));
				state->transfer.written_but_not_consumed = 0;
			}
		}
#endif

		state->transfer.transferred = state->transfer.total_written;
		if (output_is_pipe && !state->control.linemode) {
			/*
			 * Writing bytes to a pipe - the amount transferred
			 * to the receiver is the total amount we've
			 * written, minus what's sitting in the pipe buffer
			 * waiting for the receiver to consume it.
			 */
			state->transfer.transferred -= state->transfer.written_but_not_consumed;

		} else if (output_is_pipe && state->control.linemode && state->transfer.written_but_not_consumed > 0
			   && NULL != state->transfer.line_positions) {
			/*
			 * Writing lines to a pipe - similar to above, but
			 * we have to work out how many lines the
			 * yet-to-be-consumed data in the buffer equates to.
			 *
			 * To do this, we walk backwards through our record
			 * of the line positions in the output we've
			 * written.
			 */
			off_t last_consumed_position =
			    state->transfer.last_output_position - state->transfer.written_but_not_consumed;
			size_t lines_not_consumed = 0;
			size_t line_from_end = 0;

			/*
			 * positions[head-1] = position of last separator written
			 * positions[head-2] = position of second last separator written
			 * etc
			 *
			 * We start at [head-1] and go backwards, wrapping
			 * around as it's a circular buffer, stopping at the
			 * length (number of positions stored), or when we
			 * have gone before the last consumed position.
			 */
			for (line_from_end = 0; line_from_end < state->transfer.line_positions_length; line_from_end++) {
				size_t array_index;
				array_index =
				    state->transfer.line_positions_head + state->transfer.line_positions_capacity -
				    line_from_end - 1;
				while (array_index >= state->transfer.line_positions_capacity)
					array_index -= state->transfer.line_positions_capacity;
				if (state->transfer.line_positions[array_index] <= last_consumed_position)
					break;
				lines_not_consumed++;
			}

			debug("%s: %lld -> %lld", "written_but_not_consumed bytes to lines",
			      (unsigned long long) (state->transfer.written_but_not_consumed),
			      (unsigned long long) lines_not_consumed);

			state->transfer.transferred -= lines_not_consumed;
		}

		/*
		 * EOF, and files remain - advance to the next file.
		 */
		while (eof_in && eof_out && file_idx < (state->files.file_count - 1)) {
			file_idx++;
			input_fd = pv_next_file(state, file_idx, input_fd);
			if (input_fd >= 0) {
				eof_in = false;
				eof_out = false;
#if HAVE_POSIX_FADVISE
				/* Advise the OS that we will only be reading sequentially. */
				(void) posix_fadvise(input_fd, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif
			}
		}

		/* Now check the current time. */
		pv_elapsedtime_read(&cur_time);

		/*
		 * If we've read everything and written everything, and the
		 * output pipe buffer is empty, then set the final update
		 * flag, and force a display update.
		 */
		if (eof_in && eof_out && 0 == state->transfer.written_but_not_consumed) {
			final_update = true;
			if ((state->display.output_produced)
			    || (state->control.delay_start < 0.001)) {
				pv_elapsedtime_copy(&next_update, &cur_time);
			}
		}

		/*
		 * If we've read everything and written everything, and the
		 * output pipe buffer is NOT empty, then pause a short while
		 * so we don't spin in a tight loop waiting for the output
		 * buffer to empty (#164).
		 */
		if (eof_in && eof_out && state->transfer.written_but_not_consumed > 0) {
			debug("%s", "EOF but bytes remain in output pipe - sleeping");
			pv_nanosleep(50000000);
		}

		/*
		 * If -W was given, we don't output anything until we have
		 * written a byte (or line, in line mode), at which point
		 * we then count time as if we started when the first byte
		 * was received.
		 */
		if (state->control.wait) {
			/* Restart the loop if nothing written yet. */
			if (state->control.linemode) {
				if (lineswritten < 1)
					continue;
			} else {
				if (written < 1)
					continue;
			}

			state->control.wait = false;

			/*
			 * Reset the timer offset counter now that data
			 * transfer has begun, otherwise if we had been
			 * stopped and started (with ^Z / SIGTSTOP)
			 * previously (while waiting for data), the timers
			 * will be wrongly offset.
			 *
			 * While we reset the offset counter we must disable
			 * SIGTSTOP so things don't mess up.
			 */
			pv_sig_nopause();
			pv_elapsedtime_read(&start_time);
			pv_elapsedtime_zero(&(state->signal.total_stoppage_time));
			pv_sig_allowpause();

			/*
			 * Start the display, but only at the next interval,
			 * not immediately.
			 */
			pv_elapsedtime_copy(&next_update, &start_time);
			pv_elapsedtime_add_nsec(&next_update, (long long) (1000000000.0 * state->control.interval));
		}

		/* Calculate the elapsed transfer time. */
		state->transfer.elapsed_seconds =
		    pv__elapsed_transfer_time(&start_time, &cur_time, &(state->signal.total_stoppage_time));

		/*
		 * Just go round the loop again if there's no display and
		 * we're not reporting statistics.
		 */
		if (state->control.no_display && !state->control.show_stats) {
			continue;
		}

		/* Restart the loop if it's not time to update the display. */
		if (pv_elapsedtime_compare(&cur_time, &next_update) < 0) {
			continue;
		}

		pv_elapsedtime_add_nsec(&next_update, (long long) (1000000000.0 * state->control.interval));

		/* Set the "next update" time to now, if it's in the past. */
		if (pv_elapsedtime_compare(&next_update, &cur_time) < 0)
			pv_elapsedtime_copy(&next_update, &cur_time);

		/* Resize the display, if a resize signal was received. */
		(void) pv__resize_display_on_signal(state);

		if (state->control.no_display) {
			/* If there's no display, calculate rate for the statistics. */
			pv_calculate_transfer_rate(&(state->calc), &(state->transfer), &(state->control),
						   &(state->display), final_update);
		} else {
			/* Produce the display. */
			pv_display(&(state->status), &(state->control), &(state->flags), &(state->transfer),
				   &(state->calc), &(state->cursor), &(state->display), &(state->extra_display),
				   final_update);
		}
	}

	debug("%s: %s=%s, %s=%s", "loop ended", "eof_in", eof_in ? "true" : "false", "eof_out",
	      eof_out ? "true" : "false");

	if (state->control.cursor) {
		pv_crs_fini(&(state->cursor), &(state->control), &(state->flags));
	} else {
		if ((!state->control.numeric) && (!state->control.no_display)
		    && (state->display.output_produced))
			pv_tty_write(&(state->flags), "\n", 1);
	}

	if (1 == state->flags.trigger_exit)
		state->status.exit_status |= PV_ERROREXIT_SIGNAL;

	if (input_fd >= 0)
		(void) close(input_fd);

	/* Calculate and display the transfer statistics. */
	pv__show_stats(state);

	return state->status.exit_status;
}


/*
 * Return true if the given string contains a "%N" or "%{name}" format
 * sequence.
 */
static bool format_contains_name(const char *format_string)
{
	while ('\0' != format_string[0]) {
		/* If we're not on a '%' character, move on. */
		if ('%' != format_string[0]) {
			format_string++;
			continue;
		}
		/* Move past the '%'. */
		format_string++;
		/* Early return if we've run out of string. */
		if ('\0' == format_string[0])
			return false;
		/* If we've found "%N", return true. */
		if ('N' == format_string[0])
			return true;
		/* If we've found "%%", move on and go back round the loop. */
		if ('%' == format_string[0]) {
			format_string++;
			continue;
		}
		/* Move past any digits, for the "%123{name}" syntax. */
		while ('\0' != format_string[0] && (format_string[0] >= '0' && format_string[0] <= '9'))
			format_string++;
		/*
		 * If the next character isn't '{', this isn't a valid
		 * "%{xyz}" sequence, so go back to the top of the loop to
		 * skip to the next '%'.
		 */
		if ('{' != format_string[0])
			continue;
		/*
		 * If the next part is "{name}", forming "%{name}", return
		 * true.
		 */
		if (0 == strncmp(format_string, "{name}", 6))
			return true;
		/* Loop round again to keep looking. */
	}
	return false;
}


/*
 * Update the main format string for use with --watchfd, such that if
 * necessary, it starts with "%N " if it doesn't already.
 */
static void pv_watchfd_update_format_string(pvstate_t state)
{
	const char *original_format_string;
	char new_format_string[512];	 /* flawfinder: ignore */

	/*
	 * flawfinder rationale (new_format_string): zeroed with memset(),
	 * only written to with pv_snprintf() which checks boundaries, and
	 * explicitly terminated with \0.
	 */

	/* If there's nothing to watch, do nothing. */
	if (state->watchfd.count < 1)
		return;
	if (NULL == state->watchfd.watching)
		return;

	/*
	 * Make sure there's a format string, and then insert %N into it if
	 * it's not present AND we're either watching more than one item, or
	 * the item we're watching is a whole PID, not a single FD.
	 */
	original_format_string = state->control.format_string;
	if (NULL == original_format_string)
		original_format_string = state->control.default_format;
	if (NULL == original_format_string)
		return;

	memset(new_format_string, 0, sizeof(new_format_string));

	if (state->watchfd.count > 1 || -1 == state->watchfd.watching[0].fd) {
		/* Watching more than one single FD; need %N. */
		if (!format_contains_name(original_format_string)) {
			(void) pv_snprintf(new_format_string, sizeof(new_format_string), "%%N %s",
					   original_format_string);
		} else {
			/* If the format string is set, nothing to do. */
			if (NULL != state->control.format_string)
				return;
			/* Prepare a copy of the default format. */
			(void) pv_snprintf(new_format_string, sizeof(new_format_string), "%s", original_format_string);
		}
	} else {
		/* Watching only one FD; don't prepend %N. */
		/* If the format string is set, nothing to do. */
		if (NULL != state->control.format_string)
			return;
		/* Prepare a copy of the default format. */
		(void) pv_snprintf(new_format_string, sizeof(new_format_string), "%s", original_format_string);
	}

	new_format_string[sizeof(new_format_string) - 1] = '\0';

	if (NULL != state->control.format_string)
		free(state->control.format_string);
	state->control.format_string = pv_strdup(new_format_string);
}


/*
 * Watch the progress of the PID:FD pairs, or of all FDs under PIDs, as
 * specified in state->watchfd, showing details on standard error according
 * to the given options.
 *
 * Replaces format_string in "state" so that starts with "%N " if it doesn't
 * already do so.
 *
 * Returns nonzero on error.
 */
int pv_watchfd_loop(pvstate_t state)
{
	struct pvwatcheditem_s *watching;
	unsigned int watch_idx;
	bool all_watching_finished;
	struct timespec next_update, next_remotecheck, cur_time;
	int prev_displayed_lines, blank_lines;

	/*
	 * In each watching[].info_array[] fd info item, once its "watch_fd"
	 * is closed, its "closed" flag is set to true and its "end_time" to
	 * now.  It isn't marked "unused" until its "end_time" is long
	 * enough ago, so the information is held on-screen for a short
	 * while after the fd closes or the PID exits (#81).
	 */

	/* If there's nothing to watch, do nothing at all. */
	if (state->watchfd.count < 1)
		return 0;
	if (NULL == state->watchfd.watching)
		return PV_ERROREXIT_MEMORY;

	/* Local alias for less cumbersome access. */
	watching = state->watchfd.watching;

	/*
	 * Make sure there's no name set.
	 */
	if (NULL != state->control.name) {
		free(state->control.name);
		state->control.name = NULL;
	}

	/* Adjust the format string so PID:FD or FD prefixes are shown. */
	pv_watchfd_update_format_string(state);

	/*
	 * Populate the watching array.  In the process of doing so, raise
	 * errors if any of the initially specified PIDs or PID:FD pairs
	 * don't exist or aren't readable.
	 */
	for (watch_idx = 0; watch_idx < state->watchfd.count; watch_idx++) {
		int rc;

		watching[watch_idx].info_array = NULL;
		watching[watch_idx].array_length = 0;
		watching[watch_idx].finished = false;

		if (kill(watching[watch_idx].pid, 0) != 0) {
			/* Inaccessible PID - error, mark as finished. */
			pv_error("%s %u: %s", _("pid"), watching[watch_idx].pid, strerror(errno));
			state->status.exit_status |= PV_ERROREXIT_ACCESS;
			watching[watch_idx].finished = true;
			continue;
		}

		if (-1 == watching[watch_idx].fd) {
			/*
			 * If this is a whole-PID watch, do the initial FD
			 * scan to list all the FDs under that PID.
			 */
			rc = pv_watchpid_scanfds(state, watching[watch_idx].pid, -1,
						 &(watching[watch_idx].array_length),
						 &(watching[watch_idx].info_array));
			if (rc != 0) {
				/* Scan failed - error, mark as finished. */
				pv_error("%s %u: %s", _("pid"), watching[watch_idx].pid, strerror(errno));
				state->status.exit_status |= PV_ERROREXIT_ACCESS;
				watching[watch_idx].finished = true;
			}
		} else {
			/*
			 * Scan the PID for the one specific FD given.
			 */
			rc = pv_watchpid_scanfds(state, watching[watch_idx].pid, watching[watch_idx].fd,
						 &(watching[watch_idx].array_length),
						 &(watching[watch_idx].info_array));
			if (rc != 0) {
				/* Scan failed - mark as finished. */
				state->status.exit_status |= PV_ERROREXIT_ACCESS;
				watching[watch_idx].finished = true;
			} else if ((0 == watching[watch_idx].array_length) || (NULL == watching[watch_idx].info_array)) {
				/* FD not found - error, mark as finished. */
				pv_error("%s %u: %s %d: %s",
					 _("pid"), watching[watch_idx].pid, _("fd"), watching[watch_idx].fd,
					 strerror(ENOENT));
				state->status.exit_status |= PV_ERROREXIT_ACCESS;
				watching[watch_idx].finished = true;
			} else if (!watching[watch_idx].info_array[0].displayable) {
				/* FD not displayable - mark as finished. */
				state->status.exit_status |= PV_ERROREXIT_ACCESS;
				watching[watch_idx].finished = true;
			}
		}
	}

	/*
	 * Skip to the end if none of the items to watch can be watched.
	 */
	all_watching_finished = true;
	for (watch_idx = 0; watch_idx < state->watchfd.count; watch_idx++) {
		if (watching[watch_idx].finished)
			continue;
		all_watching_finished = false;
		break;
	}
	if (all_watching_finished)
		goto end_pv_watchfd_loop;

	/*
	 * Prepare timing structures for the main loop.
	 */

	memset(&cur_time, 0, sizeof(cur_time));
	memset(&next_remotecheck, 0, sizeof(next_remotecheck));
	memset(&next_update, 0, sizeof(next_update));

	pv_elapsedtime_read(&cur_time);
	pv_elapsedtime_copy(&next_remotecheck, &cur_time);
	pv_elapsedtime_copy(&next_update, &cur_time);
	pv_elapsedtime_add_nsec(&next_update, (long long) (1000000000.0 * state->control.interval));

	/*
	 * Main loop - continually check each watching[] item and display
	 * progress until all watched items have finished or an exit signal
	 * is received.
	 */

	prev_displayed_lines = 0;
	all_watching_finished = false;

	while (!all_watching_finished) {
		int displayed_lines;
		bool terminal_resized;

		/* Check for remote messages from -R, -Q every short while. */
		if (pv_elapsedtime_compare(&cur_time, &next_remotecheck) > 0) {
			if (pv_remote_check(state)) {
				/* Message received - ensure format has %N. */
				pv_watchfd_update_format_string(state);
				/* Fake a resize, to force a reparse. */
				state->flags.terminal_resized = 1;
			}
			pv_elapsedtime_add_nsec(&next_remotecheck, REMOTE_INTERVAL);
		}

		/* End the loop if a signal handler has set the exit flag. */
		if (1 == state->flags.trigger_exit)
			break;

		/* Get the current time. */
		pv_elapsedtime_read(&cur_time);

		/*
		 * Restart the loop after a brief delay, if it's not time to
		 * update the display.
		 */
		if (pv_elapsedtime_compare(&cur_time, &next_update) < 0) {
			pv_nanosleep(50000000);
			continue;
		}

		/* Set the time of the next display update. */
		pv_elapsedtime_add_nsec(&next_update, (long long) (1000000000.0 * state->control.interval));

		/* Set the "next update" time to now, if it's in the past. */
		if (pv_elapsedtime_compare(&next_update, &cur_time) < 0)
			pv_elapsedtime_copy(&next_update, &cur_time);

		/*
		 * Resize the display, if a resize signal was received.
		 *
		 * We also set a local flag to tell the display loop below
		 * to trigger a name reset and display reparse for every FD
		 * output line, to inherit the size change.
		 */
		terminal_resized = pv__resize_display_on_signal(state);

		/*
		 * Run through each watched item.
		 */
		displayed_lines = 0;
		for (watch_idx = 0; NULL != watching && watch_idx < state->watchfd.count; watch_idx++) {
			int info_idx;

			/* Skip watched items that have finished. */
			if (watching[watch_idx].finished)
				continue;

			if (-1 == watching[watch_idx].fd) {
				int rc;
				/*
				 * If this watched item is a whole PID,
				 * rescan that PID's FDs.
				 */
				rc = pv_watchpid_scanfds(state, watching[watch_idx].pid, -1,
							 &(watching[watch_idx].array_length),
							 &(watching[watch_idx].info_array));
				if (rc != 0) {
					/*
					 * PID now inaccessible - mark it as
					 * finished.
					 */
					watching[watch_idx].finished = true;
				}
			} else {
				/*
				 * It this watched item is a single FD, and
				 * that FD is no longer usable, mark the
				 * item as finished and move on.
				 */
				if ((NULL == watching[watch_idx].info_array) || (0 == watching[watch_idx].array_length)
				    || (watching[watch_idx].info_array[0].unused)
				    || (!watching[watch_idx].info_array[0].displayable)) {
					watching[watch_idx].finished = true;
					continue;
				}
			}

			/*
			 * Run through the info array of this item.
			 */
			for (info_idx = 0;
			     NULL != watching[watch_idx].info_array && info_idx < watching[watch_idx].array_length;
			     info_idx++) {
				off_t position_now;
				pvwatchfd_t info_item = &(watching[watch_idx].info_array[info_idx]);

				/* Skip unused array entries. */
				if (info_item->unused)
					continue;

				/* No more lines if we've filled the display. */
				if (displayed_lines >= (int) (state->control.height))
					break;

				if (!info_item->displayable) {
					/*
					 * Non-displayable fd - just remove if
					 * changed.
					 */
					if (pv_watchfd_changed(info_item)) {
						debug("%s %d: %s", "fd", info_item->watch_fd,
						      "non-displayable, and has changed - removing");
						info_item->unused = true;
						info_item->displayable = false;
						pv_freecontents_watchfd(info_item);
					}
					continue;
				}

				if (info_item->watch_fd < 0) {
					debug("%s %d: %s", "fd", info_item->watch_fd, "negative fd - skipping");
					continue;
				}

				/*
				 * Displayable fd - display, or remove if
				 * changed.
				 */

				position_now = -1;

				if (info_item->closed) {
					/*
					 * Closed fd - check how long since
					 * it was closed.
					 */
					struct timespec time_since_closed;
					long double seconds_since_closed;

					memset(&time_since_closed, 0, sizeof(time_since_closed));
					pv_elapsedtime_subtract(&time_since_closed, &cur_time, &(info_item->end_time));
					seconds_since_closed = pv_elapsedtime_seconds(&time_since_closed);

					/* Closed for long enough - remove. */
					if (seconds_since_closed > state->control.interval) {
						debug("%s %d: %s (%Lf s)", "fd", info_item->watch_fd,
						      "closed for long enough - removing", seconds_since_closed);
						info_item->unused = true;
						info_item->displayable = false;
						pv_freecontents_watchfd(info_item);
						continue;
					}

				} else {

					/*
					 * Open fd - get its current
					 * position.
					 */

					position_now = pv_watchfd_position(info_item);

					if (position_now < 0) {
						/*
						 * The fd was closed - mark
						 * as closed.
						 */
						debug("%s %d: %s", "fd", info_item->watch_fd, "marking as closed");
						pv_elapsedtime_copy(&(info_item->end_time), &cur_time);
						info_item->closed = true;
					}
				}

				if (position_now >= 0) {
					/*
					 * If the fd is still open and we
					 * got its current position, update
					 * its position and timers.
					 */
					info_item->position = position_now;
					info_item->transfer.elapsed_seconds =
					    pv__elapsed_transfer_time(&(info_item->start_time), &cur_time,
								      &(info_item->total_stoppage_time));
				}

				/*
				 * Now display the information about the fd.
				 */

				if (displayed_lines > 0) {
					debug("%s", "adding newline");
					pv_tty_write(&(state->flags), "\n", 1);
				}

				debug("%s %d, %s %d [%d/%d]: %Lf / %Ld", "pid", (int) (info_item->watch_pid), "fd",
				      info_item->watch_fd, watch_idx, info_idx, info_item->transfer.elapsed_seconds,
				      info_item->position);

				if (terminal_resized) {
					pv_watchpid_setname(state, info_item);
					info_item->flags.reparse_display = 1;
				}

				info_item->transfer.transferred = info_item->position;
				info_item->transfer.total_written = info_item->position;
				state->control.name = info_item->display_name;
				state->control.size = info_item->size;

				pv_display(&(state->status),
					   &(state->control), &(info_item->flags),
					   &(info_item->transfer), &(info_item->calc),
					   &(state->cursor), &(info_item->display), NULL, false);

				/*@-mustfreeonly@ */
				state->control.name = NULL;
				/*
				 * splint warns of a memory leak, but we'd
				 * set name to be an alias of display_name,
				 * so nothing is lost here.
				 */
				/*@+mustfreeonly@ */

				displayed_lines++;
			}

		}

		/*
		 * Write blank lines if we're writing fewer lines than last
		 * time.
		 */
		blank_lines = prev_displayed_lines - displayed_lines;
		prev_displayed_lines = displayed_lines;

		if (blank_lines > 0)
			debug("%s: %d", "adding blank lines", blank_lines);

		while (blank_lines > 0) {
			pvdisplay_width_t blank_count;
			if (displayed_lines > 0)
				pv_tty_write(&(state->flags), "\n", 1);
			for (blank_count = 0; blank_count < state->control.width; blank_count++)
				pv_tty_write(&(state->flags), " ", 1);
			pv_tty_write(&(state->flags), "\r", 1);
			blank_lines--;
			displayed_lines++;
		}

		debug("%s: %d", "displayed lines", displayed_lines);

		while (displayed_lines > 1) {
			pv_tty_write(&(state->flags), "\033[A", 3);
			displayed_lines--;
		}

		/* Check whether all watched items have finished. */
		all_watching_finished = true;
		for (watch_idx = 0; watch_idx < state->watchfd.count; watch_idx++) {
			if (watching[watch_idx].finished)
				continue;
			all_watching_finished = false;
			break;
		}
	}

	if (!state->control.numeric) {
		/*
		 * Move the cursor past the displayed lines, so the last
		 * output stays on the screen and isn't overwritten by the
		 * command prompt (#81).
		 */
		while (prev_displayed_lines > 0) {
			pv_tty_write(&(state->flags), "\n", 1);
			prev_displayed_lines--;
		}
	}

	/*
	 * If a signal caused the end of the loop, reflect that in the
	 * return code.
	 */
	if (1 == state->flags.trigger_exit)
		state->status.exit_status |= PV_ERROREXIT_SIGNAL;

      end_pv_watchfd_loop:
	/* Free all allocated sub-structures. */
	pv_freecontents_watchfd_items(watching, state->watchfd.count);

	return state->status.exit_status;
}


/*
 * Watch the progress of another pv process.
 *
 * Returns nonzero on error.
 */
int pv_query_loop(pvstate_t state, pid_t query)
{
	struct timespec cur_time, next_remotecheck, next_update;

	pv_crs_init(&(state->cursor), &(state->control), &(state->flags));

	/*
	 * NB transfer.total_written has been initialised by main().
	 */

	state->display.initial_offset = 0;

	memset(&cur_time, 0, sizeof(cur_time));
	memset(&next_remotecheck, 0, sizeof(next_remotecheck));
	memset(&next_update, 0, sizeof(next_update));

	pv_elapsedtime_read(&cur_time);
	pv_elapsedtime_copy(&next_remotecheck, &cur_time);
	pv_elapsedtime_copy(&next_update, &cur_time);

	if ((state->control.delay_start > 0)
	    && (state->control.delay_start > state->control.interval)) {
		pv_elapsedtime_add_nsec(&next_update, (long long) (1000000000.0 * state->control.delay_start));
	} else {
		pv_elapsedtime_add_nsec(&next_update, (long long) (1000000000.0 * state->control.interval));
	}

	/*
	 * Repeat until the queried process exits.
	 */

	while (0 == kill(query, 0)) {

		/*
		 * Check for remote messages from -R, and run the query,
		 * every short while.
		 */
		if (pv_elapsedtime_compare(&cur_time, &next_remotecheck) > 0) {
			if (0 != pv_remote_transferstate_fetch(state, query, NULL, true))
				break;
			(void) pv_remote_check(state);
			pv_elapsedtime_add_nsec(&next_remotecheck, REMOTE_INTERVAL);
			/*
			 * Set the next-remote-check time to now +
			 * REMOTE_INTERVAL, if it's still in the past.
			 */
			if (pv_elapsedtime_compare(&next_update, &cur_time) < 0) {
				pv_elapsedtime_copy(&next_update, &cur_time);
				pv_elapsedtime_add_nsec(&next_remotecheck, REMOTE_INTERVAL);
			}
		}

		if (1 == state->flags.trigger_exit)
			break;

		/* Now check the current time. */
		pv_elapsedtime_read(&cur_time);

		/*
		 * Just go round the loop again if there's no display and
		 * we're not reporting statistics.
		 */
		if (state->control.no_display && !state->control.show_stats) {
			pv_nanosleep(50000000);
			continue;
		}

		/*
		 * If -W was given, we don't output anything until something
		 * has been transferred.
		 */
		if (state->control.wait) {
			/* Restart the loop if nothing written yet. */
			if (state->transfer.transferred < 1) {
				pv_nanosleep(50000000);
				continue;
			}

			state->control.wait = false;

			/*
			 * Start the display at the next interval.
			 */
			pv_elapsedtime_copy(&next_update, &cur_time);
			pv_elapsedtime_add_nsec(&next_update, (long long) (1000000000.0 * state->control.interval));
		}

		/* Restart the loop if it's not time to update the display. */
		if (pv_elapsedtime_compare(&cur_time, &next_update) < 0) {
			pv_nanosleep(50000000);
			continue;
		}

		pv_elapsedtime_add_nsec(&next_update, (long long) (1000000000.0 * state->control.interval));

		/* Set the "next update" time to now, if it's in the past. */
		if (pv_elapsedtime_compare(&next_update, &cur_time) < 0)
			pv_elapsedtime_copy(&next_update, &cur_time);

		/* Resize the display, if a resize signal was received. */
		(void) pv__resize_display_on_signal(state);

		if (state->control.no_display) {
			/* If there's no display, calculate rate for the statistics. */
			pv_calculate_transfer_rate(&(state->calc), &(state->transfer), &(state->control),
						   &(state->display), false);
		} else {
			/* Produce the display. */
			pv_display(&(state->status), &(state->control), &(state->flags), &(state->transfer),
				   &(state->calc), &(state->cursor), &(state->display), &(state->extra_display), false);
		}
	}

	if (state->control.cursor) {
		pv_crs_fini(&(state->cursor), &(state->control), &(state->flags));
	} else {
		if ((!state->control.numeric) && (!state->control.no_display)
		    && (state->display.output_produced))
			pv_tty_write(&(state->flags), "\n", 1);
	}

	if (1 == state->flags.trigger_exit)
		state->status.exit_status |= PV_ERROREXIT_SIGNAL;

	/* Calculate and display the transfer statistics. */
	pv__show_stats(state);

	return state->status.exit_status;
}
