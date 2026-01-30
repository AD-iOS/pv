/*
 * Output debugging information.
 *
 * Copyright 2002-2008, 2010, 2012-2015, 2017, 2021, 2023-2025 Andrew Wood
 *
 * License GPLv3+: GNU GPL version 3 or later; see `docs/COPYING'.
 */

#include "config.h"
#include "pv.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>


#ifdef ENABLE_DEBUGGING
/*@null@*/ static const char *debug_filename = NULL;

/*
 * Set the destination for debugging information.
 */
void debugging_output_destination(const char *filename)
{
	debug_filename = filename;
}


/*
 * Output debugging information to the file specified earlier by a call to
 * debugging_output_destination(), if any.
 */
void debugging_output(const char *function, const char *file, int line, const char *format, ...)
{
	static bool tried_open = false;
	static FILE *debugfptr = NULL;
	va_list ap;
	time_t t;
	struct tm *tm;
	char tbuf[128];			 /* flawfinder: ignore */

	/*
	 * flawfinder note: tbuf is only written to by strftime() which
	 * takes its size, and we enforce string termination.
	 */

	if (false == tried_open) {
		if (NULL != debug_filename) {
			debugfptr = fopen(debug_filename, "a");	/* flawfinder: ignore */
			/*
			 * flawfinder note: caller directly controls
			 * filename, the safest we can manage is to use
			 * append mode.
			 */
		}
		tried_open = true;
	}

	if (NULL == debugfptr) {
		return;
	}

	/*
	 * Note that here we use gmtime() rather than localtime(), otherwise
	 * we can get stuck in signal handlers - testing with "strace"
	 * showed many cases where "pv </dev/zero | cat >/dev/null" being
	 * paused and backgrounded would cause pv to be stuck in
	 * futex_wait() inside a pv_sig_alrm() inside a pv_sig_cont().  The
	 * backtrace mentioned many time zone conversion steps, and all of
	 * that goes away with gmtime().
	 */

	(void) time(&t);
	tm = gmtime(&t);
	tbuf[0] = '\0';
	if (0 == strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm)) {
		tbuf[0] = '\0';
	}
	tbuf[sizeof(tbuf) - 1] = '\0';	    /* enforce termination */

	(void) fprintf(debugfptr, "[%s] (%d) %s (%s:%d): ", tbuf, getpid(), function, file, line);

	va_start(ap, format);
	(void) vfprintf(debugfptr, format, ap);	/* flawfinder: ignore */
	va_end(ap);

	/*
	 * flawfinder note: vfprintf format is explicitly controlled by the
	 * caller of this function - no mitigation possible or desirable.
	 */

	(void) fprintf(debugfptr, "\n");
	(void) fflush(debugfptr);
}

#else				/* ! ENABLE_DEBUGGING */

/*
 * Stub debugging destination function.
 */
void debugging_output_destination( /*@unused@ */  __attribute__((unused))
				  const char *filename)
{
}

/*
 * Stub debugging output function.
 */
void debugging_output( /*@unused@ */  __attribute__((unused))
		      const char *function, /*@unused@ */  __attribute__((unused))
		      const char *file, /*@unused@ */  __attribute__((unused))
		      int line, /*@unused@ */  __attribute__((unused))
		      const char *format, ...)
{
}

#endif				/* ENABLE_DEBUGGING */
