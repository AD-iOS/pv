/*
 * Functions for transferring data between file descriptors.
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
#include <time.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>

/*
 * splint note: In a few places we use "#if SPLINT" to substitute other code
 * while analysing with splint, to work around the issues it has with
 * FD_ZERO, FD_SET, FD_ISSET - these macros expand to code it does not like,
 * such as using << with an fd which may be negative, or comparing an
 * unsigned integer with a size_t, and it doesn't seem to work to turn off
 * those specific warnings where these macros are used.
 */

/*
 * Return >0 if data is ready to read on fd_in, or write on fd_out, before
 * "usec" microseconds have elapsed, 0 if not, or negative on error.  Either
 * or both of "fd_in" and "fd_out" may be negative to ignore that side.  If
 * fd_in_ready and/or fd_out_ready are not NULL, they will be populated with
 * true or false depending on whether data is ready on those sides.
 */
static int is_data_ready(int fd_in, /*@null@ */ bool *fd_in_ready, int fd_out, /*@null@ */ bool *fd_out_ready,
			 long usec)
{
	struct timeval tv;
	fd_set readfds;
	fd_set writefds;
	fd_set exceptfds;
	int max_fd;
	int result;

	max_fd = -1;
	if (fd_in > max_fd)
		max_fd = fd_in;
	if (fd_out > max_fd)
		max_fd = fd_out;

	memset(&tv, 0, sizeof(tv));

#if SPLINT
	/* splint doesn't like FD_ZERO and FD_SET. */
	memset(&readfds, 0, sizeof(readfds));
	memset(&writefds, 0, sizeof(writefds));
	memset(&exceptfds, 0, sizeof(exceptfds));
#else				/* !SPLINT */
	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_ZERO(&exceptfds);
	if (fd_in >= 0)
		FD_SET(fd_in, &readfds);
	if (fd_out >= 0)
		FD_SET(fd_out, &writefds);
#endif				/* !SPLINT */

	tv.tv_sec = usec / 1000000;
	tv.tv_usec = usec % 1000000;

	if (NULL != fd_in_ready)
		*fd_in_ready = false;
	if (NULL != fd_out_ready)
		*fd_out_ready = false;

	result = select(max_fd + 1, &readfds, &writefds, &exceptfds, &tv);

	if (result > 0) {
		if ((fd_in >= 0) && (NULL != fd_in_ready)
#ifndef SPLINT
		    && (FD_ISSET(fd_in, &readfds))
#endif
		    ) {
			*fd_in_ready = true;
		}
		if ((fd_out >= 0) && (NULL != fd_out_ready)
#ifndef SPLINT
		    && (FD_ISSET(fd_out, &writefds))
#endif
		    ) {
			*fd_out_ready = true;
		}
	}

	return result;
}


/*
 * Read up to "count" bytes from file descriptor "fd" into the buffer "buf",
 * and return the number of bytes read, like read().
 *
 * Unlike read(), if we have read less than "count" bytes, we check to see
 * if there's any more to read, and keep trying, to make sure we fill the
 * buffer as full as we can.
 *
 * We stop retrying if the time elapsed since this function was entered
 * reaches TRANSFER_READ_TIMEOUT seconds.
 */
static ssize_t pv__transfer_read_repeated(int fd, char *buf, size_t count)
{
	struct timespec start_time;
	ssize_t total_read;

	memset(&start_time, 0, sizeof(start_time));

	pv_elapsedtime_read(&start_time);

	total_read = 0;

	while (count > 0) {
		ssize_t nread;
		struct timespec cur_time, transfer_elapsed;
		long double elapsed_seconds;

		nread = read(fd, buf, (size_t) (count > MAX_READ_AT_ONCE ? MAX_READ_AT_ONCE : count));	/* flawfinder: ignore */

		/*
		 * flawfinder rationale: reads stop after "count" bytes, and
		 * we handle negative and zero results from read(), so it is
		 * bounded to the buffer size the caller told us to use.
		 */

		if (nread < 0)
			return nread;

		total_read += nread;
		buf += nread;
		count -= nread;

		if (0 == nread)
			return total_read;

		memset(&cur_time, 0, sizeof(cur_time));
		memset(&transfer_elapsed, 0, sizeof(transfer_elapsed));
		elapsed_seconds = 0.0;

		pv_elapsedtime_read(&cur_time);
		pv_elapsedtime_subtract(&transfer_elapsed, &cur_time, &start_time);
		elapsed_seconds = pv_elapsedtime_seconds(&transfer_elapsed);

		if (elapsed_seconds > TRANSFER_READ_TIMEOUT) {
			debug("%s %d: %s (%f %s)", "fd", fd,
			      "stopping read - timer expired", (double) elapsed_seconds, "sec elapsed");
			return total_read;
		}

		if (count > 0) {
			debug("%s %d: %s (%ld %s, %ld %s)", "fd", fd,
			      "trying another read after partial buffer fill", nread, "read", count, "remaining");
			if (is_data_ready(fd, NULL, -1, NULL, 0) < 1)
				break;
		}
	}

	return total_read;
}


/*
 * Write up to "count" bytes to file descriptor "fd" from the buffer "buf",
 * and return the number of bytes written, like write().
 *
 * Unlike write(), if we have written less than "count" bytes, we check to
 * see if we can write any more, and keep trying, to make sure we empty the
 * buffer as much as we can.
 *
 * While this is called after a successful write-possible select(), write() is
 * not guaranteed to succeed for _all_ sizes; we may end up returning 0 if this
 * occurs. (The first write() may return -1 / EINTR if the consumer doesn't
 * read any data before our timeout and the buffer of whatever stdout is is
 * near-full.) (see https://codeberg.org/ivarch/pv/pulls/93)
 *
 * If "sync_after_write" is true, we call fdatasync() after each write() (or
 * fsync() if _POSIX_SYNCHRONIZED_IO is not > 0).
 *
 * We stop retrying if the time elapsed since this function was entered
 * reaches TRANSFER_WRITE_TIMEOUT seconds.
 */
