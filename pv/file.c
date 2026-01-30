/*
 * Functions for opening and closing files, and calculating their size.
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
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>


/*@-type@*/
/* splint has trouble with off_t and mode_t throughout this file. */

/*
 * Calculate the total number of bytes to be transferred by adding up the
 * sizes of all input files.  If any of the input files are of indeterminate
 * size (such as if they are a pipe), the total size is set to zero.
 *
 * Any files that cannot be stat()ed or that access() says we can't read
 * will be skipped, and the total size will be set to zero.
 *
 * Returns the total size, or 0 if it is unknown.
 */
static off_t pv_calc_total_bytes(pvstate_t state)
{
	off_t total;
	struct stat sb;
	unsigned int file_idx;

	total = 0;
	memset(&sb, 0, sizeof(sb));

	/*
	 * No files specified - check stdin.
	 */
	if ((state->files.file_count < 1) || (NULL == state->files.filename)) {
		if (0 == fstat(STDIN_FILENO, &sb))
			total = sb.st_size;
		return total;
	}

	for (file_idx = 0; file_idx < state->files.file_count; file_idx++) {
		int rc;

		/* Skip any NULL entries, though they should be impossible. */
		if (NULL == state->files.filename[file_idx])
			continue;

		if (0 == strcmp(state->files.filename[file_idx], "-")) {
			rc = fstat(STDIN_FILENO, &sb);
			if (rc != 0) {
				total = 0;
				return total;
			}
		} else {
			rc = stat(state->files.filename[file_idx], &sb);
			if (0 == rc) {
				rc = access(state->files.filename[file_idx], R_OK);	/* flawfinder: ignore */
				/*
				 * flawfinder rationale: we're not really
				 * using access() to do permissions checks,
				 * but to zero the total if we might be
				 * unable to read the file later, so if an
				 * attacker redirected one of the input
				 * files in between this part and the actual
				 * reading, the outcome would be that the
				 * total byte count would be wrong or
				 * missing, nothing useful.
				 */
			}
		}

		if (rc != 0) {
			debug("%s: %s", state->files.filename[file_idx], strerror(errno));
			total = 0;
			return total;
		}

		if (S_ISBLK(sb.st_mode)) {
			int fd;

			/*
			 * Get the size of block devices by opening
			 * them and seeking to the end.
			 */
			if (0 == strcmp(state->files.filename[file_idx], "-")) {
				fd = open("/dev/stdin", O_RDONLY);	/* flawfinder: ignore */
				/*
				 * flawfinder rationale: "/dev/stdin" may be
				 * a symlink, so can't use O_NOFOLLOW, and
				 * so we have to assume that it being under
				 * "/dev" means the path is less likely to
				 * be under the control of someone else.
				 */
			} else {
				fd = open(state->files.filename[file_idx], O_RDONLY);	/* flawfinder: ignore */
				/* flawfinder - see last open() below. */
			}
			if (fd >= 0) {
				off_t end_position;
				end_position = lseek(fd, 0, SEEK_END);
				if (end_position > 0) {
					total += end_position;
				}
				(void) close(fd);
			} else {
				total = 0;
				return total;
			}
		} else if (S_ISREG(sb.st_mode)) {
			total += sb.st_size;
		} else {
			total = 0;
		}
	}

	/*
	 * Patch from Peter Samuelson: if we cannot work out the size of the
	 * input, but we are writing to a block device, then use the size of
	 * the output block device.
	 *
	 * Further modified to check that output is not in append-only mode
	 * and that we can seek back to the start after getting the size.
	 */
	if (total < 1) {
		int rc;

		rc = fstat(state->control.output_fd, &sb);

		if ((0 == rc) && S_ISBLK(sb.st_mode)
		    && (0 == (fcntl(state->control.output_fd, F_GETFL) & O_APPEND))) {
			off_t end_position;
			end_position = lseek(state->control.output_fd, 0, SEEK_END);
			total = 0;
			if (end_position > 0) {
				total = end_position;
			}
			if (lseek(state->control.output_fd, 0, SEEK_SET) != 0) {
				pv_error("%s: %s: %s",
					 NULL == state->control.output_name ? "(null)" : state->control.output_name,
					 _("failed to seek to start of output"), strerror(errno));
				state->status.exit_status |= PV_ERROREXIT_ACCESS;
			}
			/*
			 * If we worked out a size, then set the
			 * stop-at-size flag to prevent a "no space left on
			 * device" error when we reach the end of the output
			 * device.
			 */
			if (total > 0) {
				state->control.stop_at_size = true;
			}
		}
	}

	return total;
}


