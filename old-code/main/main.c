/*
 * Main program entry point - read the command line options, then perform
 * the appropriate actions.
 *
 * Copyright 2002-2008, 2010, 2012-2015, 2017, 2021, 2023-2025 Andrew Wood
 *
 * License GPLv3+: GNU GPL version 3 or later; see `docs/COPYING'.
 */

#include "config.h"
#include "options.h"
#include "pv.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_LANGINFO_H
#include <langinfo.h>
#endif

/*
 * Write a PID file, returning nonzero on error.  Write it atomically, such
 * that the file either exists and contains the PID, or is not updated at
 * all.  This is done by writing to a temporary file in the same directory
 * first, and then renaming the temporary file to the target name.
 */
static int pv__write_pidfile(opts_t opts)
{
	char *pidfile_tmp_name;
	size_t pidfile_tmp_bufsize;
	int pidfile_tmp_fd;
	FILE *pidfile_tmp_fptr;
	mode_t prev_umask;
	const char *pidfile_template = "%s.XXXXXX";

	if (NULL == opts->pidfile)
		return 0;

	/*
	 * The buffer needs to be long enough to hold the pidfile with the
	 * mkstemp template ".XXXXXX" after it.  The "%s" of our
	 * pidfile_template adds 2 extra bytes to the length, of which we
	 * need 1 byte for the terminating \0, so we subtract 1 byte more to
	 * get the exact amount of space we need.
	 */
	pidfile_tmp_bufsize = strlen(pidfile_template) + strlen(opts->pidfile) - 1;	/* flawfinder: ignore */

	/*
	 * flawfinder rationale: flawfinder never likes strlen() in case
	 * it's called on a string that isn't \0 terminated.  We have to use
	 * strlen() to find the length of opts->pidfile, so have to trust
	 * that the arguments in argv[] were \0 terminated.  We can be sure
	 * that pidfile_template is \0 terminated because we've set it to a
	 * constant value.  So we tell flawfinder to skip this check here.
	 */
	pidfile_tmp_name = malloc(pidfile_tmp_bufsize);
	if (NULL == pidfile_tmp_name) {
		fprintf(stderr, "%s: %s\n", opts->program_name, strerror(errno));
		return PV_ERROREXIT_REMOTE_OR_PID;
	}
	memset(pidfile_tmp_name, 0, pidfile_tmp_bufsize);
	(void) pv_snprintf(pidfile_tmp_name, pidfile_tmp_bufsize, pidfile_template, opts->pidfile);

	/*@-type@ *//* splint doesn't like mode_t */
	prev_umask = umask(0000);	    /* flawfinder: ignore */
	(void) umask(prev_umask | 0133);    /* flawfinder: ignore */

	/*@-unrecog@ *//* splint doesn't know mkstemp() */
	pidfile_tmp_fd = mkstemp(pidfile_tmp_name);	/* flawfinder: ignore */
	/*@+unrecog@ */
	if (pidfile_tmp_fd < 0) {
		fprintf(stderr, "%s: %s: %s\n", opts->program_name, pidfile_tmp_name, strerror(errno));
		(void) umask(prev_umask);   /* flawfinder: ignore */
		free(pidfile_tmp_name);
		return PV_ERROREXIT_REMOTE_OR_PID;
	}

	(void) umask(prev_umask);	    /* flawfinder: ignore */

	/*
	 * flawfinder rationale (umask, mkstemp) - flawfinder
	 * recommends setting the most restrictive umask possible
	 * when calling mkstemp(), so this is what we have done.
	 *
	 * We get the original umask and OR it with 0133 to make
	 * sure new files will be at least chmod 644.  Then we put
	 * the umask back to what it was, after creating the
	 * temporary file.
	 */

	/*@+type@ */

	pidfile_tmp_fptr = fdopen(pidfile_tmp_fd, "w");
	if (NULL == pidfile_tmp_fptr) {
		fprintf(stderr, "%s: %s: %s\n", opts->program_name, pidfile_tmp_name, strerror(errno));
		(void) close(pidfile_tmp_fd);
		(void) remove(pidfile_tmp_name);
		free(pidfile_tmp_name);
		return PV_ERROREXIT_REMOTE_OR_PID;
	}

	fprintf(pidfile_tmp_fptr, "%d\n", getpid());
	if (0 != fclose(pidfile_tmp_fptr)) {
		fprintf(stderr, "%s: %s: %s\n", opts->program_name, opts->pidfile, strerror(errno));
	}

	if (rename(pidfile_tmp_name, opts->pidfile) < 0) {
		fprintf(stderr, "%s: %s: %s\n", opts->program_name, opts->pidfile, strerror(errno));
		(void) remove(pidfile_tmp_name);
	}

	free(pidfile_tmp_name);

	return 0;
}


