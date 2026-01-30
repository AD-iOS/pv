/*
 * Cursor positioning functions.
 *
 * If IPC is available, then a shared memory segment is used to co-ordinate
 * cursor positioning across multiple instances of `pv'. The shared memory
 * segment contains an integer which is the original "y" co-ordinate of the
 * first `pv' process.
 *
 * However, some OSes (FreeBSD and MacOS X so far) don't allow locking of a
 * terminal, so we try to use a lockfile if terminal locking doesn't work,
 * and finally abort if even that is unavailable.
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
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#ifdef HAVE_IPC
#include <sys/ipc.h>
#include <sys/shm.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#endif				/* HAVE_IPC */

/* for basename() */
#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#endif


/*
 * Create a per-euid, per-tty, lockfile in ${TMPDIR:-${TMP:-/tmp}} for the
 * tty on the given file descriptor.
 */
static void pv_crs_open_lockfile(pvcursorstate_t cursor, readonly_pvcontrol_t control, int fd)
{
	char *ttydev;
	char *tmpdir;
	int openflags;

	cursor->lock_fd = -1;

	ttydev = ttyname(fd);
	if (!ttydev) {
		if (!control->force) {
			pv_error("%s: %s", _("failed to get terminal name"), strerror(errno));
		}
		/*
		 * If we don't know our terminal name, we can neither do IPC
		 * nor make a lock file, so turn off cursor positioning.
		 */
		cursor->disable = true;
		debug("%s", "ttyname failed - cursor positioning disabled");
		return;
	}

	tmpdir = (char *) getenv("TMPDIR"); /* flawfinder: ignore */
	if ((NULL == tmpdir) || ('\0' == tmpdir[0]))
		tmpdir = (char *) getenv("TMP");	/* flawfinder: ignore */
	if ((NULL == tmpdir) || ('\0' == tmpdir[0]))
		tmpdir = "/tmp";

	/*
	 * flawfinder rationale: null and zero-size values of $TMPDIR and
	 * $TMP are rejected, and the destination buffer is bounded.
	 */

	memset(cursor->lock_file, 0, PV_SIZEOF_CRS_LOCK_FILE);
	(void) pv_snprintf(cursor->lock_file,
			   PV_SIZEOF_CRS_LOCK_FILE, "%s/pv-%s-%i.lock", tmpdir, basename(ttydev), (int) geteuid());

	/*
	 * Pawel Piatek - not everyone has O_NOFOLLOW, e.g. AIX doesn't.
	 */
#ifdef O_NOFOLLOW
	openflags = O_RDWR | O_CREAT | O_NOFOLLOW;
#else
	openflags = O_RDWR | O_CREAT;
#endif

	cursor->lock_fd = open(cursor->lock_file, openflags, 0600);	/* flawfinder: ignore */

	/*
	 * flawfinder rationale: we aren't truncating the lock file, we
	 * don't change its contents, and we are attempting to use
	 * O_NOFOLLOW where possible to avoid symlink attacks, so this
	 * open() is as safe as we can make it.
	 */

	if (cursor->lock_fd < 0) {
		pv_error("%s: %s: %s", cursor->lock_file, _("failed to open lock file"), strerror(errno));
		cursor->disable = true;
		return;
	}
}


/*
 * Lock the terminal on the given file descriptor, falling back to using a
 * lockfile if the terminal itself cannot be locked.
 */
