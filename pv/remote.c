/*
 * Remote-control and remote-query functions.
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
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

#ifdef PV_REMOTE_CONTROL

/* Structure for transferring settings with --remote. */
struct remote_msg {
	bool progress;			 /* progress bar flag */
	bool timer;			 /* timer flag */
	bool eta;			 /* ETA flag */
	bool fineta;			 /* absolute ETA flag */
	bool rate;			 /* rate counter flag */
	bool average_rate;		 /* average rate counter flag */
	bool bytes;			 /* bytes transferred flag */
	bool bufpercent;		 /* transfer buffer percentage flag */
	size_t lastwritten;		 /* last-written bytes count */
	off_t rate_limit;		 /* rate limit, in bytes per second */
	size_t buffer_size;		 /* buffer size, in bytes (0=default) */
	off_t size;			 /* total size of data */
	double interval;		 /* interval between updates */
	unsigned int width;		 /* screen width */
	unsigned int height;		 /* screen height */
	bool width_set_manually;	 /* width was set manually, not detected */
	bool height_set_manually;	 /* height was set manually, not detected */
	char name[256];			 /* flawfinder: ignore */
	char format[256];		 /* flawfinder: ignore */
	char extra_display[256];	 /* flawfinder: ignore */
};

/* Structure for transferring transfer state with --query. */
struct query_msg {
	long double elapsed_seconds;	 /* from state.transfer */
	off_t transferred;		 /* from state.transfer */
	off_t size;			 /* from state.control */
	bool response;			 /* true if this is the response signal. */
};

/*
 * flawfinder rationale: name, format, and extra_display are always
 * explicitly zeroed and bounded to one less than their size so they are
 * always \0 terminated.
 */


/*
 * Set the options of a remote process by writing them to a file, sending a
 * signal to the receiving process, and waiting for the message to be
 * consumed by the remote process.
 *
 * Returns nonzero on error.
 */