static ssize_t pv__transfer_write_repeated(int fd, char *buf, size_t count, bool sync_after_write)
{
	struct timespec start_time;
	ssize_t total_written;

	memset(&start_time, 0, sizeof(start_time));

	pv_elapsedtime_read(&start_time);

	total_written = 0;

	while (count > 0) {
		ssize_t nwritten;
		struct timespec cur_time, transfer_elapsed;
		long double elapsed_seconds;
		size_t asked_to_write;

		asked_to_write = count > MAX_WRITE_AT_ONCE ? MAX_WRITE_AT_ONCE : count;

		nwritten = write(fd, buf, asked_to_write);

#ifdef HAVE_FDATASYNC
		if (sync_after_write && nwritten >= 0) {
			/*
			 * Ignore non IO errors, such as EBADFD (bad file
			 * descriptor), EINVAL (non syncable fd, such as a
			 * pipe), etc - only return an error on EIO.
			 */
#if defined(_POSIX_SYNCHRONIZED_IO) && _POSIX_SYNCHRONIZED_IO > 0
			if ((fdatasync(fd) < 0) && (EIO == errno)) {
				return -1;
			}
#else
			if ((fsync(fd) < 0) && (EIO == errno)) {
				return -1;
			}
#endif
		}
#endif				/* HAVE_FDATASYNC */

		if (nwritten < 0) {
			if ((EINTR == errno) || (EAGAIN == errno)) {
				/*
				 * Interrupted by a signal - probably our
				 * alarm or interval timer - so just return
				 * what we've written so far.
				 */
				return total_written;
			} else {
				/*
				 * Legitimate error - return negative.
				 */
				return nwritten;
			}
		}

		total_written += nwritten;
		buf += nwritten;
		count -= nwritten;

		if (0 == nwritten)
			return total_written;

		memset(&cur_time, 0, sizeof(cur_time));
		memset(&transfer_elapsed, 0, sizeof(transfer_elapsed));
		elapsed_seconds = 0.0;

		pv_elapsedtime_read(&cur_time);
		pv_elapsedtime_subtract(&transfer_elapsed, &cur_time, &start_time);
		elapsed_seconds = pv_elapsedtime_seconds(&transfer_elapsed);

		if (elapsed_seconds > TRANSFER_WRITE_TIMEOUT) {
			debug("%s %d: %s (%f %s)", "fd", fd,
			      "stopping write - timer expired", (double) elapsed_seconds, "sec elapsed");
			return total_written;
		}

		/*
		 * Running the select() here seems to make PV eat a lot of
		 * CPU in some cases, so instead we just go round the loop
		 * again and rely on our alarm or interval timer to
		 * interrupt us if we run out of time - also on our elapsed
		 * time check.
		 */
		if (count > 0) {
			debug("%s %d: %s (%ld %s, %ld %s)", "fd", fd,
			      "trying another write after partial buffer flush",
			      nwritten, "written", count, "remaining");

#if 0					    /* removed after 1.6.0 - see comment above */
			if (is_data_ready(-1, NULL, fd, NULL, 0) < 1) {
				break;
			}
#endif				/* end of removed section */
		}
	}

	return total_written;
}


/*
 * Read some data from the given file descriptor. Returns zero if there was
 * a transient error and we need to return 0 from pv_transfer, otherwise
 * returns 1.
 *
 * At most, the number of bytes read will be the number of bytes remaining
 * in the input buffer, capped to the number of bytes left until
 * state->control.size is reached if state->control.stop_at_size is true.
 * If state->control.rate_limit is >0, and/or "max_to_write" is >0, and
 * splice() is used, then the maximum number of bytes read will be further
 * capped to the value of "max_to_write", since splice() writes as well as
 * reads.
 *
 * If splice() was successfully used, sets state->transfer.splice_used to
 * true; if it failed, then state->transfer.splice_failed_fd is updated to
 * the current fd so splice() won't be tried again until the next input
 * file.
 *
 * Updates state->transfer.read_position by the number of bytes read, unless
 * splice() was used, in which case it does not since there's nothing in the
 * buffer (and it also adds the bytes to state->transfer.written since
 * they've been written to the output).
 *
 * Also increases state->transfer.total_bytes_read by the number of bytes
 * read, regardless of whether splice() was used, since total_bytes_read is
 * what it says it is, rather than being a buffer indicator.
 *
 * On read error, updates state->status.exit_status, and if max_to_write by
 * state->control.skip_errors, tries to skip past the problem.
 *
 * If the end of the input file is reached or the error is unrecoverable,
 * sets *eof_in to true.  If all data in the buffer has been written at this
 * point, then also sets *eof_out to true.
 */