/*
 * Count the total number of lines to be transferred by reading through all
 * input files.  If any of the inputs are not regular files (such as if they
 * are a pipe or a block device), the total size is set to zero.
 *
 * Any files that cannot be stat()ed or that access() says we can't read
 * will be skipped, and the total size will be set to zero.
 *
 * Returns the total size, or 0 if it is unknown.
 */
static off_t pv_calc_total_lines(pvstate_t state)
{
	off_t total;
	struct stat sb;
	unsigned int file_idx;

	total = 0;

	for (file_idx = 0; file_idx < state->files.file_count && NULL != state->files.filename; file_idx++) {
		int fd = -1;
		int rc = 0;

		/* Skip any NULL entries, though they should be impossible. */
		if (NULL == state->files.filename[file_idx])
			continue;

		if (0 == strcmp(state->files.filename[file_idx], "-")) {
			rc = fstat(STDIN_FILENO, &sb);
			if ((rc != 0) || (!S_ISREG(sb.st_mode))) {
				total = 0;
				return total;
			}
			fd = dup(STDIN_FILENO);
		} else {
			rc = stat(state->files.filename[file_idx], &sb);
			if ((rc != 0) || (!S_ISREG(sb.st_mode))) {
				total = 0;
				return total;
			}
			fd = open(state->files.filename[file_idx], O_RDONLY);	/* flawfinder: ignore */
			/* flawfinder - see last open() below. */
		}

		if (fd < 0) {
			debug("%s: %s", state->files.filename[file_idx], strerror(errno));
			total = 0;
			return total;
		}
#if HAVE_POSIX_FADVISE
		/* Advise the OS that we will only be reading sequentially. */
		(void) posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif

		while (true) {
			char scanbuf[1024];	/* flawfinder: ignore */
			ssize_t numread, buf_idx;

			/* flawfinder - always bounded, below. */

			numread = read(fd, scanbuf, sizeof(scanbuf));	/* flawfinder: ignore */
			/*
			 * flawfinder rationale: each time around the loop
			 * we are always reading into the start of the
			 * buffer, not moving along it, so the bounding is
			 * OK.
			 */
			if (numread < 0) {
				pv_error("%s: %s", state->files.filename[file_idx], strerror(errno));
				state->status.exit_status |= PV_ERROREXIT_ACCESS;
				break;
			} else if (0 == numread) {
				break;
			}
			for (buf_idx = 0; buf_idx < numread; buf_idx++) {
				if (state->control.null_terminated_lines) {
					if ('\0' == scanbuf[buf_idx])
						total++;
				} else {
					if ('\n' == scanbuf[buf_idx])
						total++;
				}
			}
		}

		if (0 != lseek(fd, 0, SEEK_SET)) {
			pv_error("%s: %s", state->files.filename[file_idx], strerror(errno));
			state->status.exit_status |= PV_ERROREXIT_ACCESS;
		}

		(void) close(fd);
	}

	return total;
}


/*
 * Work out the total size of all data by adding up the sizes of all input
 * files, using either pv_calc_total_bytes() or pv_calc_total_lines()
 * depending on whether state->control.linemode is true.
 *
 * Returns the total size, or 0 if it is unknown.
 */
off_t pv_calc_total_size(pvstate_t state)
{
	if (state->control.linemode) {
		return pv_calc_total_lines(state);
	} else {
		return pv_calc_total_bytes(state);
	}
}


/*
 * Close the given file descriptor and open the next one, whose number in
 * the list is "filenum", returning the new file descriptor (or negative on
 * error). It is an error if the next input file is the same as the file
 * the output is pointing to.
 *
 * Updates state->status.current_input_file in the process.
 */