static void pv_crs_lock(pvcursorstate_t cursor, readonly_pvcontrol_t control, int fd)
{
	struct flock lock;
	int lock_fd;

	lock_fd = fd;
	if (cursor->lock_fd >= 0)
		lock_fd = cursor->lock_fd;

	memset(&lock, 0, sizeof(lock));
	lock.l_type = (short) F_WRLCK;
	lock.l_whence = (short) SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 1;
	while (fcntl(lock_fd, F_SETLKW, &lock) < 0) {
		if (errno != EINTR) {
			if (cursor->lock_fd == -2) {
				pv_crs_open_lockfile(cursor, control, fd);
				if (cursor->lock_fd >= 0) {
					lock_fd = cursor->lock_fd;
				}
			} else {
				pv_error("%s: %s", _("lock attempt failed"), strerror(errno));
				return;
			}
		}
	}

	if (cursor->lock_fd >= 0) {
		debug("%s: %s", cursor->lock_file, "terminal lockfile acquired");
	} else {
		debug("%s", "terminal lock acquired");
	}
}


/*
 * Unlock the terminal on the given file descriptor.  If pv_crs_lock used
 * lockfile locking, unlock the lockfile.
 */
static void pv_crs_unlock(pvcursorstate_t cursor, int fd)
{
	struct flock lock;
	int lock_fd;

	lock_fd = fd;
	if (cursor->lock_fd >= 0)
		lock_fd = cursor->lock_fd;

	memset(&lock, 0, sizeof(lock));
	lock.l_type = (short) F_UNLCK;
	lock.l_whence = (short) SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 1;
	(void) fcntl(lock_fd, F_SETLK, &lock);

	if (cursor->lock_fd >= 0) {
		debug("%s: %s", cursor->lock_file, "terminal lockfile released");
	} else {
		debug("%s", "terminal lock released");
	}
}


#ifdef HAVE_IPC
/*
 * Get the current number of processes attached to our shared memory
 * segment, i.e. find out how many `pv' processes in total are running in
 * cursor mode (including us), and store it in pv_crs_pvcount. If this is
 * larger than pv_crs_pvmax, update pv_crs_pvmax.
 */
static void pv_crs_ipccount(pvcursorstate_t cursor)
{
	struct shmid_ds buf;

	memset(&buf, 0, sizeof(buf));
	buf.shm_nattch = 0;

	(void) shmctl(cursor->shmid, IPC_STAT, &buf);
	cursor->pvcount = (int) (buf.shm_nattch);

	if (cursor->pvcount > cursor->pvmax)
		cursor->pvmax = cursor->pvcount;

	debug("%s: %d", "pvcount", cursor->pvcount);
}
#endif				/* HAVE_IPC */


/*
 * Get the current cursor Y co-ordinate by sending the ECMA-48 CPR code to
 * the terminal connected to the given file descriptor.
 *
 * This should only be called while the terminal is locked, to avoid
 * confusing other "pv -c" instances about the current terminal attributes,
 * since this function temporarily changes the terminal attributes and then
 * puts them back.
 */