static int pv__transfer_read(pvstate_t state, int fd, bool *eof_in, bool *eof_out, off_t max_to_write)
{
	bool do_not_skip_errors;
	size_t bytes_can_read;
	off_t amount_to_skip, amount_skipped, orig_offset, skip_offset;
	ssize_t nread;

	do_not_skip_errors = false;
	if (0 == state->control.skip_errors)
		do_not_skip_errors = true;

	bytes_can_read = state->transfer.buffer_size - state->transfer.read_position;

	/*
	 * Don't read past control.size if stop_at_size is true (issue
	 * #166).
	 *
	 * This isn't workable in line mode.
	 */
	if (state->control.stop_at_size && !state->control.linemode) {
		off_t bytes_remaining_to_read = state->control.size - state->transfer.total_bytes_read;
		if ((long long) bytes_can_read > (long long) bytes_remaining_to_read) {
			debug("%lld > (%lld-%lld=%lld): %s", (long long) bytes_can_read,
			      (long long) (state->control.size), (long long) state->transfer.total_bytes_read,
			      (long long) bytes_remaining_to_read, "truncating for stop-at-size");
			bytes_can_read = (size_t) bytes_remaining_to_read;
		}
	}

	nread = 0;

#ifdef HAVE_SPLICE
	state->transfer.splice_used = false;
	if ((!state->control.linemode) && (!state->control.no_splice)
	    && (fd != state->transfer.splice_failed_fd)
	    && (0 == state->transfer.to_write)) {
		size_t bytes_to_splice;

		if (state->control.rate_limit > 0 || max_to_write != 0) {
			bytes_to_splice = (size_t) max_to_write;
		} else {
			bytes_to_splice = bytes_can_read;
		}

		/*@-nullpass@ */
		/*@-type@ */
		/* splint doesn't know about splice */
		nread = splice(fd, NULL, state->control.output_fd, NULL, bytes_to_splice, SPLICE_F_MORE);
		/*@+type@ */
		/*@+nullpass@ */

		state->transfer.splice_used = true;
		if ((nread < 0) && (EINVAL == errno)) {
			debug("%s %d: %s", "fd", fd, "splice failed with EINVAL - disabling");
			state->transfer.splice_failed_fd = fd;
			state->transfer.splice_used = false;
			/*
			 * Fall through to read() below.
			 */
		} else if (nread > 0) {
			state->transfer.written = nread;
			state->transfer.total_bytes_read += nread;
#ifdef HAVE_FDATASYNC
			if (state->control.sync_after_write) {
				/*
				 * Ignore non IO errors, such as EBADFD (bad file
				 * descriptor), EINVAL (non syncable fd, such as a
				 * pipe), etc - only treat EIO as a failure.

				 * Since this is a write error, not a read
				 * error, we cannot skip it, so set
				 * "do_not_skip_errors".
				 */
				if ((fdatasync(state->control.output_fd) < 0)
				    && (EIO == errno)) {
					nread = -1;
					do_not_skip_errors = true;
				}
			}
#endif				/* HAVE_FDATASYNC */
		} else if ((-1 == nread) && (EAGAIN == errno)) {
			/* nothing read yet - do nothing */
		} else {
			/* EOF might not really be EOF, it seems */
			state->transfer.splice_used = false;
		}
	}
	if (!state->transfer.splice_used) {
		nread =
		    pv__transfer_read_repeated(fd, state->transfer.transfer_buffer + state->transfer.read_position,
					       bytes_can_read);
	}
#else
	nread =
	    pv__transfer_read_repeated(fd, state->transfer.transfer_buffer + state->transfer.read_position,
				       bytes_can_read);
#endif				/* HAVE_SPLICE */


	if (0 == nread) {
		/*
		 * If read returned 0, we've reached the end of this input
		 * file.  If we've also written all the data in the transfer
		 * buffer, we set eof_out as well, so that the main loop can
		 * move on to the next input file.
		 */
		*eof_in = true;
		if (state->transfer.write_position >= state->transfer.read_position)
			*eof_out = true;
		return 1;
	} else if (nread > 0) {
		/*
		 * Read returned >0, so we successfully read data - clear
		 * the error counter and update our record of how much data
		 * we've got in the buffer.
		 */
		state->transfer.read_errors_in_a_row = 0;
#ifdef HAVE_SPLICE
		/*
		 * If we used splice(), there isn't any more data in the
		 * buffer than there was before.
		 */
		if (!state->transfer.splice_used)
			state->transfer.read_position += nread;
#else
		state->transfer.read_position += nread;
#endif				/* HAVE_SPLICE */
		/* Update the counter of all bytes read so far. */
		state->transfer.total_bytes_read += nread;
		return 1;
	}

	/*
	 * If we reach this point, nread<0, so there was an error.
	 */

	/*
	 * If a read error occurred but it was EINTR or EAGAIN, just wait a
	 * bit and then return zero, since this was a transient error.
	 */
	if ((EINTR == errno) || (EAGAIN == errno)) {
		debug("%s %d: %s: %s", "fd", fd, "transient error - waiting briefly", strerror(errno));
		(void) is_data_ready(-1, NULL, -1, NULL, 10000);
		return 0;
	}

	/*
	 * The read error is not transient, so update the program's final
	 * exit status, regardless of whether we're skipping errors, and
	 * increment the error counter.
	 */
	state->status.exit_status |= PV_ERROREXIT_TRANSFER;
	state->transfer.read_errors_in_a_row++;

	/*
	 * If we aren't skipping errors, show the error and pretend we
	 * reached the end of this file.
	 */
	if (do_not_skip_errors) {
		pv_error("%s: %s: %s", pv_current_file_name(state), _("read failed"), strerror(errno));
		*eof_in = true;
		if (state->transfer.write_position >= state->transfer.read_position) {
			*eof_out = true;
		}
		return 1;
	}

	/*
	 * Try to skip past the error.
	 */

	amount_skipped = -1;

	if (!state->transfer.read_error_warning_shown) {
		/*@-compdef@ */
		pv_error("%s: %s: %s", pv_current_file_name(state), _("warning: read errors detected"),
			 strerror(errno));
		/*@+compdef@ */
		/* splint - see previous pv_current_file_name() call. */
		state->transfer.read_error_warning_shown = true;
	}

	orig_offset = (off_t) lseek(fd, 0, SEEK_CUR);

	/*
	 * If the file is not seekable, we can't skip past the error, so we
	 * will have to abandon the attempt and pretend we reached the end
	 * of the file.
	 */
	if (0 > orig_offset) {
		/*@-compdef@ */
		pv_error("%s: %s: %s", pv_current_file_name(state), _("file is not seekable"), strerror(errno));
		/*@+compdef@ */
		/* splint - see previous pv_current_file_name() calls. */
		*eof_in = true;
		if (state->transfer.write_position >= state->transfer.read_position) {
			*eof_out = true;
		}
		return 1;
	}

	/*
	 * If a non-zero error skip block size was given, just use that,
	 * otherwise start small and ramp up based on the number of errors
	 * in a row.
	 */
	if (state->control.error_skip_block > 0) {
		amount_to_skip = state->control.error_skip_block;
	} else {
		if (state->transfer.read_errors_in_a_row < 10) {
			amount_to_skip = (off_t) (state->transfer.read_errors_in_a_row < 5 ? 1 : 2);
		} else if (state->transfer.read_errors_in_a_row < 20) {
			unsigned int shift_by = (unsigned int) (state->transfer.read_errors_in_a_row - 10);
			amount_to_skip = (off_t) (1 << shift_by);
		} else {
			amount_to_skip = 512;
		}
	}

	/*
	 * Round the skip amount down to the start of the next block of the
	 * skip amount size.  For instance if the skip amount is 512, but
	 * our file offset is 257, we'll jump to 512 instead of 769.
	 */
	if (amount_to_skip > 1) {
		skip_offset = orig_offset + amount_to_skip;
		skip_offset -= (skip_offset % amount_to_skip);
		if (skip_offset > orig_offset) {
			amount_to_skip = skip_offset - orig_offset;
		}
	}

	/*
	 * Trim the skip amount so we wouldn't read too much.
	 */
	if (amount_to_skip > (off_t) bytes_can_read)
		amount_to_skip = (off_t) bytes_can_read;

	/*@+longintegral@ */
	/* splint complains about __off_t vs off_t */
	skip_offset = (off_t) lseek(fd, (off_t) (orig_offset + amount_to_skip), SEEK_SET);
	/*@-longintegral@ */

	/*
	 * If the skip we just tried didn't work, try only skipping 1 byte
	 * in case we were trying to go past the end of the input file.
	 */
	if (skip_offset < 0) {
		amount_to_skip = 1;
		/*@+longintegral@ */
		/* see above */
		skip_offset = (off_t) lseek(fd, (off_t) (orig_offset + amount_to_skip), SEEK_SET);
		/*@-longintegral@ */
	}

	if (skip_offset < 0) {
		/*
		 * Failed to skip - lseek() returned an error, so mark the
		 * file as having ended.
		 */
		*eof_in = true;
		/*
		 * EINVAL means the file has ended since we've tried to go
		 * past the end of it, so we don't bother with a warning
		 * since it just means we've reached the end anyway.
		 */
		if (EINVAL != errno) {
			/*@-compdef@ */
			pv_error("%s: %s: %s", pv_current_file_name(state), _("failed to seek past error"),
				 strerror(errno));
			/*@+compdef@ */
			/* splint - see previous pv_current_file_name() calls. */
		}
	} else {
		amount_skipped = skip_offset - orig_offset;
	}

	/*
	 * If we succeeded in skipping some bytes, zero the equivalent part
	 * of the transfer buffer, and update the buffer position.
	 */
	if (amount_skipped > 0) {
		memset(state->transfer.transfer_buffer + state->transfer.read_position, 0, (size_t) amount_skipped);
		state->transfer.read_position += amount_skipped;
		if (state->control.skip_errors < 2) {
			/*@-compdef@ */
			pv_error("%s: %s: %ld - %ld (%ld %s)",
				 pv_current_file_name(state),
				 _("skipped past read error"), (long) orig_offset, (long) skip_offset,
				 (long) amount_skipped, _("B"));
			/*@+compdef@ */
			/* splint - see previous pv_current_file_name() calls. */
		}
	} else {
		/*
		 * Failed to skip - mark file as ended.
		 */
		*eof_in = true;
		if (state->transfer.write_position >= state->transfer.read_position) {
			*eof_out = true;
		}
	}

	return 1;
}