int pv_remote_set(pvstate_t state, pid_t remote)
{
	char control_filename[4096];	 /* flawfinder: ignore */
	FILE *control_fptr;
	struct remote_msg msgbuf;
	pid_t signal_sender;
	long timeout;
	bool received;

	/*
	 * flawfinder rationale: buffer is large enough, explicitly zeroed,
	 * and always bounded properly as we are only writing to it with
	 * pv_snprintf().
	 */

	/*
	 * Check that the remote process exists.
	 */
	if (kill((pid_t) (remote), 0) != 0) {
		pv_error("%u: %s", remote, strerror(errno));
		return PV_ERROREXIT_REMOTE_OR_PID;
	}

	/*
	 * Copy parameters into message buffer.
	 */
	memset(&msgbuf, 0, sizeof(msgbuf));
	msgbuf.progress = state->control.format_option.progress;
	msgbuf.timer = state->control.format_option.timer;
	msgbuf.eta = state->control.format_option.eta;
	msgbuf.fineta = state->control.format_option.fineta;
	msgbuf.rate = state->control.format_option.rate;
	msgbuf.average_rate = state->control.format_option.average_rate;
	msgbuf.bytes = state->control.format_option.bytes;
	msgbuf.bufpercent = state->control.format_option.bufpercent;
	msgbuf.lastwritten = state->control.format_option.lastwritten;
	msgbuf.rate_limit = state->control.rate_limit;
	msgbuf.buffer_size = state->control.target_buffer_size;
	msgbuf.size = state->control.size;
	msgbuf.interval = state->control.interval;
	msgbuf.width = (unsigned int) (state->control.width);
	msgbuf.height = (unsigned int) (state->control.height);
	msgbuf.width_set_manually = state->control.width_set_manually;
	msgbuf.height_set_manually = state->control.height_set_manually;

	if (state->control.name != NULL) {
		strncpy(msgbuf.name, state->control.name, sizeof(msgbuf.name) - 1);	/* flawfinder: ignore */
	}
	if (state->control.format_string != NULL) {
		strncpy(msgbuf.format, state->control.format_string, sizeof(msgbuf.format) - 1);	/* flawfinder: ignore */
	}
	if (state->control.extra_display_spec != NULL) {
		strncpy(msgbuf.extra_display, state->control.extra_display_spec, sizeof(msgbuf.extra_display) - 1);	/* flawfinder: ignore */
	}

	/*
	 * Make sure parameters are within sensible bounds.
	 */
	if (msgbuf.width < 1)
		msgbuf.width = 80;
	if (msgbuf.height < 1)
		msgbuf.height = 25;
	if (msgbuf.width > 999999)
		msgbuf.width = 999999;
	if (msgbuf.height > 999999)
		msgbuf.height = 999999;
	if ((msgbuf.interval > 0) && (msgbuf.interval < 0.1))
		msgbuf.interval = 0.1;
	if (msgbuf.interval > 600)
		msgbuf.interval = 600;

	/*
	 * flawfinder rationale: name, format, and extra_display are
	 * explicitly bounded to 1 less than the size of their buffer and
	 * the buffer is \0 terminated by memset() earlier.
	 */

	/*
	 * Get the filename and file stream to use for remote control.
	 */
	memset(control_filename, 0, sizeof(control_filename));
	control_fptr = pv_open_controlfile(control_filename, sizeof(control_filename), (pid_t) getpid(), SIGUSR2, true);
	if (NULL == control_fptr) {
		pv_error("%s", strerror(errno));
		return PV_ERROREXIT_REMOTE_OR_PID;
	}

	/*
	 * Write the message buffer to the remote control file, and close
	 * it.
	 */
	if (1 != fwrite(&msgbuf, sizeof(msgbuf), 1, control_fptr)) {
		pv_error("%s", strerror(errno));
		(void) fclose(control_fptr);
		(void) remove(control_filename);
		return PV_ERROREXIT_REMOTE_OR_PID;
	}

	if (0 != fclose(control_fptr)) {
		pv_error("%s", strerror(errno));
		(void) remove(control_filename);
		return PV_ERROREXIT_REMOTE_OR_PID;
	}

	/*
	 * Send a SIGUSR2 signal to the remote process, to tell it a message
	 * is ready to read, after clearing our own "SIGUSR2 received" flag.
	 */
	signal_sender = 0;
	(void) pv_sigusr2_received(state, &signal_sender);
	if (kill((pid_t) (remote), SIGUSR2) != 0) {
		pv_error("%u: %s", remote, strerror(errno));
		(void) remove(control_filename);
		return PV_ERROREXIT_REMOTE_OR_PID;
	}

	debug("%s", "message sent");

	/*
	 * Wait for a signal from the remote process to say it has received
	 * the message.
	 */

	timeout = 1100000;
	received = false;

	while (timeout > 10000 && !received) {
		struct timeval tv;

		memset(&tv, 0, sizeof(tv));
		tv.tv_sec = 0;
		tv.tv_usec = 10000;
		/*@-nullpass@ *//* splint: NULL is OK with select() */
		(void) select(0, NULL, NULL, NULL, &tv);
		/*@+nullpass@ */
		timeout -= 10000;

		if (pv_sigusr2_received(state, &signal_sender)) {
			if (signal_sender == remote) {
				debug("%s", "message received");
				received = true;
			}
		}
	}

	/*
	 * Remove the remote control file.
	 */
	debug("%s: %s", "removing", control_filename);
	if (0 != remove(control_filename)) {
		pv_error("%s", strerror(errno));
	}

	/*
	 * Return 0 if the message was received.
	 */
	if (received)
		return 0;

	/*@-mustfreefresh@ */
	/*
	 * splint note: the gettext calls made by _() cause memory leak
	 * warnings, but in this case it's unavoidable, and mitigated by the
	 * fact we only translate each string once.
	 */
	pv_error("%u: %s", remote, _("message not received"));
	return PV_ERROREXIT_REMOTE_OR_PID;
	/*@+mustfreefresh @ */
}


/*
 * Check for a --remote message (SIGUSR2), returning false if none was
 * found.
 *
 * If a message was received, update the current process's options with the
 * ones in the message.
 *
 * Note that this relies on pv_state_set_format() causing the output format
 * to be reparsed.
 */