static int pv_crs_get_ypos(int terminalfd)
{
	struct termios tty;
	struct termios old_tty;
	char cpr[32];			 /* flawfinder: ignore - bounded, zeroed */
	int ypos;
	ssize_t r;
#ifdef CURSOR_ANSWERBACK_BYTE_BY_BYTE
	int got;
#endif				/* CURSOR_ANSWERBACK_BYTE_BY_BYTE */

	if (0 != tcgetattr(terminalfd, &tty)) {
		debug("%s: %s", "tcgetattr (1) failed", strerror(errno));
	}
	if (0 != tcgetattr(terminalfd, &old_tty)) {
		debug("%s: %s", "tcgetattr (2) failed", strerror(errno));
	}
	tty.c_lflag &= ~(ICANON | ECHO);
	if (0 != tcsetattr(terminalfd, TCSANOW | TCSAFLUSH, &tty)) {
		debug("%s: %s", "tcsetattr (1) failed", strerror(errno));
	}

	pv_write_retry(terminalfd, "\033[6n", 4);

	memset(cpr, 0, sizeof(cpr));

#ifdef CURSOR_ANSWERBACK_BYTE_BY_BYTE
	/* Read answerback byte by byte - fails on AIX */
	for (got = 0, r = 0; got < (int) (sizeof(cpr) - 2); got += r) {
		r = read(terminalfd, cpr + got, 1);	/* flawfinder: ignore */
		/* flawfinder rationale: bounded to buffer size by "for" */
		if (r <= 0) {
			debug("got=%d, r=%d: %s", got, r, strerror(errno));
			break;
		}
		if (cpr[got] == 'R')
			break;
	}

	debug
	    ("read answerback message from fd %d, length %d - buf = %02X %02X %02X %02X %02X %02X",
	     terminalfd, got, cpr[0], cpr[1], cpr[2], cpr[3], cpr[4], cpr[5]);

#else				/* !CURSOR_ANSWERBACK_BYTE_BY_BYTE */
	/* Read answerback in one big lump - may fail on Solaris */
	r = read(terminalfd, cpr, sizeof(cpr) - 2);	/* flawfinder: ignore */
	/* flawfinder rationale: bounded to buffer size */
	if (r <= 0) {
		debug("r=%d: %s", r, strerror(errno));
	} else {
		debug
		    ("read answerback message from fd %d, length %d - buf = %02X %02X %02X %02X %02X %02X",
		     terminalfd, r, cpr[0], cpr[1], cpr[2], cpr[3], cpr[4], cpr[5]);

	}
#endif				/* !CURSOR_ANSWERBACK_BYTE_BY_BYTE */

	ypos = (int) pv_getnum_count(cpr + 2, false);

	if (0 != tcsetattr(terminalfd, TCSANOW | TCSAFLUSH, &old_tty)) {
		debug("%s: %s", "tcsetattr (2) failed", strerror(errno));
	}

	debug("%s: %d", "ypos", ypos);

	return ypos;
}


#ifdef HAVE_IPC
/*
 * Initialise the IPC data, returning nonzero on error.
 *
 * To do this, we attach to the shared memory segment (creating it if it
 * does not exist). If we are the only process attached to it, then we
 * initialise it with the current cursor position.
 *
 * There is a race condition here: another process could attach before we've
 * had a chance to check, such that no process ends up getting an "attach
 * count" of one, and so no initialisation occurs. So, we lock the terminal
 * with pv_crs_lock() while we are attaching and checking.
 */
static int pv_crs_ipcinit(pvcursorstate_t cursor, readonly_pvcontrol_t control, char *ttyfile, int terminalfd)
{
	key_t key;

	/*
	 * Base the key for the shared memory segment on our current tty, so
	 * we don't end up interfering in any way with instances of `pv'
	 * running on another terminal.
	 */
	key = ftok(ttyfile, (int) 'p');
	if (-1 == key) {
		debug("%s: %s\n", "ftok failed", strerror(errno));
		return 1;
	}

	pv_crs_lock(cursor, control, terminalfd);
	if ((!control->cursor) || (cursor->disable)) {
		debug("%s", "early return - cursor has been disabled");
		return 1;
	}

	cursor->shmid = shmget(key, sizeof(struct pvipccursorstate_s), 0600 | IPC_CREAT);
	if (cursor->shmid < 0) {
		debug("%s: %s", "shmget failed", strerror(errno));
		pv_crs_unlock(cursor, terminalfd);
		return 1;
	}

	/*@-nullpass@ */
	/* splint doesn't know shmaddr can be NULL */
	cursor->shared = shmat(cursor->shmid, NULL, 0);
	/*@+nullpass@ */

	pv_crs_ipccount(cursor);

	/*
	 * If nobody else is attached to the shared memory segment, we're
	 * the first, so we need to initialise the shared memory with our
	 * current Y cursor co-ordinate and with an initial false value for
	 * the TOSTOP-added flag.
	 */
	if (cursor->pvcount < 2) {
		cursor->y_start = pv_crs_get_ypos(terminalfd);
		cursor->shared->y_topmost = cursor->y_start;
		cursor->shared->tty_tostop_added = false;
		cursor->y_lastread = cursor->y_start;
		debug("%s", "we are the first to attach");
	}

	cursor->y_offset = cursor->pvcount - 1;
	if (cursor->y_offset < 0)
		cursor->y_offset = 0;

	/*
	 * If anyone else had attached to the shared memory segment, we need
	 * to read the top Y co-ordinate from it.
	 */
	if (cursor->pvcount > 1) {
		cursor->y_start = cursor->shared->y_topmost;
		cursor->y_lastread = cursor->y_start;
		debug("%s: %d", "not the first to attach - got top y", cursor->y_start);
	}

	pv_crs_unlock(cursor, terminalfd);

	return 0;
}
#endif				/* HAVE_IPC */