/*
 * Write state->transfer.to_write bytes of data from the transfer buffer to the output.
 * Returns zero if there was a transient error and we need to return 0 from
 * pv_transfer, otherwise returns 1.
 *
 * Updates state->transfer.write_position by moving it on by the number of bytes
 * written; adds the number of bytes written to state->transfer.written; sets
 * *eof_out to true, on output EOF, or when the write position catches up
 * with the read position AND *eof_in is true (meaning we've reached the end
 * of data).
 *
 * On error, sets *eof_out to true, sets state->transfer.written to -1, and updates
 * state->status.exit_status.
 *
 * If state->control.discard_input is true, does not actually write anything.
 */
static int pv__transfer_write(pvstate_t state, bool *eof_in, bool *eof_out, long *lineswritten)
{
	ssize_t nwritten;
	int write_errno;
	bool all_nulls;
	off_t output_offset;
	size_t write_check_position, write_end_position;

	if (NULL == state->transfer.transfer_buffer) {
		pv_error("%s", _("no transfer buffer allocated"));
		state->status.exit_status |= PV_ERROREXIT_MEMORY;
		*eof_out = true;
		state->transfer.written = -1;
		return 1;
	}

	nwritten = 0;
	write_errno = 0;

	if (state->control.discard_input) {
		nwritten = state->transfer.to_write;
	} else if (state->transfer.to_write > 0) {

		/*
		 * In sparse output mode, check whether all of the bytes to
		 * be written are null, and if so, try to seek the output
		 * instead of writing the null bytes.
		 */
		if (state->control.sparse_output && !state->transfer.output_not_seekable) {
			write_check_position = state->transfer.write_position;
			write_end_position = write_check_position + (size_t) (state->transfer.to_write);
			all_nulls = true;
			while (all_nulls && write_check_position < write_end_position) {
				if ('\0' == state->transfer.transfer_buffer[write_check_position]) {
					write_check_position++;
				} else {
					all_nulls = false;
				}
			}
			if (all_nulls) {

				/*
				 * Use lseek() to move forward in the file,
				 * and then ftruncate() it to its new size.
				 *
				 * If any step fails, mark the output not
				 * seekable so we stop trying.
				 */

				/*@+longintegral@ */
				/*
				 * splint has trouble with off_t / __off_t, in the lseek() call.
				 */
				output_offset =
				    lseek(state->control.output_fd, (off_t) (state->transfer.to_write), SEEK_CUR);
				if (output_offset == (off_t) - 1) {
					debug("%s: %s", "output lseek() failed", strerror(errno));
					state->transfer.output_not_seekable = true;
				} else {
					/* Seek successful - skip write. */
					debug("%s (%ld) -> %s: %ld", "skipped null writes",
					      (long) (state->transfer.to_write), "new position", (long) output_offset);
					nwritten = state->transfer.to_write;
					goto pv__transfer_write_completed;
				}
				/*@-longintegral@ */
			}
		}

		/*
		 * Set an interval timer or an alarm to interrupt the write
		 * with a signal if the write takes too long, so we can
		 * continue producing progress information.
		 */
#if HAVE_SETITIMER
		struct itimerval new_timer;

		/*@-unrecog@ */
		/* splint doesn't know setitimer or ITIMER_REAL */
		memset(&new_timer, 0, sizeof(new_timer));
		new_timer.it_value.tv_sec = (time_t) (state->control.interval);
		new_timer.it_value.tv_usec = (suseconds_t) (((long) (state->control.interval * 1000000.0)) % 1000000);

		/*
		 * We have to set the interval so that the timer continues
		 * to repeat while writes are attempted, especially as it's
		 * possible that the initial timer run will expire
		 * immediately if the period is less than 1 second.
		 */

		new_timer.it_interval.tv_sec = new_timer.it_value.tv_sec;
		new_timer.it_interval.tv_usec = new_timer.it_value.tv_usec;

		debug("%s: [%lds,%ldus]", "setting interval timer", (long) (new_timer.it_value.tv_sec),
		      (long) (new_timer.it_value.tv_usec));

		if (0 != setitimer(ITIMER_REAL, &new_timer, NULL)) {
			pv_error("%s: %s", _("failed to set interval timer"), strerror(errno));
		}

#else				/* ! HAVE_SETITIMER */
		(void) alarm(1);
		debug("%s", "setting alarm");
#endif				/* HAVE_SETITIMER */
		debug("%s: %ld %s", "beginning write attempt", (long) (state->transfer.to_write), "bytes");
		nwritten = pv__transfer_write_repeated(state->control.output_fd,
						       state->transfer.transfer_buffer +
						       state->transfer.write_position,
						       (size_t) (state->transfer.to_write),
						       state->control.sync_after_write);
		if (nwritten < 0) {
			write_errno = (int) errno;
			debug("%s: %ld: %s", "bytes written", (long) nwritten, strerror(errno));
		} else {
			debug("%s: %ld", "bytes written", (long) nwritten);
		}
#if HAVE_SETITIMER
		memset(&new_timer, 0, sizeof(new_timer));
		new_timer.it_interval.tv_sec = 0;
		new_timer.it_interval.tv_usec = 0;
		new_timer.it_value.tv_sec = 0;
		new_timer.it_value.tv_usec = 0;
		if (0 != setitimer(ITIMER_REAL, &new_timer, NULL)) {
			pv_error("%s: %s", _("failed to clear interval timer"), strerror(errno));
		}

		/*@+unrecog@ */
#else				/* ! HAVE_SETITIMER */
		debug("%s", "cancelling alarm");
		(void) alarm(0);
#endif				/* HAVE_SETITIMER */
	}

      pv__transfer_write_completed:
	/* If lseek() worked for sparse output, it jumps down here. */

	if (nwritten > 0) {
		bool tracking_lines = false;

		if ((state->control.linemode) && (lineswritten != NULL))
			tracking_lines = true;
		else if (state->display.showing_previous_line)
			tracking_lines = true;

		/*
		 * Write returned >0 - data successfully written.
		 */
		if (tracking_lines) {
			char separator;
			char *ptr;
			long lines = 0;

			/*
			 * Tracking lines - either line mode, or we're
			 * showing the last line in the display, or both.
			 * So we need to look through what we've just
			 * written to either count how many lines there
			 * were, or get the content of the most recent
			 * complete line, or both.
			 */

			/* Allocate buffer to remember line positions. */
			if (NULL == state->transfer.line_positions && NULL != lineswritten) {
				state->transfer.line_positions_capacity = MAX_LINE_POSITIONS;
				/*@-mustfreeonly@ */
				state->transfer.line_positions =
				    calloc((size_t) (state->transfer.line_positions_capacity), sizeof(off_t));
				if (NULL == state->transfer.line_positions) {
					pv_error("%s: %s", _("line position buffer allocation failed"),
						 strerror(errno));
				}
				/*@+mustfreeonly@ */
				/* splint doesn't see we only call calloc() when line_positions is NULL. */
			}

			if (state->control.null_terminated_lines) {
				separator = '\0';
			} else {
				separator = '\n';
			}

			ptr = (char *) (state->transfer.transfer_buffer + state->transfer.write_position - 1);
			for (ptr++;
			     ptr - (char *) state->transfer.transfer_buffer - state->transfer.write_position <
			     (size_t) nwritten; ptr++, state->transfer.last_output_position++) {
				if (*ptr != separator) {
					/*
					 * If we're displaying the previous
					 * line ("%L"), add to our line
					 * buffer.
					 */
					if (state->display.showing_previous_line
					    && state->display.next_line_len < PV_SIZEOF_PREVLINE_BUFFER - 1) {
						state->display.next_line[state->display.next_line_len] = *ptr;
						state->display.next_line_len++;
					}
					continue;
				}

				/* Separator found - increment line count. */
				++lines;

				/*
				 * If we're displaying the previous line
				 * ("%L"), update the previous-line buffer
				 * with the line we just completed, and
				 * start a new one.
				 */
				if (state->display.showing_previous_line) {
					memset(state->display.previous_line, 0, PV_SIZEOF_PREVLINE_BUFFER);
					if (state->display.next_line_len > PV_SIZEOF_PREVLINE_BUFFER - 1)
						state->display.next_line_len = PV_SIZEOF_PREVLINE_BUFFER - 1;
					if (state->display.next_line_len > 0) {
						memcpy(state->display.previous_line, state->display.next_line,	/* flawfinder: ignore */
						       state->display.next_line_len);
						debug("%s: [%s]", "updated previous_line",
						      state->display.previous_line);
					}
					state->display.next_line_len = 0;
					/*
					 * flawfinder - next_line_len is
					 * guaranteed to be less than the
					 * size of the previous_line buffer
					 * since we check it just before
					 * memcpy(), and we ensure that the
					 * last byte in the buffer is \0.
					 */
				}

				if (NULL == state->transfer.line_positions)
					continue;

				/* Store the position of the separator. */
				state->transfer.line_positions[state->transfer.line_positions_head] =
				    state->transfer.last_output_position;
				state->transfer.line_positions_head++;

				/* Circular buffer - wrap around. */
				if (state->transfer.line_positions_head >= state->transfer.line_positions_capacity) {
					state->transfer.line_positions_head = 0;
				}

				/* Increment count of line positions, if below capacity. */
				if (state->transfer.line_positions_length < state->transfer.line_positions_capacity) {
					state->transfer.line_positions_length++;
				}
			}

			if (NULL != lineswritten)
				*lineswritten += lines;
		}

		state->transfer.write_position += nwritten;
		state->transfer.written += nwritten;

		/*
		 * If we're monitoring the output, update our copy of the
		 * last few bytes we've written.
		 */
		if (state->display.showing_last_written && (nwritten > 0)) {
			size_t new_portion_size, old_portion_size;

			new_portion_size = (size_t) nwritten;
			if (new_portion_size > state->display.lastwritten_bytes)
				new_portion_size = state->display.lastwritten_bytes;

			old_portion_size = state->display.lastwritten_bytes - new_portion_size;

			/*
			 * Make room for the new portion.
			 */
			if (old_portion_size > 0) {
				memmove(state->display.lastwritten_buffer,
					state->display.lastwritten_buffer + new_portion_size, old_portion_size);
			}

			/*
			 * Copy the new data in.
			 */
			memcpy(state->display.lastwritten_buffer +	/* flawfinder: ignore */
			       old_portion_size,
			       state->transfer.transfer_buffer + state->transfer.write_position - new_portion_size,
			       new_portion_size);
			/*
			 * flawfinder rationale: calculations above ensure
			 * that old_portion_size + new_portion_size is
			 * always <= lastwritten_bytes, and
			 * lastwritten_bytes is guaranteed by
			 * pv__format_init() to be no more than
			 * PV_SIZEOF_LASTWRITTEN_BUFFER, which is the size
			 * of lastwritten_buffer, so the memcpy() will
			 * always fit into the buffer.
			 */
		}

		/*
		 * If we've written all the data in the buffer, reset the
		 * read pointer to the start, and if the input file is at
		 * EOF, set eof_out as well to indicate that we've written
		 * everything for this input file.
		 */
		if (state->transfer.write_position >= state->transfer.read_position) {
			state->transfer.write_position = 0;
			state->transfer.read_position = 0;
			if (*eof_in)
				*eof_out = true;
		}

		return 1;
	}

	/*
	 * If we reach this point, nwritten<=0, so there may be an error.
	 */

	/*
	 * If a write error occurred but it was EINTR or EAGAIN, or write(2)
	 * blocked on first write such that nwritten == 0, just wait a bit and
	 * then return zero, since this was a transient error.
	 */
	if ((0 == nwritten) || (EINTR == write_errno) || (EAGAIN == write_errno)) {
		if (0 == nwritten) {
			debug("%s", "attempted write blocked - waiting briefly");
		} else {
			debug("%s: %s", "transient write error - waiting briefly", strerror(write_errno));
		}
		(void) is_data_ready(-1, NULL, -1, NULL, 10000);
		return 0;
	}

	/*
	 * SIGPIPE means we've finished. Don't output an error because it's
	 * not really our error to report.
	 */
	if (EPIPE == write_errno) {
		*eof_in = true;
		*eof_out = true;
		state->flags.pipe_closed = 1;
		debug("%s", "SIGPIPE received - setting pipe_closed");
		return 0;
	}

	pv_error("%s: %s", _("write failed"), strerror(write_errno));
	state->status.exit_status |= PV_ERROREXIT_TRANSFER;
	*eof_out = true;
	state->transfer.written = -1;

	return 1;
}