static bool pv__rxsignal_usr2(pvstate_t state)
{
	pid_t signal_sender;
	char control_filename[4096];	 /* flawfinder: ignore */
	FILE *control_fptr;
	struct remote_msg msgbuf;

	/* flawfinder rationale: as above. */

	/*
	 * Return early if a SIGUSR2 signal has not been received.
	 */
	signal_sender = 0;
	if (!pv_sigusr2_received(state, &signal_sender))
		return false;

	memset(control_filename, 0, sizeof(control_filename));
	control_fptr = pv_open_controlfile(control_filename, sizeof(control_filename), signal_sender, SIGUSR2, false);
	if (NULL == control_fptr) {
		pv_error("%s: %s", control_filename, strerror(errno));
		return false;
	}

	/*
	 * Read the message buffer from the remote control file, and close
	 * it.
	 */
	if (1 != fread(&msgbuf, sizeof(msgbuf), 1, control_fptr)) {
		pv_error("%s", strerror(errno));
		(void) fclose(control_fptr);
		return false;
	}

	if (0 != fclose(control_fptr)) {
		pv_error("%s", strerror(errno));
		return false;
	}

	/*
	 * Send a SIGUSR2 signal to the sending process, to tell it the
	 * message has been received.
	 */
	if (kill(signal_sender, SIGUSR2) != 0) {
		debug("%u: %s", signal_sender, strerror(errno));
	}

	debug("%s", "received remote control message");

	pv_state_format_string_set(state, NULL);
	pv_state_name_set(state, NULL);
	pv_state_extra_display_set(state, NULL);

	msgbuf.name[sizeof(msgbuf.name) - 1] = '\0';
	msgbuf.format[sizeof(msgbuf.format) - 1] = '\0';
	msgbuf.extra_display[sizeof(msgbuf.extra_display) - 1] = '\0';

	pv_state_set_format(state, msgbuf.progress, msgbuf.timer,
			    msgbuf.eta, msgbuf.fineta, msgbuf.rate,
			    msgbuf.average_rate,
			    msgbuf.bytes, msgbuf.bufpercent,
			    msgbuf.lastwritten, '\0' == msgbuf.name[0] ? NULL : msgbuf.name);

	if (msgbuf.rate_limit > 0)
		pv_state_rate_limit_set(state, msgbuf.rate_limit);
	if (msgbuf.buffer_size > 0) {
		pv_state_target_buffer_size_set(state, msgbuf.buffer_size);
	}
	if (msgbuf.size > 0)
		pv_state_size_set(state, msgbuf.size);
	if (msgbuf.interval > 0)
		pv_state_interval_set(state, msgbuf.interval);
	if (msgbuf.width > 0 && msgbuf.width_set_manually)
		pv_state_width_set(state, msgbuf.width, msgbuf.width_set_manually);
	if (msgbuf.height > 0 && msgbuf.height_set_manually)
		pv_state_height_set(state, msgbuf.height, msgbuf.height_set_manually);
	if (msgbuf.format[0] != '\0')
		pv_state_format_string_set(state, msgbuf.format);
	if (msgbuf.extra_display[0] != '\0')
		pv_state_extra_display_set(state, msgbuf.extra_display);

	return true;
}


/*
 * Check for a --query message (SIGUSR1).
 *
 * If a type 0 message was received (query), then write a type 1 (response)
 * message to the control file and send a SIGUSR1 to the sending process.
 *
 * If a type 1 message was received (response), update the state from the
 * message in the control file.
 *
 * In both cases, the receiver of the message deletes its control file.
 *
 * The state transferred by message is transfer.elapsed_seconds,
 * transfer.transferred, and control.size.
 *
 * Returns true if a signal was received and dealt with, false otherwise.
 *
 * If match_sender is not zero, then ignores the signal and returns false if
 * the sending PID is not match_sender.
 */