/*
 * Initialise the terminal for cursor positioning.
 */
void pv_crs_init(pvcursorstate_t cursor, readonly_pvcontrol_t control, pvtransientflags_t flags)
{
	char *ttyfile;
	int terminalfd;

	cursor->lock_fd = -2;
	cursor->lock_file[0] = '\0';

	if ((!control->cursor) || (cursor->disable))
		return;

	debug("%s", "init");

	ttyfile = ttyname(STDERR_FILENO);
	if (NULL == ttyfile) {
		debug("%s: %s", "disabling cursor positioning because ttyname failed", strerror(errno));
		cursor->disable = true;
		return;
	}

	terminalfd = open(ttyfile, O_RDWR); /* flawfinder: ignore */

	/*
	 * flawfinder rationale: the file we open won't be truncated but
	 * could be corrupted by writes attempting to get the current Y
	 * position; but we get the filename from ttyname() and it could be
	 * a symbolic link, so we can't do much more than trust it.
	 */

	if (terminalfd < 0) {
		pv_error("%s: %s: %s", _("failed to open terminal"), ttyfile, strerror(errno));
		cursor->disable = true;
		return;
	}
#ifdef HAVE_IPC
	if (pv_crs_ipcinit(cursor, control, ttyfile, terminalfd) != 0) {
		debug("%s", "ipcinit failed, setting noipc flag");
		cursor->noipc = true;
	}

	/*
	 * If we have already set the terminal TOSTOP attribute, set the
	 * flag in shared memory to let the other instances know.
	 */
	if ((!cursor->noipc) && (1 == flags->clear_tty_tostop_on_exit) && (NULL != cursor->shared)) {
		debug("%s", "propagating local clear_tty_tostop_on_exit true value to shared tty_tostop_added flag");
		cursor->shared->tty_tostop_added = true;
	}

	/*
	 * If we are not using IPC, then we need to get the current Y
	 * co-ordinate. If we are using IPC, then the pv_crs_ipcinit()
	 * function takes care of this in a more multi-process-friendly way.
	 */
	if (cursor->noipc) {
#else				/* ! HAVE_IPC */
	if (1) {
#endif				/* HAVE_IPC */
		/*
		 * Get current cursor position + 1.
		 */
		pv_crs_lock(cursor, control, terminalfd);
		cursor->y_start = pv_crs_get_ypos(terminalfd);
		/*
		 * Move down a line while the terminal is locked, so that
		 * other processes in the pipeline will get a different
		 * initial ypos.
		 */
		if (cursor->y_start > 0)
			pv_tty_write(flags, "\n", 1);
		pv_crs_unlock(cursor, terminalfd);

		if (cursor->y_start < 1)
			cursor->disable = true;
	}

	(void) close(terminalfd);
}


#ifdef HAVE_IPC
/*
 * Set the "we need to reinitialise cursor positioning" flag.
 */
void pv_crs_needreinit(pvcursorstate_t cursor)
{
	cursor->needreinit += 2;
	if (cursor->needreinit > 3)
		cursor->needreinit = 3;
}
#endif


#ifdef HAVE_IPC
/*
 * Reinitialise the cursor positioning code (called if we are backgrounded
 * then foregrounded again).
 */
static void pv_crs_reinit(pvcursorstate_t cursor, readonly_pvcontrol_t control, pvtransientflags_t flags)
{
	debug("%s", "reinit");

	if (1 == flags->suspend_stderr) {
		debug("%s", "reinit abandoned - stderr is suspended");
		return;
	}

	pv_crs_lock(cursor, control, STDERR_FILENO);

	cursor->needreinit--;
	if (cursor->y_offset < 1)
		cursor->needreinit = 0;

	if (cursor->needreinit > 0) {
		pv_crs_unlock(cursor, STDERR_FILENO);
		return;
	}

	debug("%s", "reinit full");

	cursor->y_start = pv_crs_get_ypos(STDERR_FILENO);

	if ((cursor->y_offset < 1) && (NULL != cursor->shared)) {
		cursor->shared->y_topmost = cursor->y_start;
	}
	cursor->y_lastread = cursor->y_start;

	pv_crs_unlock(cursor, STDERR_FILENO);
}
#endif


/*
 * Output a single-line update (\0-terminated), using the ECMA-48 CSI "CUP"
 * sequence to move the cursor to the correct position to do so.
 */
void pv_crs_update(pvcursorstate_t cursor, readonly_pvcontrol_t control, pvtransientflags_t flags,
		   const char *output_line)
{
	char cup_cmd[32];		 /* flawfinder: ignore */
	size_t cup_cmd_length, output_line_length;
	int y;

	/* Early return if cursor positioning was disabled. */
	if (cursor->disable)
		return;

	/*
	 * flawfinder rationale: the "cup_cmd" buffer is always zeroed
	 * before each use, and is only written to by pv_snprintf() bounded
	 * by its size; also, that function explictly zero-terminates the
	 * string it writes.  The write() call is also bounded.
	 */

	output_line_length = strlen(output_line);	/* flawfinder: ignore */
	/* flawfinder - output_line is explictly expected to be \0-terminated. */

#ifdef HAVE_IPC
	if (!cursor->noipc) {
		if (cursor->needreinit > 0)
			pv_crs_reinit(cursor, control, flags);

		pv_crs_ipccount(cursor);
		if (NULL != cursor->shared) {
			if (cursor->y_lastread != cursor->shared->y_topmost) {
				cursor->y_start = cursor->shared->y_topmost;
				cursor->y_lastread = cursor->y_start;
			}
		}

		if (cursor->needreinit > 0)
			return;
	}
#endif				/* HAVE_IPC */

	y = cursor->y_start;

#ifdef HAVE_IPC
	/*
	 * If the screen has scrolled, or is about to scroll, due to
	 * multiple `pv' instances taking us near the bottom of the screen,
	 * scroll the screen (only if we're the first `pv'), and then move
	 * our initial Y co-ordinate up.
	 */
	if (((cursor->y_start + cursor->pvmax) > (int) (control->height))
	    && (!cursor->noipc)
	    ) {
		int offs;

		offs = ((cursor->y_start + cursor->pvmax) - control->height);

		cursor->y_start -= offs;
		if (cursor->y_start < 1)
			cursor->y_start = 1;

		debug("%s: %d", "scroll offset", offs);

		/*
		 * Scroll the screen if we're the first `pv'.
		 */
		if (0 == cursor->y_offset) {
			pv_crs_lock(cursor, control, STDERR_FILENO);

			memset(cup_cmd, 0, sizeof(cup_cmd));
			(void) pv_snprintf(cup_cmd, sizeof(cup_cmd), "\033[%u;1H", control->height);
			cup_cmd_length = strlen(cup_cmd);	/* flawfinder: ignore */
			pv_tty_write(flags, cup_cmd, cup_cmd_length);
			for (; offs > 0; offs--) {
				pv_tty_write(flags, "\n", 1);
			}

			pv_crs_unlock(cursor, STDERR_FILENO);

			debug("%s", "we are the first - scrolled screen");
		}
	}

	if (!cursor->noipc)
		y = cursor->y_start + cursor->y_offset;
#endif				/* HAVE_IPC */

	/*
	 * Keep the Y co-ordinate within sensible bounds, so we can never
	 * overflow the "cup_cmd" buffer.
	 */
	if ((y < 1) || (y > 999999))
		y = 1;

	memset(cup_cmd, 0, sizeof(cup_cmd));
	(void) pv_snprintf(cup_cmd, sizeof(cup_cmd), "\033[%d;1H", y);
	cup_cmd_length = strlen(cup_cmd);   /* flawfinder: ignore */

	/*
	 * flawfinder rationale: "cup_cmd" is only written to by
	 * pv_snprintf(), which explicitly ensures that the string will be
	 * \0-terminated, and the buffer is zeroed beforehand so if
	 * pv_snprintf() fails to run the string will be of zero length.
	 */

	pv_crs_lock(cursor, control, STDERR_FILENO);

	pv_tty_write(flags, cup_cmd, cup_cmd_length);
	pv_tty_write(flags, output_line, output_line_length);

	pv_crs_unlock(cursor, STDERR_FILENO);
}


/*
 * Reposition the cursor to a final position.
 */
void pv_crs_fini(pvcursorstate_t cursor, readonly_pvcontrol_t control, pvtransientflags_t flags)
{
	char cup_cmd[32];		 /* flawfinder: ignore */
	unsigned int y;

	/* flawfinder - "cup_cmd" is zeroed, and only written by pv_snprintf() */

	debug("%s", "fini");

	y = (unsigned int) (cursor->y_start);

#ifdef HAVE_IPC
	if ((cursor->pvmax > 0) && (!cursor->noipc))
		y += cursor->pvmax - 1;
#endif				/* HAVE_IPC */

	if (y > control->height)
		y = control->height;

	/*
	 * Absolute bounds check.
	 */
	if ((y < 1) || (y > 999999))
		y = 1;

	memset(cup_cmd, 0, sizeof(cup_cmd));
	(void) pv_snprintf(cup_cmd, sizeof(cup_cmd), "\033[%u;1H\n", y);

	pv_crs_lock(cursor, control, STDERR_FILENO);

	if (!cursor->disable) {
		pv_tty_write(flags, cup_cmd, strlen(cup_cmd));	/* flawfinder: ignore */
	}
	/* flawfinder - pv_snprintf() always \0-terminates (see above). */

#ifdef HAVE_IPC
	/*
	 * If any other "pv -c" instances have set the terminal TOSTOP
	 * attribute, set our local flag so pv_sig_fini() will know about
	 * it.
	 */
	if ((!cursor->noipc) && (NULL != cursor->shared) && cursor->shared->tty_tostop_added) {
		if (0 == flags->clear_tty_tostop_on_exit) {
			debug("%s",
			      "propagating shared tty_tostop_added true value to local clear_tty_tostop_on_exit flag");
			flags->clear_tty_tostop_on_exit = 1;
		}
	}

	pv_crs_ipccount(cursor);
	if (NULL != cursor->shared) {
		(void) shmdt(cursor->shared);
	}
	cursor->shared = NULL;

	/*
	 * If we are the last instance detaching from the shared memory,
	 * delete it so it's not left lying around.
	 */
	if (cursor->pvcount < 2) {
		struct shmid_ds shm_buf;
		memset(&shm_buf, 0, sizeof(shm_buf));
		(void) shmctl(cursor->shmid, IPC_RMID, &shm_buf);
	}

#endif				/* HAVE_IPC */

	pv_crs_unlock(cursor, STDERR_FILENO);

	if (cursor->lock_fd >= 0) {
		(void) close(cursor->lock_fd);
		/*
		 * We can get away with removing this on exit because all
		 * the other PVs will be finishing at the same sort of time.
		 */
		(void) remove(cursor->lock_file);
	}
}