/*
 * Return a pointer to a newly allocated buffer of the given size, aligned
 * appropriately for the current input and output file descriptors
 * (important if using O_DIRECT).
 *
 * Falls back to unaligned allocation if it was not possible to get an
 * aligned buffer, or if the relevant operating system features were not
 * available.  With O_DIRECT, this means that transfers could fail with an
 * "Invalid argument" error (EINVAL).
 *
 * Returns NULL on complete allocation failure.
 */
/*@null@*/
/*@only@*/
static char *pv__allocate_aligned_buffer(int outfd, int infd, size_t target_size)
{
	void *newptr;

#if defined(HAVE_FPATHCONF) && defined(HAVE_POSIX_MEMALIGN) && defined(_PC_REC_XFER_ALIGN)
	long input_alignment, output_alignment, min_alignment;
	long required_alignment;

	input_alignment = infd >= 0 ? fpathconf(infd, _PC_REC_XFER_ALIGN) : -1;
	output_alignment = fpathconf(outfd, _PC_REC_XFER_ALIGN);
#if defined(HAVE_SYSCONF) && defined(_SC_PAGESIZE)
	min_alignment = sysconf(_SC_PAGESIZE);
#else				/* ! defined(HAVE_SYSCONF) && defined(_SC_PAGESIZE) */
	min_alignment = 8192;
#endif				/* defined(HAVE_SYSCONF) && defined(_SC_PAGESIZE) */

	if (input_alignment > output_alignment) {
		required_alignment = input_alignment;
	} else if (output_alignment > input_alignment) {
		required_alignment = output_alignment;
	} else if (input_alignment < min_alignment) {
		required_alignment = min_alignment;
	} else {
		required_alignment = input_alignment;
	}

	/* Ensure the alignment is at least the page size. */
	if (required_alignment < min_alignment) {
		required_alignment = min_alignment;
	}

	newptr = NULL;

	/*@-unrecog@ */
	/* splice doesn't know of posix_memalign(). */
	if (0 != posix_memalign((void **) (&newptr), (size_t) required_alignment, target_size)) {
		newptr = malloc(target_size);
	}
	/*@+unrecog@ */
#else				/* ! defined(HAVE_FPATHCONF) && defined(HAVE_POSIX_MEMALIGN) && defined(_PC_REC_XFER_ALIGN) */
	newptr = malloc(target_size);
#endif				/* defined(HAVE_FPATHCONF) && defined(HAVE_POSIX_MEMALIGN) && defined(_PC_REC_XFER_ALIGN) */

	/* Initialise the buffer with zeroes. */
	if (NULL != newptr)
		memset(newptr, 0, target_size);

	return newptr;
}