static bool pv__rxsignal_usr1(pvstate_t state, pid_t match_sender)
{
	pid_t signal_sender;
	char control_filename[4096];	 /* flawfinder: ignore */
	FILE *control_fptr;
	struct query_msg msgbuf;

	/* flawfinder rationale: as above. */

	/*
	 * Return early if a SIGUSR1 signal has not been received.
	 */
	signal_sender = 0;
	if (!pv_sigusr1_received(state, &signal_sender))
		return false;

	/*
	 * If match_sender was specified, ignore the signal if the sender
	 * doesn't match.
	 */
	if ((0 != match_sender) && (signal_sender != match_sender)) {
		debug("%s=%d, %s=%d - %s", "match_sender", match_sender, "signal_sender", signal_sender,
		      "ignoring USR1");
		return false;
	}

	/*
	 * Note that we use debug() rather than pv_error() here so that the
	 * display of a running pv doesn't get interrupted by, for example,
	 * a querying pv being terminated.
	 */

	memset(control_filename, 0, sizeof(control_filename));
	control_fptr = pv_open_controlfile(control_filename, sizeof(control_filename), signal_sender, SIGUSR1, false);
	if (NULL == control_fptr) {
		debug("%s: %s", control_filename, strerror(errno));
		return false;
	}

	/*
	 * Read the message buffer from the remote control file, close it,
	 * and delete it.
	 */
	if (1 != fread(&msgbuf, sizeof(msgbuf), 1, control_fptr)) {
		debug("fread: %s", strerror(errno));
		(void) fclose(control_fptr);
		return false;
	}

	if (0 != fclose(control_fptr)) {
		debug("fclose: %s", strerror(errno));
		return false;
	}

	debug("%s: %s", "removing", control_filename);
	if (0 != remove(control_filename)) {
		debug("remove: %s", strerror(errno));
		return false;
	}

	if (msgbuf.response) {
		/* Response message - update local state. */
		debug("%s: %d [%Lg, %ld, %ld]", "query response received", signal_sender, msgbuf.elapsed_seconds,
		      msgbuf.transferred, msgbuf.size);
		state->transfer.elapsed_seconds = msgbuf.elapsed_seconds;
		state->transfer.transferred = msgbuf.transferred;
		state->control.size = msgbuf.size;
		return true;
	}

	/* Query message - transmit state to sender. */

	debug("%s: %d", "query received", signal_sender);

	msgbuf.elapsed_seconds = state->transfer.elapsed_seconds;
	msgbuf.transferred = state->transfer.transferred;
	msgbuf.size = state->control.size;
	msgbuf.response = true;

	memset(control_filename, 0, sizeof(control_filename));
	control_fptr = pv_open_controlfile(control_filename, sizeof(control_filename), (pid_t) getpid(), SIGUSR1, true);
	if (NULL == control_fptr) {
		debug("%s", strerror(errno));
		return true;		    /* true since the signal was received. */
	}

	if (1 != fwrite(&msgbuf, sizeof(msgbuf), 1, control_fptr)) {
		debug("fwrite: %s", strerror(errno));
		(void) fclose(control_fptr);
		(void) remove(control_filename);
		return true;		    /* as above. */
	}

	if (0 != fclose(control_fptr)) {
		debug("fclose: %s", strerror(errno));
		(void) remove(control_filename);
		return true;
	}

	/*
	 * Send a SIGUSR1 signal to the remote process, to tell it this
	 * query response message is ready to read.
	 */
	if (kill((pid_t) (signal_sender), SIGUSR1) != 0) {
		debug("%u: %s", signal_sender, strerror(errno));
		(void) remove(control_filename);
		return true;
	}

	debug("%s: %d", "query response sent", signal_sender);

	return true;
}


/*
 * Check for remote control messages.  For a SIGUSR2 (--remote), replace the
 * current process's options with those being passed in.  For a SIGUSR1
 * (--query), either receive transfer state from the sending process, or
 * send our transfer state to the sending process, depending on the content
 * of the message.
 *
 * NB --remote relies on pv_state_set_format() causing the output format to
 * be reparsed.
 *
 * Returns true if a --remote message was received, false otherwise.
 */
bool pv_remote_check(pvstate_t state)
{
	bool received_remote;

	received_remote = pv__rxsignal_usr2(state);
	(void) pv__rxsignal_usr1(state, 0);

	return received_remote;
}


/*
 * Replace the transfer state with that of the given process, including the
 * total transfer size.  If sizeptr is not NULL, the size is also copied to
 * *sizeptr (this is so a caller can get the size without knowing the
 * structure of pvstate_t).
 *
 * Returns nonzero on error.  If "silent" is false, reports the error.
 */