/*
 * Set the output file, if applicable.  Returns nonzero on error.
 */
static int pv__set_output(pvstate_t state, opts_t opts, /*@null@ */ const char *output_file)
{
	int output_fd;

	if ((NULL == state) || (NULL == opts))
		return 0;

	if (NULL == output_file || 0 == strcmp(output_file, "-")) {
		pv_state_output_set(state, STDOUT_FILENO, "(stdout)");
		return 0;
	}

	output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0600);	/* flawfinder: ignore */
	/*
	 * flawfinder rationale: the output filename has been
	 * explicitly provided, and in many cases the operator will
	 * want to write to device files and other special
	 * destinations, so there is no sense-checking we can do to
	 * make this safer.
	 */
	if (output_fd < 0) {
		fprintf(stderr, "%s: %s: %s\n", opts->program_name, output_file, strerror(errno));
		return PV_ERROREXIT_ACCESS;
	}

	pv_state_output_set(state, output_fd, output_file);
	return 0;
}


/*
 * Run in store-and-forward mode: run the main loop once with the output
 * forced to the store-and-forward file (taking care of creation and removal
 * of a temporary file if "-" was specified); then run the main loop again
 * with the input file list forced to be just the store-and-forward file. 
 * Returns nonzero on error.
 */
static int pv__store_and_forward(pvstate_t state, opts_t opts, bool can_have_eta)
{
	char tmp_filename[4096];	 /* flawfinder: ignore */
	bool use_temporary_file;
	char *real_store_and_forward_file;
	int retcode;

	/* flawfinder: zeroed with memset and bounded by pv_snprintf. */

	if ((NULL == state) || (NULL == opts) || (NULL == opts->store_and_forward_file))
		return 0;

	memset(tmp_filename, 0, sizeof(tmp_filename));

	use_temporary_file = false;
	if (0 == strcmp(opts->store_and_forward_file, "-"))
		use_temporary_file = true;

	/*
	 * Create a temporary file if the specified file was "-".
	 */
	if (use_temporary_file) {
		char *tmpdir;
		int tmp_fd;

		tmpdir = (char *) getenv("TMPDIR");	/* flawfinder: ignore */
		if ((NULL == tmpdir) || ('\0' == tmpdir[0]))
			tmpdir = (char *) getenv("TMP");	/* flawfinder: ignore */
		if ((NULL == tmpdir) || ('\0' == tmpdir[0]))
			tmpdir = "/tmp";

		/*
		 * flawfinder rationale: null and zero-size values of $TMPDIR and
		 * $TMP are rejected, and the destination buffer is bounded.
		 */

		(void) pv_snprintf(tmp_filename, sizeof(tmp_filename), "%s/pv.XXXXXX", tmpdir);
		/*@-unrecog@ *//* splint doesn't know mkstemp() */
		tmp_fd = mkstemp(tmp_filename);	/* flawfinder: ignore */
		/*@+unrecog@ */
		if (tmp_fd < 0) {
			fprintf(stderr, "%s: %s: %s\n", opts->program_name, tmp_filename, strerror(errno));
			return PV_ERROREXIT_SAF;
		}
		(void) close(tmp_fd);
	}

	/*
	 * Real store-and-forward file: either the one we were given, or the
	 * temporary file we created if we were given "-".
	 */
	real_store_and_forward_file = use_temporary_file ? tmp_filename : opts->store_and_forward_file;

	/*
	 * First, set the output file to the store-and-forward file.
	 */
	debug("%s: %s", "setting output to store-and-forward file", real_store_and_forward_file);
	retcode = pv__set_output(state, opts, real_store_and_forward_file);
	if (0 != retcode)
		goto end_store_and_forward;

	/* Reset the formatting to set the displayed name to "(input)". */
	/*@-mustfreefresh@ */
	pv_state_set_format(state, opts->progress, opts->timer, can_have_eta ? opts->eta : false,
			    can_have_eta ? opts->fineta : false, opts->rate, opts->average_rate,
			    opts->bytes, opts->bufpercent, opts->lastwritten, _("(input)"));
	/*@+mustfreefresh@ *//* see below about gettext _() calls. */

	/* Run the main loop as normal. */
	debug("%s", "running store-and-forward receiver");
	retcode = pv_main_loop(state);
	if (0 != retcode)
		goto end_store_and_forward;

	/* Set the output file back to what it originally was. */
	debug("%s: %s", "setting output to original value", NULL == opts->output ? "(null)" : opts->output);
	retcode = pv__set_output(state, opts, opts->output);
	if (0 != retcode)
		goto end_store_and_forward;

	/* Replace the list of input files with the store-and-forward file. */
	debug("%s", "resetting input file list");
	pv_state_inputfiles(state, 1, (const char **) &real_store_and_forward_file);

	/* Recalculate the input size. */
	pv_state_size_set(state, pv_calc_total_size(state));

	/* Reset the format, since we might have been asked to show ETA. */
	pv_state_set_format(state, opts->progress, opts->timer, opts->eta,
			    opts->fineta, opts->rate, opts->average_rate,
			    opts->bytes, opts->bufpercent, opts->lastwritten, opts->name);

	/* Reset calculated values in the state. */
	pv_state_reset(state);

	/* Run the main loop again. */
	debug("%s", "running store-and-forward transmitter");
	retcode = pv_main_loop(state);

      end_store_and_forward:
	if (use_temporary_file)
		(void) remove(tmp_filename);

	return retcode;
}