int pv_next_file(pvstate_t state, unsigned int filenum, int oldfd)
{
	struct stat isb;
	struct stat osb;
	int fd;
	bool input_file_is_output;
	char *next_filename;

	if (oldfd >= 0) {
		if (0 != close(oldfd)) {
			pv_error("%s: %s", _("failed to close file"), strerror(errno));
			state->status.exit_status |= PV_ERROREXIT_TRANSITION;
			return -1;
		}
	}

	if (filenum >= state->files.file_count) {
		debug("%s: %d >= %d", "filenum too large", filenum, state->files.file_count);
		state->status.exit_status |= PV_ERROREXIT_TRANSITION;
		return -1;
	}

	next_filename = NULL;
	if (NULL != state->files.filename) {
		next_filename = state->files.filename[filenum];
		if (NULL == next_filename) {
			debug("%s: @%d, max %d", "unexpected null filename", filenum, state->files.file_count);
			state->status.exit_status |= PV_ERROREXIT_TRANSITION;
			return -1;
		}
	}

	if ((NULL == next_filename) || (0 == strcmp(next_filename, "-"))) {
		fd = STDIN_FILENO;
	} else {
		fd = open(next_filename, O_RDONLY);	/* flawfinder: ignore */
		/*
		 * flawfinder rationale: the input file list is under the
		 * control of the operator by its nature, so we can't refuse
		 * to open symlinks etc as that would be counterintuitive.
		 */
		if (fd < 0) {
			pv_error("%s: %s: %s", _("failed to read file"), next_filename, strerror(errno));
			state->status.exit_status |= PV_ERROREXIT_ACCESS;
			return -1;
		}
	}

	if (0 != fstat(fd, &isb)) {
		pv_error("%s: %s: %s", _("failed to stat file"),
			 NULL == next_filename ? "-" : next_filename, strerror(errno));
		(void) close(fd);
		state->status.exit_status |= PV_ERROREXIT_ACCESS;
		return -1;
	}

	if (0 != fstat(state->control.output_fd, &osb)) {
		pv_error("%s: %s", _("failed to stat output file"), strerror(errno));
		(void) close(fd);
		state->status.exit_status |= PV_ERROREXIT_ACCESS;
		return -1;
	}

	/*
	 * Check that this new input file is not the same as output's
	 * destination. This restriction is ignored for anything other
	 * than a regular file or block device.
	 */
	input_file_is_output = true;
	if (isb.st_dev != osb.st_dev)
		input_file_is_output = false;
	if (isb.st_ino != osb.st_ino)
		input_file_is_output = false;
	if (0 != isatty(fd))
		input_file_is_output = false;
	if ((!S_ISREG(isb.st_mode)) && (!S_ISBLK(isb.st_mode)))
		input_file_is_output = false;

	if (input_file_is_output) {
		pv_error("%s: %s", _("input file is output file"), NULL == next_filename ? "-" : next_filename);
		(void) close(fd);
		state->status.exit_status |= PV_ERROREXIT_OUROBOROS;
		return -1;
	}

	state->status.current_input_file = filenum;
#ifdef O_DIRECT
	/*
	 * Set or clear O_DIRECT on the file descriptor.
	 */
	if (0 != fcntl(fd, F_SETFL, (state->control.direct_io ? O_DIRECT : 0) | fcntl(fd, F_GETFL))) {
		/*@-compdef@ */
		/*
		 * splint - passed or returned storage is undefined - but at
		 * this point we know the input file list is been populated,
		 * so that's OK.
		 */
		debug("%s: %s: %s", pv_current_file_name(state), "fcntl", strerror(errno));
		/*@+compdef@ */
	}
	/*
	 * We don't clear direct_io_changed here, to avoid race conditions
	 * that could cause the input and output settings to differ.
	 */
#endif				/* O_DIRECT */

	debug("%s: %d: %s: fd=%d", "next file opened", filenum, pv_current_file_name(state), fd);

	return fd;
}


/*
 * Return the name of the current file.  The returned buffer may point to
 * internal state and must not be passed to free() or used after "state" is
 * freed.
 */