int pv_remote_transferstate_fetch(pvstate_t state, pid_t query, /*@null@ */ off_t * sizeptr, bool silent)
{
	char control_filename[4096];	 /* flawfinder: ignore */
	FILE *control_fptr;
	struct query_msg msgbuf;
	pid_t signal_sender;
	long timeout;
	bool received;

	/*
	 * flawfinder rationale: buffer is large enough, explicitly zeroed,
	 * and always bounded properly as we are only writing to it with
	 * pv_snprintf().
	 */

	/*
	 * Check that the remote process exists.
	 */
	if (kill((pid_t) (query), 0) != 0) {
		if (!silent)
			pv_error("%u: %s", query, strerror(errno));
		return PV_ERROREXIT_REMOTE_OR_PID;
	}

	/* Set up the query message. */
	memset(&msgbuf, 0, sizeof(msgbuf));
	msgbuf.response = false;

	/*
	 * Get the filename and file stream to use.
	 */
	memset(control_filename, 0, sizeof(control_filename));
	control_fptr = pv_open_controlfile(control_filename, sizeof(control_filename), (pid_t) getpid(), SIGUSR1, true);
	if (NULL == control_fptr) {
		if (!silent)
			pv_error("%s", strerror(errno));
		return PV_ERROREXIT_REMOTE_OR_PID;
	}

	/*
	 * Write the message to the control file, and close it.
	 */
	if (1 != fwrite(&msgbuf, sizeof(msgbuf), 1, control_fptr)) {
		if (!silent)
			pv_error("%s", strerror(errno));
		(void) fclose(control_fptr);
		(void) remove(control_filename);
		return PV_ERROREXIT_REMOTE_OR_PID;
	}

	if (0 != fclose(control_fptr)) {
		if (!silent)
			pv_error("%s", strerror(errno));
		(void) remove(control_filename);
		return PV_ERROREXIT_REMOTE_OR_PID;
	}

	/*
	 * Send a SIGUSR1 signal to the remote process, to tell it a query
	 * message is ready, after first clearing our own SIGUSR1 received
	 * flag.
	 */
	signal_sender = 0;
	(void) pv_sigusr1_received(state, &signal_sender);
	if (kill((pid_t) (query), SIGUSR1) != 0) {
		if (!silent)
			pv_error("%u: %s", query, strerror(errno));
		(void) remove(control_filename);
		return PV_ERROREXIT_REMOTE_OR_PID;
	}

	debug("%s: %d", "query sent", query);

	/*
	 * Wait for a SIGUSR1 signal to be sent back with a query response
	 * message.
	 */

	timeout = 1100000;
	received = false;

	while (timeout > 10000 && !received && 0 == state->flags.trigger_exit) {
		struct timeval tv;

		memset(&tv, 0, sizeof(tv));
		tv.tv_sec = 0;
		tv.tv_usec = 10000;
		/*@-nullpass@ *//* splint: NULL is OK with select() */
		(void) select(0, NULL, NULL, NULL, &tv);
		/*@+nullpass@ */
		timeout -= 10000;

		if (pv__rxsignal_usr1(state, query)) {
			debug("%s", "response received");
			received = true;
			if (NULL != sizeptr)
				*sizeptr = state->control.size;
		}
	}

	/*
	 * Remove the control file in case it still exists - the other
	 * process should have removed it.
	 */
	debug("%s: %s", "cleaning up", control_filename);
	(void) remove(control_filename);

	/*
	 * Return 0 if the message was received.
	 */
	if (received)
		return 0;

	/*@-mustfreefresh@ */
	/*
	 * splint note: the gettext calls made by _() cause memory leak
	 * warnings, but in this case it's unavoidable, and mitigated by the
	 * fact we only translate each string once.
	 */
	if (!silent)
		pv_error("%u: %s", query, _("message not received"));
	return PV_ERROREXIT_REMOTE_OR_PID;
	/*@+mustfreefresh @ */

}


#else				/* !PV_REMOTE_CONTROL */

/*
 * Dummy stubs for remote control when we don't have PV_REMOTE_CONTROL.
 */

bool pv_remote_check( /*@unused@ */  __attribute__((unused)) pvstate_t state)
{
}


int pv_remote_set(			 /*@unused@ */
			 __attribute__((unused)) pvstate_t state, /*@unused@ */  __attribute__((unused)) pid_t remote)
{
	/*@-mustfreefresh@ *//* splint - see above */
	pv_error("%s", _("SA_SIGINFO not supported on this system"));
	/*@+mustfreefresh@ */
	return PV_ERROREXIT_REMOTE_OR_PID;
}


int pv_remote_transferstate_fetch(	 /*@unused@ */
					 __attribute__((unused)) pvstate_t state,
					 /*@unused@ */
					 __attribute__((unused)) pid_t query,
					 /*@null@ */
					 /*@unused@ */
					 __attribute__((unused)) off_t * sizeptr,
					 /*@unused@ */
					 __attribute__((unused))
					 bool silent)
{
	/*@-mustfreefresh@ *//* splint - see above */
	fprintf(stderr, "%s\n", _("SA_SIGINFO not supported on this system"));
	/*@+mustfreefresh@ */
	return PV_ERROREXIT_REMOTE_OR_PID;
}

#endif				/* PV_REMOTE_CONTROL */