/*
 * Transfer some data from "fd" to standard output, timing out after 9/100
 * of a second.  If state->control.rate_limit is >0, and/or "allowed" is >0, only up
 * to "allowed" bytes can be written.  The variables that "eof_in" and
 * "eof_out" point to are used to flag that we've finished reading and
 * writing respectively.
 *
 * Returns the number of bytes written, or negative on error (in which case
 * state->status.exit_status is updated). In line mode, the number of lines written
 * will be put into *lineswritten.
 */
ssize_t pv_transfer(pvstate_t state, int fd, bool *eof_in, bool *eof_out, off_t allowed, long *lineswritten)
{
	bool ready_to_read, ready_to_write;
	int check_read_fd, check_write_fd;
	int n;

	if (NULL == state)
		return 0;

#ifdef O_DIRECT
	/*
	 * Set or clear O_DIRECT on the input and output file descriptors,
	 * if the setting has changed.
	 */
	if (state->control.direct_io_changed) {
		if (!(*eof_in)) {
			if (0 != fcntl(fd, F_SETFL, (state->control.direct_io ? O_DIRECT : 0) | fcntl(fd, F_GETFL))) {
				/*@-compdef@ */
				debug("%s: %s: %s", pv_current_file_name(state), "fcntl", strerror(errno));
				/*@+compdef@ */
				/* splint - see previous pv_current_file_name() calls. */
			}
		}
		if (!(*eof_out)) {
			if (0 != fcntl(state->control.output_fd, F_SETFL,
				       (state->control.direct_io ? O_DIRECT : 0) |
				       fcntl(state->control.output_fd, F_GETFL))) {
				debug("%s: %s: %s",
				      NULL == state->control.output_name ? "(null)" : state->control.output_name,
				      "fcntl", strerror(errno));
			}
		}
		state->control.direct_io_changed = false;
	}
#endif				/* O_DIRECT */

	/*
	 * Reinitialise the error skipping variables if the file descriptor
	 * has changed since the last time we were called.
	 */
	if (fd != state->transfer.last_read_skip_fd) {
		state->transfer.last_read_skip_fd = fd;
		state->transfer.read_errors_in_a_row = 0;
		state->transfer.read_error_warning_shown = false;
	}

	/*
	 * Allocate a new buffer, aligned appropriately for the input file
	 * (important if using O_DIRECT).
	 */
	if (NULL == state->transfer.transfer_buffer) {
		state->transfer.transfer_buffer =
		    pv__allocate_aligned_buffer(state->control.output_fd, fd, state->control.target_buffer_size + 32);
		if (NULL == state->transfer.transfer_buffer) {
			pv_error("%s: %s", _("buffer allocation failed"), strerror(errno));
			state->status.exit_status |= PV_ERROREXIT_MEMORY;
			return -1;
		}
		state->transfer.buffer_size = state->control.target_buffer_size;
	}

	/*
	 * Reallocate the buffer if the buffer size has changed
	 * mid-transfer.  We have to do this by allocating a new buffer,
	 * copying to it, and freeing the old one (potentially leaking
	 * memory) because the buffer may need to be aligned for O_DIRECT,
	 * and we can't realloc() an aligned buffer.
	 */
	if (state->transfer.buffer_size < state->control.target_buffer_size) {
		char *newptr;
		newptr =
		    pv__allocate_aligned_buffer(state->control.output_fd, fd, state->control.target_buffer_size + 32);
		if (NULL == newptr) {
			/*
			 * Reset target if realloc failed so we don't keep
			 * trying to realloc over and over.
			 */
			debug("realloc: %s", strerror(errno));
			state->control.target_buffer_size = state->transfer.buffer_size;
		} else {
			debug("%s: %ld", "buffer resized", state->transfer.buffer_size);
			/*
			 * Copy the old buffer contents into the new buffer,
			 * and free the old one.
			 */
			if (state->transfer.buffer_size > 0) {
				memcpy(newptr, state->transfer.transfer_buffer, state->transfer.buffer_size);	/* flawfinder: ignore */
			}
			/*
			 * flawfinder rationale: number of bytes copied is
			 * definitely always smaller than the new buffer
			 * size.
			 */
			free(state->transfer.transfer_buffer);
			state->transfer.transfer_buffer = newptr;
			state->transfer.buffer_size = state->control.target_buffer_size;
		}
	}

	if ((state->control.linemode) && (lineswritten != NULL))
		*lineswritten = 0;

	if ((*eof_in) && (*eof_out)) {
		debug("%s %d: %s", "fd", fd, "early return 0 - EOF in and out");
		return 0;
	}

	check_read_fd = -1;
	check_write_fd = -1;

	/*
	 * If the input file is not at EOF and there's room in the buffer,
	 * look for incoming data from it.
	 */
	if ((!(*eof_in)) && (state->transfer.read_position < state->transfer.buffer_size)) {
		check_read_fd = fd;
	}

	/*
	 * Work out how much we're allowed to write, based on the amount of
	 * data left in the buffer.  If rate limiting is active or "allowed"
	 * is >0, then this puts an upper limit on how much we're allowed to
	 * write.
	 */
	state->transfer.to_write = (ssize_t) (state->transfer.read_position - state->transfer.write_position);
	if ((state->control.rate_limit > 0) || (allowed > 0)) {
		if ((off_t) (state->transfer.to_write) > allowed) {
			state->transfer.to_write = (ssize_t) allowed;
		}
	}

	/*
	 * If we don't think we've finished writing and there's anything
	 * we're allowed to write, look for the output becoming writable.
	 */
	if ((!(*eof_out)) && (state->transfer.to_write > 0)) {
		check_write_fd = state->control.output_fd;
	}

	ready_to_read = false;
	ready_to_write = false;
	n = is_data_ready(check_read_fd, &ready_to_read, check_write_fd, &ready_to_write, 90000);

	if (n < 0) {
		/*
		 * Ignore transient errors by returning 0 immediately.
		 */
		if (EINTR == errno) {
			debug("%s %d: %s", "fd", fd, "early return 0 - is_data_ready < 0");
			return 0;
		}

		/*
		 * Any other error is a problem and we must report back.
		 */
		/*@-compdef@ */
		pv_error("%s: %s: %d: %s", pv_current_file_name(state), _("select call failed"), n, strerror(errno));
		/*@+compdef@ */
		/* splint - see previous pv_current_file_name() calls. */

		state->status.exit_status |= PV_ERROREXIT_TRANSFER;

		return -1;
	}

	state->transfer.written = 0;

	/*
	 * If there is data to read, try to read some in. Return early if
	 * there was a transient read error.
	 *
	 * NB this can update state->transfer.written because of splice().
	 */
	if (ready_to_read) {
		if (pv__transfer_read(state, fd, eof_in, eof_out, allowed) == 0) {
			debug("%s %d: %s (%s=%s, %s=%s, %s=%lu)", "fd", fd,
			      "early return 0 - pv__transfer_read returned 0", "eof_in", eof_in ? "true" : "false",
			      "eof_out", eof_out ? "true" : "false", "allowed", (unsigned long) allowed);
			return 0;
		}
	}

	/*
	 * In line mode, only write up to and including the last newline,
	 * so that we're writing output line-by-line.
	 */
	if ((state->transfer.to_write > 0) && (state->control.linemode) && !(state->control.null_terminated_lines)) {
		char *start;
		char *end;

		start = (char *) (state->transfer.transfer_buffer + state->transfer.write_position);
		end = pv_memrchr(start, (int) '\n', (size_t) (state->transfer.to_write));

		if (NULL != end) {
			state->transfer.to_write = (ssize_t) ((end - start) + 1);
		}
	}

	/*
	 * If there is data to write, and the output is ready to receive it,
	 * and we didn't use splice() this time, write some data. 
	 * Return early if there was a transient write error.
	 */
	if (ready_to_write
#ifdef HAVE_SPLICE
	    && (!state->transfer.splice_used)
#endif				/* HAVE_SPLICE */
	    && (state->transfer.read_position > state->transfer.write_position)
	    && (state->transfer.to_write > 0)
	    && (NULL != lineswritten)) {
		if (pv__transfer_write(state, eof_in, eof_out, lineswritten) == 0) {
			debug("%s %d: %s (%s=%s, %s=%s, %s=%lu)", "fd", fd,
			      "early return 0 - pv__transfer_write returned 0", "eof_in", eof_in ? "true" : "false",
			      "eof_out", eof_out ? "true" : "false", "lineswritten", (unsigned long) lineswritten);
			return 0;
		}
	}
#ifdef MAXIMISE_BUFFER_FILL
	/*
	 * Rotate the written bytes out of the buffer so that it can be
	 * filled up completely by the next read.
	 */
	if (state->transfer.write_position > 0) {
		if (state->transfer.write_position < state->transfer.read_position) {
			memmove(state->transfer.transfer_buffer,
				state->transfer.transfer_buffer +
				state->transfer.write_position,
				state->transfer.read_position - state->transfer.write_position);
			state->transfer.read_position -= state->transfer.write_position;
			state->transfer.write_position = 0;
		} else {
			state->transfer.write_position = 0;
			state->transfer.read_position = 0;
		}
	}
#endif				/* MAXIMISE_BUFFER_FILL */

	if (0 == state->transfer.written) {
		debug("%s %d: %s", "fd", fd, "end-of-function return 0 - transfer.written is zero");
	}

	return state->transfer.written;
}