/*
 * Process command-line arguments and set option flags, then call functions
 * to initialise, and finally enter the main loop.
 */
int main(int argc, char **argv)
{
	/*@only@ */ opts_t opts = NULL;
	/*@only@ */ pvstate_t state = NULL;
	int retcode = 0;
	bool can_have_eta = true;
	bool terminal_supports_utf8 = false;

#if ! HAVE_SETPROCTITLE
	initproctitle(argc, argv);
#endif

#ifdef ENABLE_NLS
	/* Initialise language translation. */
	(void) setlocale(LC_ALL, "");
	(void) bindtextdomain(PACKAGE, LOCALEDIR);
	(void) textdomain(PACKAGE);
#ifdef HAVE_LANGINFO_H
	/*@-mustfreefresh@ *//* splint thinks nl_langinfo() leaks memory */
	if (0 == strcmp(nl_langinfo(CODESET), "UTF-8"))
		terminal_supports_utf8 = true;
	/*@+mustfreefresh@ */
#endif
#endif

	/* Parse the command line arguments. */
	opts = opts_parse(argc >= 0 ? (unsigned int) argc : 0, argv);
	if (NULL == opts) {
		debug("%s: %d", "exiting with status", PV_ERROREXIT_MEMORY);
		return PV_ERROREXIT_MEMORY;
	}

	/* Early exit if necessary, such as with "-h". */
	if (PV_ACTION_NOTHING == opts->action) {
		debug("%s", "nothing to do - exiting with status 0");
		opts_free(opts);
		return 0;
	}

	/* Set the error message prefix. */
	/*@-keeptrans@ */
	pv_set_error_prefix(opts->program_name);
	/* splint - this function doesn't add an alias or release it. */
	/*@+keeptrans@ */

	/*
	 * Allocate our internal state buffer.
	 */
	state = pv_state_alloc();
	if (NULL == state) {
		/*@-mustfreefresh@ */
		/*
		 * splint note: the gettext calls made by _() cause memory
		 * leak warnings, but in this case it's unavoidable, and
		 * mitigated by the fact we only translate each string once.
		 */
		fprintf(stderr, "%s: %s: %s\n", opts->program_name, _("state allocation failed"), strerror(errno));
		opts_free(opts);
		debug("%s: %d", "exiting with status", PV_ERROREXIT_MEMORY);
		return PV_ERROREXIT_MEMORY;
		/*@+mustfreefresh@ */
	}

	/*
	 * Write a PID file if -P was specified.
	 */
	if (opts->pidfile != NULL) {
		int pidfile_rc;
		pidfile_rc = pv__write_pidfile(opts);
		if (0 != pidfile_rc) {
			pv_state_free(state);
			opts_free(opts);
			return pidfile_rc;
		}
	}

	/*
	 * If no files were given, pretend "-" was given (stdin).
	 */
	if (0 == opts->argc) {
		debug("%s", "no files given - adding fake argument `-'");
		if (!opts_add_file(opts, "-")) {
			pv_state_free(state);
			opts_free(opts);
			return PV_ERROREXIT_MEMORY;
		}
	}

	/*
	 * Put our list of input files into the PV internal state.
	 */
	if (NULL != opts->argv) {
		pv_state_inputfiles(state, opts->argc, (const char **) (opts->argv));
	}

	/*
	 * Put the list of watchfd items into the PV internal state.
	 */
	if ((opts->watchfd_count > 0) && (NULL != opts->watchfd_pid) && (NULL != opts->watchfd_fd)) {
		pv_state_watchfds(state, opts->watchfd_count, opts->watchfd_pid, opts->watchfd_fd);
	}

	/*
	 * If stderr is not a terminal and we're neither forcing output nor
	 * outputting numerically, we will have nothing to display at all.
	 */
	if ((0 == isatty(STDERR_FILENO))
	    && (false == opts->force)
	    && (false == opts->numeric)) {
		opts->no_display = true;
		debug("%s", "nothing to display - setting no_display");
	}

	/*
	 * Auto-detect width or height if either are unspecified.
	 */
	if ((0 == opts->width) || (0 == opts->height)) {
		unsigned int width, height;
		width = 0;
		height = 0;
		pv_screensize(&width, &height);
		if (0 == opts->width) {
			opts->width = width;
			debug("%s: %u", "auto-detected terminal width", width);
		}
		if (0 == opts->height) {
			opts->height = height;
			debug("%s: %u", "auto-detected terminal height", height);
		}
	}

	/*
	 * Width and height bounds checking (and defaults).
	 */
	if (opts->width < 1)
		opts->width = 80;
	if (opts->height < 1)
		opts->height = 25;
	if (opts->width > 999999)
		opts->width = 999999;
	if (opts->height > 999999)
		opts->height = 999999;

	/*
	 * Interval must be at least 0.1 second, and at most 10 minutes.
	 */
	if (opts->interval < 0.1)
		opts->interval = 0.1;
	if (opts->interval > 600)
		opts->interval = 600;

	/*
	 * Set output file, treating no output or "-" as stdout; we have to
	 * do this before looking at setting the size, as the size
	 * calculation looks at the output file if the input size can't be
	 * calculated (issue #91).
	 *
	 * We have to set the sparse output flag before doing this, so that
	 * in sparse mode the lseek() on O_APPEND can be done (issue #45);
	 * see the comments in pv_state_output_set() in src/pv/state.c.
	 */
	pv_state_sparse_output_set(state, opts->sparse_output);
	retcode = pv__set_output(state, opts, opts->output);
	if (0 != retcode) {
		pv_state_free(state);
		opts_free(opts);
		return retcode;
	}

	/*
	 * Copy the "stop at size" option before checking the total size,
	 * since calculating the size from the output block device size
	 * after this may want to force this setting on, and if we set it
	 * afterwards, we undo the override.
	 */
	pv_state_stop_at_size_set(state, opts->stop_at_size);

	/* Total size calculation, in normal transfer mode. */
	if (PV_ACTION_TRANSFER == opts->action) {
		/*
		 * If no size was given, try to calculate the total size.
		 */
		if (0 == opts->size) {
			pv_state_linemode_set(state, opts->linemode);
			pv_state_null_terminated_lines_set(state, opts->null_terminated_lines);
			opts->size = pv_calc_total_size(state);
			debug("%s: %llu", "no size given - calculated", opts->size);
		}

		/*
		 * If the size is unknown, we cannot have an ETA.
		 */
		if (opts->size < 1) {
			can_have_eta = false;
			debug("%s", "size unknown - ETA disabled");
		}
	}

	/* Initialise the signal handling. */
	pv_sig_init(state);

	/*
	 * Get the total size, in query mode.
	 *
	 * Note that this uses signals, so signal handling has to have been
	 * set up first.
	 */
	if (PV_ACTION_QUERY == opts->action) {
		opts->size = 0;
		retcode = pv_remote_transferstate_fetch(state, opts->query, &(opts->size), false);
		if (0 != retcode) {
			pv_sig_fini(state);
			pv_state_free(state);
			opts_free(opts);
			return retcode;
		}
		/* As above - no ETA if the size is unknown. */
		if (opts->size < 1) {
			can_have_eta = false;
			debug("%s", "size unknown - ETA disabled");
		}
	}

	/*
	 * Copy the remaining parameters from the options into the main
	 * state.
	 */

	pv_state_interval_set(state, opts->interval);
	pv_state_width_set(state, opts->width, opts->width_set_manually);
	pv_state_height_set(state, opts->height, opts->height_set_manually);
	pv_state_no_display_set(state, opts->no_display);
	pv_state_force_set(state, opts->force);
	pv_state_cursor_set(state, opts->cursor);
	pv_state_show_stats_set(state, opts->show_stats);
	pv_state_numeric_set(state, opts->numeric);
	pv_state_wait_set(state, opts->wait);
	pv_state_delay_start_set(state, opts->delay_start);
	pv_state_rate_gauge_set(state, opts->rate_gauge);
	pv_state_linemode_set(state, opts->linemode);
	pv_state_bits_set(state, opts->bits);
	pv_state_decimal_units_set(state, opts->decimal_units);
	pv_state_null_terminated_lines_set(state, opts->null_terminated_lines);
	pv_state_skip_errors_set(state, opts->skip_errors);
	pv_state_error_skip_block_set(state, opts->error_skip_block);
	pv_state_sync_after_write_set(state, opts->sync_after_write);
	pv_state_direct_io_set(state, opts->direct_io);
	pv_state_discard_input_set(state, opts->discard_input);
	pv_state_rate_limit_set(state, opts->rate_limit);
	pv_state_target_buffer_size_set(state, opts->buffer_size);
	pv_state_no_splice_set(state, opts->no_splice);
	pv_state_size_set(state, opts->size);
	pv_state_name_set(state, opts->name);
	pv_state_default_bar_style_set(state, opts->default_bar_style);
	pv_state_format_string_set(state, opts->format);
	pv_state_extra_display_set(state, opts->extra_display);
	pv_state_average_rate_window_set(state, opts->average_rate_window);

	pv_state_set_format(state, opts->progress, opts->timer, can_have_eta ? opts->eta : false,
			    can_have_eta ? opts->fineta : false, opts->rate, opts->average_rate,
			    opts->bytes, opts->bufpercent, opts->lastwritten, opts->name);

	debug("%s: %s", "terminal_supports_utf8", terminal_supports_utf8 ? "true" : "false");
	pv_state_set_terminal_supports_utf8(state, terminal_supports_utf8);

	/* Run the appropriate main loop. */
	switch (opts->action) {
	case PV_ACTION_NOTHING:
		break;
	case PV_ACTION_TRANSFER:
		/* Normal "transfer data" mode. */
		retcode = pv_main_loop(state);
		break;
	case PV_ACTION_STORE_AND_FORWARD:
		/* Store-and-forward transfer mode. */
		retcode = pv__store_and_forward(state, opts, can_have_eta);
		break;
	case PV_ACTION_WATCHFD:
		/* "Watch file descriptor(s) of another process" mode. */
		retcode = pv_watchfd_loop(state);
		break;
	case PV_ACTION_REMOTE_CONTROL:
		/* Change the options of another running pv. */
		retcode = pv_remote_set(state, opts->remote);
		break;
	case PV_ACTION_QUERY:
		/* Query the progress of another running pv. */
		retcode = pv_query_loop(state, opts->query);
		break;
	}

	/* Clear up the PID file, if one was written. */
	if (opts->pidfile != NULL) {
		if (0 != remove(opts->pidfile)) {
			fprintf(stderr, "%s: %s: %s\n", opts->program_name, opts->pidfile, strerror(errno));
		}
	}

	/* Close down the signal handling. */
	pv_sig_fini(state);

	/* Free the internal PV state. */
	pv_state_free(state);

	/* Free the data from parsing the command-line arguments. */
	opts_free(opts);

	debug("%s: %d", "exiting with status", retcode);

	return retcode;
}
