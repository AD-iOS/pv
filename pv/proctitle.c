/*
 * Functions for setting the process title.
 *
 * Copyright 2024-2025 Andrew Wood
 *
 * License GPLv3+: GNU GPL version 3 or later; see `docs/COPYING'.
 */

#include "config.h"
#include "pv.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#if ! HAVE_SETPROCTITLE
extern char **environ;

/*@null@*/ static char **base_argv = NULL;
static size_t space_available = 0;


/*
 * Assuming the environment comes after the command line arguments, we make
 * a duplicate of the environment array and all its values, so we can use
 * the space the environment used to occupy for the process title.
 *
 * The logic for this was derived from util-linux-ng.
 */
/*@-mustfreefresh@ */
/*@-nullstate@ */
void initproctitle(int argc, char **argv)
{
	char **original_environment = environ;
	char **new_environment;
	size_t env_array_size, env_index;

	/* Find the number of entries in the environment array. */
	for (env_array_size = 0; environ[env_array_size] != NULL; env_array_size++)
		continue;

	/* Allocate a new environment array. */
	new_environment = (char **) malloc(sizeof(char *) * (env_array_size + 1));
	if (NULL == new_environment)
		return;

	/* Duplicate the strings in the environment. */
	for (env_index = 0; original_environment[env_index] != NULL; env_index++) {
		new_environment[env_index] = pv_strdup(original_environment[env_index]);
		if (NULL == new_environment[env_index])
			return;
	}
	new_environment[env_index] = NULL;

	base_argv = argv;

	/* Work out how much room we have. */
	if (env_index > 0) {
		/* From argv[0] to the end of the last environment value. */
		space_available = (size_t)
		    (original_environment[env_index - 1] + strlen(original_environment[env_index - 1]) - argv[0]);	/* flawfinder: ignore */
	} else if (argc > 0) {
		/* No environment; from argv[0] to the end of the last argument. */
		space_available = (size_t) (argv[argc - 1] + strlen(argv[argc - 1]) - argv[0]);	/* flawfinder: ignore */
	}

	/*
	 * flawfinder - we have to trust that the environment and argument
	 * strings are null-terminated, since that's what the OS is supposed
	 * to guarantee.
	 */

	environ = new_environment;
}

/*@+nullstate@ */
/*@+mustfreefresh@ */


/*
 * Set the process title visible to ps(1), by overwriting argv[0] with the
 * given printf() format string.
 */
void setproctitle(const char *format, ...)
{
	char title[1024];		 /* flawfinder: ignore */
	size_t length;
	va_list ap;

	/* flawfinder - buffer is zeroed and users of it are bounded. */

	if (NULL == base_argv)
		return;

	memset(title, 0, sizeof(title));

	va_start(ap, format);
	(void) vsnprintf(title, sizeof(title) - 1, format, ap);	/* flawfinder: ignore */
	va_end(ap);

	/*
	 * flawfinder - the format is explicitly caller-supplied.  Callers
	 * of this function are all passing fixed format strings and not
	 * user-supplied formats.
	 */

	length = strlen(title);		    /* flawfinder: ignore */
	/* flawfinder - we left a byte at the end to force null-termination. */

	if (length > space_available - 2)
		length = space_available - 2;

	(void) pv_snprintf(base_argv[0], space_available, "%s", title);
	if (space_available > length)
		memset(base_argv[0] + length, 0, space_available - length);
}

#endif