/*@keep@*/ const char *pv_current_file_name(pvstate_t state)
{
	static char *str_none = NULL;
	static char *str_stdin = NULL;
	const char *input_file_name = NULL;

	/*@-observertrans@ */
	/*@-onlytrans@ */
	/*@-statictrans@ */
	/*
	 * Here we are doing bad things with regards to whether the returned
	 * string is an allocated string from the state->files.filename array,
	 * a constant string, or a returned string from gettext(), but it
	 * has no impact.  We explicitly document, above, that the returned
	 * string expires with the state, and hence switch off the
	 * associated splint warnings.
	 */

	if (NULL == str_none)
		str_none = _("(none)");
	if (NULL == str_stdin)
		str_stdin = _("(stdin)");

	/* Fallback in case of translation failure. */
	if (NULL == str_none)
		str_none = "(none)";
	if (NULL == str_stdin)
		str_stdin = "(stdin)";

	if (state->status.current_input_file < 0)
		return str_none;
	if ((unsigned int) (state->status.current_input_file) >= state->files.file_count)
		return str_none;

	if (NULL == state->files.filename) {
		input_file_name = NULL;
	} else {
		input_file_name = state->files.filename[state->status.current_input_file];
	}
	if (NULL == input_file_name)
		return str_none;
	if (0 == strcmp(input_file_name, "-"))
		return str_stdin;

	/*@-compdef@ */
	return input_file_name;
	/*@+compdef@ */
	/*
	 * splint warns about state->files.filename being undefined, but we
	 * know it's been populated fully by the time this function is
	 * called.
	 */

	/*@+statictrans@ */
	/*@+onlytrans@ */
	/*@+observertrans@ */
}


/*
 * Return a stream pointer, and populate the filename buffer, for a control
 * file associated with a particular process ID, for the given signal
 * number; it will be opened for writing if "sender" is true.  Returns NULL
 * on error.
 */
/*@null@ */
/*@dependent@ */
FILE *pv_open_controlfile(char *filename, size_t bufsize, pid_t control_pid, int signum, bool sender)
{
	int open_flags, open_mode, control_fd;
	/*@dependent@ */ FILE *control_fptr = NULL;

	open_flags = O_RDONLY;
#ifdef O_NOFOLLOW
	open_flags += O_NOFOLLOW;
#endif
	if (sender)
		open_flags = O_WRONLY | O_CREAT | O_EXCL;

	open_mode = 0644;

	(void) pv_snprintf(filename, bufsize, "/run/user/%lu/pv.remote.%d.%lu", (unsigned long) geteuid(), signum,
			   (unsigned long) control_pid);
	control_fd = open(filename, open_flags, open_mode);	/* flawfinder: ignore */

	/*
	 * If /run/user/<uid> wasn't usable, try $HOME/.pv instead.
	 */
	if (control_fd < 0) {
		char *home_dir;

		home_dir = getenv("HOME");  /* flawfinder: ignore */
		if (NULL == home_dir)
			return NULL;
		if ('\0' == home_dir[0])
			return NULL;

		/*
		 * flawfinder rationale: null and zero-size values are
		 * rejected, and the destination buffer is bounded.
		 */

		(void) pv_snprintf(filename, bufsize, "%s/.pv/remote.%d.%lu", home_dir, signum,
				   (unsigned long) control_pid);
		control_fd = open(filename, open_flags, open_mode);	/* flawfinder: ignore */

		/*
		 * If the open failed, try creating the $HOME/.pv directory
		 * first.
		 */
		if (control_fd < 0) {
			(void) pv_snprintf(filename, bufsize, "%s/.pv", home_dir);
			(void) mkdir(filename, 0700);
			/* In case of weird umask, explicitly chmod the dir. */
			(void) chmod(filename, 0700);	/* flawfinder: ignore */
			(void) pv_snprintf(filename, bufsize, "%s/.pv/remote.%d.%lu", home_dir, signum,
					   (unsigned long) control_pid);
			control_fd = open(filename, open_flags, open_mode);	/* flawfinder: ignore */
		}
	}

	/*
	 * flawfinder rationale: the files are in a directory whose parents
	 * cannot be manipulated, and we are not allowing the final
	 * component to be a symbolic link.  We are checking that $HOME is
	 * not NULL, and it's bounded by pv_snprintf() so it can't overshoot
	 * the filename buffer.  When we chmod $HOME/.pv, we assume that an
	 * attacker is unlikely to be able to manipulate $HOME's contents to
	 * make use of us setting $HOME/.pv to mode 700.
	 */

	if (control_fd < 0)
		return NULL;

	debug("%s: %s", "control filename", filename);

	control_fptr = fdopen(control_fd, sender ? "wb" : "rb");

	return control_fptr;
}
