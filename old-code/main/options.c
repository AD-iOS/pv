/*
 * Parse command-line options.
 *
 * Copyright 2002-2008, 2010, 2012-2015, 2017, 2021, 2023-2025 Andrew Wood
 *
 * License GPLv3+: GNU GPL version 3 or later; see `docs/COPYING'.
 */

#include "config.h"
#include "options.h"
#include "pv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_SYSMACROS_H
#include <sys/sysmacros.h>
#ifdef major
#ifdef minor
#ifdef HAVE_STRUCT_STAT_ST_RDEV
#define CAN_BUILD_SYSFS_FILENAME 1
#endif
#endif
#endif
#endif
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif


void display_help(void);
void display_version(void);
static bool opts_watchfd_parse(opts_t, const char *, /*@null@ */ const char *, unsigned int);


/*
 * splint note about mustfreefresh: the gettext calls made by _() cause
 * memory leak warnings, but in these cases it's unavoidable, and mitigated
 * by the fact we only translate each string once.
 */


/*
 * Free an opts_t object.
 */
void opts_free( /*@only@ */ opts_t opts)
{
	if (!opts)
		return;
	/*@-keeptrans@ */
	/*
	 * splint note: we're explicitly being handed the "opts" object to
	 * free it, so the previously "kept" internally allocated buffers
	 * are now ours to free.
	 */
	if (NULL != opts->name)
		free(opts->name);
	if (NULL != opts->format)
		free(opts->format);
	if (NULL != opts->pidfile)
		free(opts->pidfile);
	if (NULL != opts->output)
		free(opts->output);
	if (NULL != opts->default_bar_style)
		free(opts->default_bar_style);
	if (NULL != opts->store_and_forward_file)
		free(opts->store_and_forward_file);
	if (NULL != opts->extra_display)
		free(opts->extra_display);
	if (NULL != opts->watchfd_pid)
		free(opts->watchfd_pid);
	if (NULL != opts->watchfd_fd)
		free(opts->watchfd_fd);
	if (NULL != opts->argv)
		free(opts->argv);
	/*@+keeptrans@ */
	free(opts);
}

/*
 * Add a filename to the list of non-option arguments, returning false on
 * error.  The filename is not copied - the pointer is stored.
 */
bool opts_add_file(opts_t opts, const char *filename)
{
	/*@-branchstate@ */
	if ((opts->argc >= opts->argv_length) || (NULL == opts->argv)) {
		opts->argv_length = opts->argc + 10;
		/*@-keeptrans@ */
		opts->argv = realloc(opts->argv, opts->argv_length * sizeof(char *));
		/*@+keeptrans@ */
		if (NULL == opts->argv) {
			fprintf(stderr, "%s: %s\n", opts->program_name, strerror(errno));
			opts->argv_length = 0;
			opts->argc = 0;
			return false;
		}
	}
	/*@+branchstate@ */

	/*
	 * splint notes: we turned off "branchstate" above because depending
	 * on whether we have to extend the array, we change argv from
	 * "keep" to "only", which is also why we turned off "keeptrans";
	 * there doesn't seem to be a clean way to tell splint that everyone
	 * else should not touch argv but we're allowed to reallocate it and
	 * so is opts_parse.
	 */

	opts->argv[opts->argc++] = filename;

	return true;
}

/*
 * Add a process ID and file descriptor to the list of items to watch with
 * --watchfd, returning false on error.
 */
static bool opts_watchfd_add_item(opts_t opts, pid_t pid, int fd)
{
	/*@-branchstate@ */
	if ((opts->watchfd_count >= opts->watchfd_length) || (NULL == opts->watchfd_pid)) {
		opts->watchfd_length = opts->watchfd_count + 10;
		/*@-keeptrans@ */
		opts->watchfd_pid = realloc(opts->watchfd_pid, opts->watchfd_length * sizeof(pid_t));
		opts->watchfd_fd = realloc(opts->watchfd_fd, opts->watchfd_length * sizeof(int));
		/*@+keeptrans@ */
		if ((NULL == opts->watchfd_pid) || (NULL == opts->watchfd_fd)) {
			fprintf(stderr, "%s: %s\n", opts->program_name, strerror(errno));
			opts->watchfd_length = 0;
			opts->watchfd_count = 0;
			return false;
		}
	}
	/*@+branchstate@ */

	/*
	 * splint notes: we turned off "branchstate" and "keeptrans" above
	 * because of the same reason as in opts_add_file().
	 */

	opts->watchfd_pid[opts->watchfd_count] = pid;
	opts->watchfd_fd[opts->watchfd_count] = fd;
	opts->watchfd_count++;

	return true;
}


/*
 * Look for processes with the given name, and add all of them to the list
 * of watched PIDs.  Returns false on error.
 */
static bool opts_watchfd_processname(opts_t opts, const char *process_name)
{
	int fds[2];
	pid_t pid;
	FILE *fptr;
	char buffer[1024];		 /* flawfinder: ignore */
	pid_t waited_pid;
	int pid_status;
	bool ok;

	/*
	 * flawfinder: buffer is zeroed before each use, and fgets() is
	 * passed one less than its size so the string functions in the loop
	 * always find a null byte at the end.
	 */

	/* Pipe for communicating with pgrep. */
	if (pipe(fds) < 0) {
		fprintf(stderr, "%s: %s\n", opts->program_name, strerror(errno));
		return false;
	}

	/* Subprocess in which to run pgrep. */
	pid = (pid_t) fork();
	if (pid < 0) {
		fprintf(stderr, "%s: %s\n", opts->program_name, strerror(errno));
		(void) close(fds[0]);
		(void) close(fds[1]);
		return false;
	} else if (0 == pid) {
		int nullfd;

		/* Child process - close stdin, set up stdout, run pgrep. */

		/* Replace stdin with /dev/null. */
		nullfd = open("/dev/null", O_RDONLY);	/* flawfinder: ignore */
		/* flawfinder: /dev/null is trusted. */
		if (nullfd < 0) {
			fprintf(stderr, "%s: %s: %s\n", opts->program_name, "/dev/null", strerror(errno));
			exit(EXIT_FAILURE);
		}
		if (dup2(nullfd, STDIN_FILENO) < 0) {
			perror("dup2");
			exit(EXIT_FAILURE);
		}
		(void) close(nullfd);

		/* Replace stdout with the write end of the pipe. */
		if (dup2(fds[1], STDOUT_FILENO) < 0) {
			perror("dup2");
			exit(EXIT_FAILURE);
		}
		(void) close(fds[1]);

		/* Close the read end of the pipe. */
		(void) close(fds[0]);

		/* Run pgrep. */
		if (execlp("pgrep", "pgrep", process_name, NULL) < 0) {	/* flawfinder: ignore */
			perror("pgrep");
		}
		/*
		 * flawfinder: deliberately calling pgrep as there isn't a
		 * portable library to use instead.
		 */
		exit(EXIT_FAILURE);
	}

	/* Close the write end of the pipe. */
	(void) close(fds[1]);

	/* Open a file stream on the read end of the pipe. */
	fptr = fdopen(fds[0], "r");
	if (NULL == fptr) {
		perror("fdopen");
		(void) close(fds[0]);
		return false;
	}

	/* Read the lines of output from the child process. */

	ok = true;
	memset(buffer, 0, sizeof(buffer));
	while ((0 == feof(fptr)) && (NULL != fgets(buffer, (int) (sizeof(buffer) - 1), fptr))) {
		unsigned int watch_pid;

		/* Skip all lines if we've hit any errors. */
		if (!ok) {
			memset(buffer, 0, sizeof(buffer));
			continue;
		}

		/* Skip lines without valid PID. */
		watch_pid = 0;
		if (sscanf(buffer, "%u", &watch_pid) < 1)
			continue;
		if (watch_pid < 1)
			continue;

		if (!opts_watchfd_parse(opts, buffer, NULL, 0))
			ok = false;

		memset(buffer, 0, sizeof(buffer));
	}


	/* Close the stream from the read end of the pipe. */
	(void) fclose(fptr);

	/* Wait for the child process to exit. */
	do {
		/*@-type@ */
		/* splint disagreement about __pid_t vs pid_t. */
		pid_status = 0;
		waited_pid = waitpid(pid, &pid_status, 0);
		/*@+type@ */
	} while (-1 == waited_pid && errno == EINTR);

	return ok;
}


/*
 * Read additional "-d" values from the given file, returning false on
 * failure.
 */
static bool opts_watchfd_listfile(opts_t opts, const char *filename)
{
	FILE *fptr;
	char buffer[1024];		 /* flawfinder: ignore */
	unsigned int linenumber;

	/*
	 * flawfinder: buffer is zeroed before each use, and fgets() is
	 * passed one less than its size so the string functions in the loop
	 * always find a null byte at the end.
	 */

	fptr = fopen(filename, "r");	    /* flawfinder: ignore */
	/*
	 * flawfinder note: caller directly controls filename, and we're
	 * opening the file read-only.
	 */
	if (NULL == fptr) {
		fprintf(stderr, "%s: -d @: %s: %s\n", opts->program_name, filename, strerror(errno));
		return false;
	}

	linenumber = 0;
	memset(buffer, 0, sizeof(buffer));

	while ((0 == feof(fptr)) && (NULL != fgets(buffer, (int) (sizeof(buffer) - 1), fptr))) {
		char *argument;
		char *nlptr;

		linenumber++;

		/* Remove trailing carriage returns or newlines. */
		nlptr = strchr(buffer, '\r');
		if (NULL != nlptr)
			nlptr[0] = '\0';
		nlptr = strchr(buffer, '\n');
		if (NULL != nlptr)
			nlptr[0] = '\0';

		/* Ignore leading spaces. */
		argument = buffer;
		while (argument[0] != '\0' && (argument[0] == ' ' || argument[0] == '\t'))
			argument++;

		/* Ignore blank lines or comment lines. */
		if ((argument[0] == '\0') || (argument[0] == '#')) {
			memset(buffer, 0, sizeof(buffer));
			continue;
		}

		/* Reject "@" lines. */
		if (argument[0] == '@') {
			(void) fclose(fptr);
			return false;
		}

		if (!opts_watchfd_parse(opts, argument, filename, linenumber)) {
			(void) fclose(fptr);
			return false;
		}

		memset(buffer, 0, sizeof(buffer));
	}

	(void) fclose(fptr);
	return true;
}


/*
 * Parse a "-d" option value and make the appropriate additions to the list
 * of watchfd items, returning false on failure.
 */
static bool opts_watchfd_parse(opts_t opts, const char *argument, /*@null@ */ const char *filename, unsigned int line)
{
	unsigned int parse_pid;
	int parse_fd;

	if ('@' == argument[0]) {
		/* "-d @FILE" syntax - read more values from FILE. */

		/* Don't allow this syntax in a list file. */
		if (NULL != filename) {
			/*@-mustfreefresh@ *//* see above */
			fprintf(stderr, "%s: -d @: %s:%u: %s\n",
				opts->program_name, filename, line, _("list files may not contain @ lines"));
			return false;
			/*@+mustfreefresh@ */
		}

		return opts_watchfd_listfile(opts, argument + 1);

	} else if ('=' == argument[0]) {
		/* "-d =NAME" syntax - find processes named NAME. */
		return opts_watchfd_processname(opts, argument + 1);
	}

	/* "-d PID[:FD]" syntax. */

	parse_pid = 0;
	parse_fd = -1;

	if (sscanf(argument, "%u:%d", &parse_pid, &parse_fd) < 1) {
		/*@-mustfreefresh@ *//* see above */
		if (NULL != filename) {
			fprintf(stderr, "%s: -d: %s:%u: %s\n",
				opts->program_name, filename, line, _("process ID or pid:fd pair expected"));
		} else {
			fprintf(stderr, "%s: -d: %s\n", opts->program_name, _("process ID or pid:fd pair expected"));
		}
		return false;
		/*@+mustfreefresh@ */
	}

	if (parse_pid < 1) {
		/*@-mustfreefresh@ *//* see above */
		if (NULL != filename) {
			fprintf(stderr, "%s: -d: %s:%u: %u: %s\n",
				opts->program_name, filename, line, parse_pid, _("invalid process ID"));
		} else {
			fprintf(stderr, "%s: -d: %u: %s\n", opts->program_name, parse_pid, _("invalid process ID"));
		}
		return false;
		/*@+mustfreefresh@ */
	}

	return opts_watchfd_add_item(opts, (pid_t) parse_pid, parse_fd);
}


/*
 * Set opts->size from the size of the file whose name is size_file,
 * returning false (and reporting the error) if there is a problem.
 *
 * If size_file points to a block device, the size of the block device is
 * used.
 */
static bool opts_use_size_of_file(opts_t opts, const char *size_file)
{
	struct stat sb;
	int stat_rc;
#ifdef CAN_BUILD_SYSFS_FILENAME
	char sysfs_filename[512];	 /* flawfinder: ignore */
	FILE *sysfs_fptr;
	long long sysfs_size;
#endif				/* CAN_BUILD_SYSFS_FILENAME */
	int device_fd;
	off_t device_size;

	/*
	 * flawfinder rationale: sysfs_filename is only used with
	 * pv_snprintf() which guarantees it will be bounded and zero
	 * terminated.
	 */

	stat_rc = 0;
	memset(&sb, 0, sizeof(sb));

	stat_rc = stat(size_file, &sb);

	if (0 != stat_rc) {
		/*@-mustfreefresh@ *//* see above */
		fprintf(stderr, "%s: %s: %s: %s\n",
			opts->program_name, size_file, _("failed to stat file"), strerror(errno));
		return false;
		/*@+mustfreefresh@ */
	}

	/* This was a regular file - use its size and return. */
	if (S_ISREG((mode_t) (sb.st_mode))) {
		opts->size = (off_t) (sb.st_size);
		return true;
	}

	/* This was a directory - report an error. */
	if (S_ISDIR((mode_t) (sb.st_mode))) {
		/*@-mustfreefresh@ *//* see above */
		fprintf(stderr, "%s: %s: %s\n", opts->program_name, size_file, _("is a directory"));
		return false;
		/*@+mustfreefresh@ */
	}

	/* This was not a block device - just use the size and return. */
	if (!S_ISBLK((mode_t) (sb.st_mode))) {
		opts->size = (off_t) (sb.st_size);
		return true;
	}

	/*
	 * Block device - determine its size by looking for
	 * /sys/dev/block/MAJOR:MINOR/size, and if that fails, try opening
	 * the file and seeking to the end.
	 */

#ifdef CAN_BUILD_SYSFS_FILENAME
	/*
	 * Try the sysfs method - lightest touch and requires no read access
	 * to the actual block device.
	 */

	memset(sysfs_filename, 0, sizeof(sysfs_filename));
	if (pv_snprintf
	    (sysfs_filename, sizeof(sysfs_filename), "/sys/dev/block/%u:%u/size", major(sb.st_rdev),
	     minor(sb.st_rdev)) < 0) {
		/*@-mustfreefresh@ *//* see above */
		fprintf(stderr, "%s: %s: %s: %s\n",
			opts->program_name, size_file, _("failed to generate sysfs filename"), strerror(errno));
		return false;
		/*@+mustfreefresh@ */
	}

	sysfs_fptr = fopen(sysfs_filename, "r");	/* flawfinder: ignore */
	/*
	 * flawfinder rationale: sysfs is trusted here, the filename is
	 * predictable, and we are restricted to reading one number.
	 */
	if (NULL != sysfs_fptr) {
		sysfs_size = -1;
		if (1 == fscanf(sysfs_fptr, "%lld", &sysfs_size)) {
			/* Read successful - use the value (* 512) and return. */
			(void) fclose(sysfs_fptr);
			opts->size = (off_t) sysfs_size *512;
			return true;
		}
		/* Read not successful - report the error and return. */
		/* NB we must fclose() after reporting to retain errno. */
		/*@-mustfreefresh@ *//* see above */
		fprintf(stderr, "%s: %s: %s: %s\n",
			opts->program_name, size_file, _("failed to read sysfs size file"), strerror(errno));
		(void) fclose(sysfs_fptr);
		return false;
		/*@+mustfreefresh@ */
	}
#endif				/* CAN_BUILD_SYSFS_FILENAME */

	/*
	 * Try opening the block device and seeking to the end.
	 */
	device_fd = open(size_file, O_RDONLY);	/* flawfinder: ignore */
	/*
	 * flawfinder rationale: the filename is under the direct control of
	 * the operator by its nature, so we can't refuse to open symlinks
	 * etc as that would be counterintuitive.
	 */

	if (device_fd < 0) {
		/*@-mustfreefresh@ *//* see above */
		fprintf(stderr, "%s: %s: %s: %s\n",
			opts->program_name, size_file, _("failed to open block device"), strerror(errno));
		return false;
		/*@+mustfreefresh@ */
	}

	device_size = (off_t) lseek(device_fd, 0, SEEK_END);

	if (device_size < 0) {
		/*@-mustfreefresh@ *//* see above */
		fprintf(stderr, "%s: %s: %s: %s\n",
			opts->program_name, size_file, _("failed to determine size of block device"), strerror(errno));
		/* NB close() after reporting error, to preserve errno. */
		(void) close(device_fd);
		return false;
		/*@+mustfreefresh@ */
	}

	(void) close(device_fd);

	/* Use the size we found. */
	opts->size = device_size;
	return true;
}


/*
 * Parse the given command-line arguments into an opts_t object, handling
 * "help" and "version" options internally.
 *
 * Returns an opts_t, or NULL on error.
 *
 * Note that the contents of *argv[] (i.e. the command line parameters)
 * aren't copied anywhere, just the pointers are copied, so make sure the
 * command line data isn't overwritten or argv[1] free()d or whatever.
 */
/*@null@ */
/*@only@ */
opts_t opts_parse(unsigned int argc, char **argv)
{
#ifdef HAVE_GETOPT_LONG
	/*@-nullassign@ */
	/* splint rationale: NULL is allowed for "flags" in long options. */
	struct option long_options[] = {
		{ "help", 0, NULL, (int) 'h' },
		{ "version", 0, NULL, (int) 'V' },
		{ "progress", 0, NULL, (int) 'p' },
		{ "timer", 0, NULL, (int) 't' },
		{ "eta", 0, NULL, (int) 'e' },
		{ "fineta", 0, NULL, (int) 'I' },
		{ "rate", 0, NULL, (int) 'r' },
		{ "average-rate", 0, NULL, (int) 'a' },
		{ "bytes", 0, NULL, (int) 'b' },
		{ "bits", 0, NULL, (int) '8' },
		{ "si", 0, NULL, (int) 'k' },
		{ "buffer-percent", 0, NULL, (int) 'T' },
		{ "last-written", 1, NULL, (int) 'A' },
		{ "force", 0, NULL, (int) 'f' },
		{ "numeric", 0, NULL, (int) 'n' },
		{ "quiet", 0, NULL, (int) 'q' },
		{ "cursor", 0, NULL, (int) 'c' },
		{ "wait", 0, NULL, (int) 'W' },
		{ "delay-start", 1, NULL, (int) 'D' },
		{ "size", 1, NULL, (int) 's' },
		{ "gauge", 0, NULL, (int) 'g' },
		{ "line-mode", 0, NULL, (int) 'l' },
		{ "null", 0, NULL, (int) '0' },
		{ "interval", 1, NULL, (int) 'i' },
		{ "width", 1, NULL, (int) 'w' },
		{ "height", 1, NULL, (int) 'H' },
		{ "name", 1, NULL, (int) 'N' },
		{ "bar-style", 1, NULL, (int) 'u' },
		{ "format", 1, NULL, (int) 'F' },
		{ "extra-display", 1, NULL, (int) 'x' },
		{ "stats", 0, NULL, (int) 'v' },
		{ "rate-limit", 1, NULL, (int) 'L' },
		{ "buffer-size", 1, NULL, (int) 'B' },
		{ "no-splice", 0, NULL, (int) 'C' },
		{ "skip-errors", 0, NULL, (int) 'E' },
		{ "error-skip-block", 1, NULL, (int) 'Z' },
		{ "stop-at-size", 0, NULL, (int) 'S' },
		{ "sync", 0, NULL, (int) 'Y' },
		{ "direct-io", 0, NULL, (int) 'K' },
		{ "sparse", 0, NULL, (int) 'O' },
		{ "sparse-output", 0, NULL, (int) 'O' },
		{ "discard", 0, NULL, (int) 'X' },
		{ "store-and-forward", 1, NULL, (int) 'U' },
		{ "remote", 1, NULL, (int) 'R' },
		{ "query", 1, NULL, (int) 'Q' },
		{ "pidfile", 1, NULL, (int) 'P' },
		{ "watchfd", 1, NULL, (int) 'd' },
		{ "output", 1, NULL, (int) 'o' },
		{ "average-rate-window", 1, NULL, (int) 'm' },
#ifdef ENABLE_DEBUGGING
		{ "debug", 1, NULL, (int) '!' },
#endif				/* ENABLE_DEBUGGING */
		{ NULL, 0, NULL, 0 }
	};
	/*@+nullassign@ */
	int option_index = 0;
#endif				/* HAVE_GETOPT_LONG */
	char *short_options = "hVpteIrab8kTA:fvnqcWD:s:gl0i:w:H:N:u:F:x:L:B:CEZ:SYKOXU:R:Q:P:d:m:o:"
#ifdef ENABLE_DEBUGGING
	    "!:"
#endif
	    ;
	int c, numopts;
	unsigned int check_pid;
	int check_fd;
	opts_t opts;
	char *leafptr;

	opts = calloc(1, sizeof(*opts));
	if (!opts) {
		/*@-mustfreefresh@ */
		/*
		 * splint note: the gettext calls made by _() cause memory
		 * leak warnings, but in this case it's unavoidable, and
		 * mitigated by the fact we only translate each string once.
		 */
		fprintf(stderr, "%s: %s: %s\n", argv[0], _("option structure allocation failed"), strerror(errno));
		return NULL;
		/*@+mustfreefresh@ */
	}

	leafptr = strrchr(argv[0], '/');
	if (NULL != leafptr) {
		opts->program_name = 1 + leafptr;
	} else {
		leafptr = argv[0];	    /* avoid splint "keep" warnings */
		opts->program_name = leafptr;
	}

	opts->argc = 0;
	opts->argv = calloc((size_t) (argc + 1), sizeof(char *));
	if (NULL == opts->argv) {
		/*@-mustfreefresh@ */
		/* splint note: as above. */
		fprintf(stderr, "%s: %s: %s\n", opts->program_name,
			_("option structure argv allocation failed"), strerror(errno));
		free(opts);		    /* can't call opts_free as argv is not set */
		return NULL;
		/*@+mustfreefresh@ */
	}
	opts->argv_length = 1 + argc;

	numopts = 0;

	opts->action = PV_ACTION_TRANSFER;
	opts->interval = 1;
	opts->delay_start = 0;
	opts->average_rate_window = 30;

	opts->width_set_manually = false;
	opts->height_set_manually = false;

	do {
#ifdef HAVE_GETOPT_LONG
		c = getopt_long((int) argc, argv, short_options, long_options, &option_index);	/* flawfinder: ignore */
#else
		c = getopt((int) argc, argv, short_options);	/* flawfinder: ignore */
#endif
		/*
		 * flawfinder rationale: we have to pass argv to getopt, and
		 * limiting the argument sizes would be impractical and
		 * cumbersome (and likely lead to more bugs); so we have to
		 * trust the system getopt to not have internal buffer
		 * overflows.
		 */

		if (c < 0)
			continue;

		/*
		 * Check that any numeric arguments are of the right type.
		 */
		switch (c) {
		case 's':
			/* "-s @" is valid, so allow it. */
			if ('@' == *optarg)
				break;
			/* falls through */
			/*@fallthrough@ */
		case 'L':
			/*@fallthrough@ */
		case 'B':
			/*@fallthrough@ */
		case 'Z':
			if (!pv_getnum_check(optarg, PV_NUMTYPE_ANY_WITH_SUFFIX)) {
				/*@-mustfreefresh@ *//* see above */
				fprintf(stderr, "%s: -%c: %s: %s\n", opts->program_name, c, optarg,
					_("numeric value not understood"));
				opts_free(opts);
				return NULL;
				/*@+mustfreefresh@ */
			}
			break;
		case 'A':
			/*@fallthrough@ */
		case 'w':
			/*@fallthrough@ */
		case 'H':
			/*@fallthrough@ */
		case 'R':
			/*@fallthrough@ */
		case 'Q':
			/*@fallthrough@ */
		case 'm':
			if (!pv_getnum_check(optarg, PV_NUMTYPE_BARE_INTEGER)) {
				/*@-mustfreefresh@ *//* see above */
				fprintf(stderr, "%s: -%c: %s: %s\n", opts->program_name, c, optarg,
					_("integer argument expected"));
				opts_free(opts);
				return NULL;
				/*@+mustfreefresh@ */
			}
			break;
		case 'i':
			/*@fallthrough@ */
		case 'D':
			if (!pv_getnum_check(optarg, PV_NUMTYPE_BARE_DOUBLE)) {
				/*@-mustfreefresh@ *//* see above */
				fprintf(stderr, "%s: -%c: %s: %s\n", opts->program_name, c, optarg,
					_("numeric argument expected"));
				opts_free(opts);
				return NULL;
				/*@+mustfreefresh@ */
			}
			break;
		case 'd':
			if ('@' == optarg[0]) {
				/* "-d @FILE" syntax - check FILE exists. */
				if (optarg[1] == '\0') {
					/*@-mustfreefresh@ *//* see above */
					fprintf(stderr, "%s: -%c @: %s\n",
						opts->program_name, c, _("missing filename"));
					opts_free(opts);
					return NULL;
					/*@+mustfreefresh@ */
				} else if (0 != access(optarg + 1, R_OK)) {	/* flawfinder: ignore */
					/*
					 * flawfinder rationale: we're not
					 * using access() to check
					 * permissions, only to help give a
					 * usable error message, so there's
					 * no TOCTOU issue.
					 */
					/*@-mustfreefresh@ *//* see above */
					fprintf(stderr, "%s: -%c @: %s: %s\n",
						opts->program_name, c, optarg + 1, strerror(errno));
					opts_free(opts);
					return NULL;
					/*@+mustfreefresh@ */
				}
				break;
			} else if ('=' == optarg[0]) {
				/*
				 * "-d =NAME" syntax - check NAME is not
				 * blank.
				 */
				if (optarg[1] == '\0') {
					/*@-mustfreefresh@ *//* see above */
					fprintf(stderr, "%s: -%c %c: %s\n",
						opts->program_name, c, optarg[0], _("missing process name"));
					opts_free(opts);
					return NULL;
					/*@+mustfreefresh@ */
				}
				break;
			}
			if (sscanf(optarg, "%u:%d", &check_pid, &check_fd)
			    < 1) {
				/* "-d PID[:FD]" syntax - check numbers. */
				/*@-mustfreefresh@ *//* see above */
				fprintf(stderr, "%s: -%c: %s\n",
					opts->program_name, c, _("process ID or pid:fd pair expected"));
				opts_free(opts);
				return NULL;
				/*@+mustfreefresh@ */
			}
			if (check_pid < 1) {
				/*@-mustfreefresh@ *//* see above */
				fprintf(stderr, "%s: -%c: %s\n", opts->program_name, c, _("invalid process ID"));
				opts_free(opts);
				return NULL;
				/*@+mustfreefresh@ */
			}
			break;
		default:
			break;
		}

		/*
		 * Parse each command line option.
		 */
		switch (c) {
		case 'h':
			display_help();
			opts->action = PV_ACTION_NOTHING;
			return opts;	    /* early return */
		case 'V':
			display_version();
			opts->action = PV_ACTION_NOTHING;
			return opts;	    /* early return */
		case 'p':
			opts->progress = true;
			numopts++;
			break;
		case 't':
			opts->timer = true;
			numopts++;
			break;
		case 'I':
			opts->fineta = true;
			numopts++;
			break;
		case 'e':
			opts->eta = true;
			numopts++;
			break;
		case 'r':
			opts->rate = true;
			numopts++;
			break;
		case 'a':
			opts->average_rate = true;
			numopts++;
			break;
		case 'b':
			opts->bytes = true;
			numopts++;
			break;
		case '8':
			opts->bytes = true;
			opts->bits = true;
			numopts++;
			break;
		case 'k':
			opts->decimal_units = true;
			break;
		case 'T':
			opts->bufpercent = true;
			numopts++;
			opts->no_splice = true;
			break;
		case 'A':
			opts->lastwritten = (size_t) pv_getnum_count(optarg, opts->decimal_units);
			numopts++;
			opts->no_splice = true;
			break;
		case 'f':
			opts->force = true;
			break;
		case 'v':
			opts->show_stats = true;
			break;
		case 'n':
			opts->numeric = true;
			numopts++;
			break;
		case 'q':
			opts->no_display = true;
			numopts++;
			break;
		case 'c':
			opts->cursor = true;
			break;
		case 'W':
			opts->wait = true;
			break;
		case 'D':
			opts->delay_start = pv_getnum_interval(optarg);
			break;
		case 's':
			if ('@' != *optarg) {
				/* A number was passed, not "@<filename>". */
				opts->size = pv_getnum_size(optarg, opts->decimal_units);
			} else {
				/* Permit "@<filename>". */
				const char *size_file = 1 + optarg;
				if (!opts_use_size_of_file(opts, size_file)) {
					opts_free(opts);
					return NULL;
				}
			}
			break;
		case 'g':
			opts->rate_gauge = true;
			break;
		case 'l':
			opts->linemode = true;
			break;
		case '0':
			opts->null_terminated_lines = true;
			opts->linemode = true;
			break;
		case 'i':
			opts->interval = pv_getnum_interval(optarg);
			break;
		case 'w':
			opts->width = pv_getnum_count(optarg, opts->decimal_units);
			opts->width_set_manually = opts->width == 0 ? false : true;
			break;
		case 'H':
			opts->height = pv_getnum_count(optarg, opts->decimal_units);
			opts->height_set_manually = opts->height == 0 ? false : true;
			break;
		case 'N':
			opts->name = pv_strdup(optarg);
			if (NULL == opts->name) {
				fprintf(stderr, "%s: -N: %s\n", opts->program_name, strerror(errno));
				opts_free(opts);
				return NULL;
			}
			break;
		case 'u':
			opts->default_bar_style = pv_strdup(optarg);
			if (NULL == opts->default_bar_style) {
				fprintf(stderr, "%s: -u: %s\n", opts->program_name, strerror(errno));
				opts_free(opts);
				return NULL;
			}
			break;
		case 'L':
			opts->rate_limit = pv_getnum_size(optarg, opts->decimal_units);
			break;
		case 'B':
			opts->buffer_size = (size_t) pv_getnum_size(optarg, opts->decimal_units);
			opts->no_splice = true;
			break;
		case 'C':
			opts->no_splice = true;
			break;
		case 'E':
			opts->skip_errors++;
			break;
		case 'Z':
			opts->error_skip_block = pv_getnum_size(optarg, opts->decimal_units);
			break;
		case 'S':
			opts->stop_at_size = true;
			break;
		case 'Y':
			opts->sync_after_write = true;
			break;
		case 'K':
			opts->direct_io = true;
			break;
		case 'O':
			opts->sparse_output = true;
			opts->no_splice = true;
			break;
		case 'X':
			opts->discard_input = true;
			opts->no_splice = true;
			break;
		case 'U':
			opts->store_and_forward_file = pv_strdup(optarg);
			if (NULL == opts->store_and_forward_file) {
				fprintf(stderr, "%s: -U: %s\n", opts->program_name, strerror(errno));
				opts_free(opts);
				return NULL;
			}
			opts->action = PV_ACTION_STORE_AND_FORWARD;
			break;
		case 'R':
			opts->remote = (pid_t) pv_getnum_count(optarg, false);
			opts->action = PV_ACTION_REMOTE_CONTROL;
			break;
		case 'Q':
			opts->query = (pid_t) pv_getnum_count(optarg, false);
			opts->action = PV_ACTION_QUERY;
			break;
		case 'P':
			opts->pidfile = pv_strdup(optarg);
			if (NULL == opts->pidfile) {
				fprintf(stderr, "%s: -P: %s\n", opts->program_name, strerror(errno));
				opts_free(opts);
				return NULL;
			}
			break;
		case 'F':
			opts->format = pv_strdup(optarg);
			if (NULL == opts->format) {
				fprintf(stderr, "%s: -F: %s\n", opts->program_name, strerror(errno));
				opts_free(opts);
				return NULL;
			}
			break;
		case 'x':
			opts->extra_display = pv_strdup(optarg);
			if (NULL == opts->extra_display) {
				fprintf(stderr, "%s: -x: %s\n", opts->program_name, strerror(errno));
				opts_free(opts);
				return NULL;
			}
			break;
		case 'd':
			if (!opts_watchfd_parse(opts, optarg, NULL, 0)) {
				opts_free(opts);
				return NULL;
			}
			opts->action = PV_ACTION_WATCHFD;
			break;
		case 'o':
			opts->output = pv_strdup(optarg);
			if (NULL == opts->output) {
				fprintf(stderr, "%s: -o: %s\n", opts->program_name, strerror(errno));
				opts_free(opts);
				return NULL;
			}
			break;
		case 'm':
			opts->average_rate_window = pv_getnum_count(optarg, opts->decimal_units);
			break;
#ifdef ENABLE_DEBUGGING
		case '!':
			debugging_output_destination(optarg);
			break;
#endif				/* ENABLE_DEBUGGING */
		default:
			/*@-mustfreefresh@ *//* see above */
			/*@-formatconst@ */
#ifdef HAVE_GETOPT_LONG
			fprintf(stderr, _("Try `%s --help' for more information."), opts->program_name);
#else
			fprintf(stderr, _("Try `%s -h' for more information."), opts->program_name);
#endif
			/*@+formatconst@ */
			/*
			 * splint note: formatconst is warning about the use
			 * of a non constant (translatable) format string;
			 * this is unavoidable here and the only attack
			 * vector is through the message catalogue.
			 */
			fprintf(stderr, "\n");
			opts_free(opts);
			opts = NULL;
			return NULL;	    /* early return */
			/*@+mustfreefresh@ */
		}

	} while (c != -1);

	/*
	 * splint thinks we can reach here after opts_free() and opts=NULL
	 * above, so explicitly return here if opts was set to NULL.
	 */
	if (NULL == opts)
		return NULL;

	if (PV_ACTION_WATCHFD == opts->action) {
		if (opts->linemode || opts->null_terminated_lines || opts->stop_at_size
		    || (opts->skip_errors > 0) || (opts->buffer_size > 0)
		    || (opts->rate_limit > 0)) {
			/*@-mustfreefresh@ *//* see above */
			fprintf(stderr, "%s: %s\n", opts->program_name,
				_("cannot use line mode or transfer modifier options when watching file descriptors"));
			opts_free(opts);
			return NULL;
			/*@+mustfreefresh@ */
		}

		if (opts->cursor) {
			/*@-mustfreefresh@ *//* see above */
			fprintf(stderr, "%s: %s\n", opts->program_name,
				_("cannot use cursor positioning when watching file descriptors"));
			opts_free(opts);
			return NULL;
			/*@+mustfreefresh@ */
		}

		if (0 != opts->remote) {
			/*@-mustfreefresh@ *//* see above */
			fprintf(stderr, "%s: %s\n", opts->program_name,
				_("cannot use remote control when watching file descriptors"));
			opts_free(opts);
			return NULL;
			/*@+mustfreefresh@ */
		}

		if (0 != opts->query) {
			/*@-mustfreefresh@ *//* see above */
			fprintf(stderr, "%s: %s\n", opts->program_name,
				_("cannot use remote query when watching file descriptors"));
			opts_free(opts);
			return NULL;
			/*@+mustfreefresh@ */
		}

		if (NULL != opts->output) {
			/*@-mustfreefresh@ *//* see above */
			fprintf(stderr, "%s: -o: %s\n", opts->program_name,
				_("cannot transfer files when watching file descriptors"));
			opts_free(opts);
			return NULL;
			/*@+mustfreefresh@ */
		}

		/* Accept additional watchfd arguments. */
		while (optind < (int) argc) {
			if (!opts_watchfd_parse(opts, argv[optind], NULL, 0)) {
				opts_free(opts);
				return NULL;
			}
			optind++;
		}

#ifndef __APPLE__
		if (0 != access("/proc/self/fdinfo", X_OK)) {	/* flawfinder: ignore */
			/*
			 * flawfinder rationale: access() is used here as a
			 * low cost stat() to see whether the path exists at
			 * all, under a path only modifiable by root, so is
			 * unlikely to be exploitable.
			 */
			/*@-mustfreefresh@ *//* see above */
			fprintf(stderr, "%s: -d: %s\n", opts->program_name,
				_("not available on systems without /proc/self/fdinfo"));
			opts_free(opts);
			return NULL;
			/*@+mustfreefresh@ */
		}
#endif
	}

	/* Don't allow -R and -Q together. */
	if ((0 != opts->remote) && (0 != opts->query)) {
		/*@-mustfreefresh@ *//* see above */
		fprintf(stderr, "%s: %s\n", opts->program_name,
			_("cannot use remote control and remote query together"));
		opts_free(opts);
		return NULL;
		/*@+mustfreefresh@ */
	}

	/*
	 * Default options: -pterb
	 */
	if (0 == numopts) {
		opts->progress = true;
		opts->timer = true;
		opts->eta = true;
		opts->rate = true;
		opts->bytes = true;
	}

	/* If -Z was given but not -E, pretend one -E was given too. */
	if (opts->error_skip_block > 0 && 0 == opts->skip_errors)
		opts->skip_errors = 1;

	/*
	 * Don't allow any non-option arguments with -R or -Q.
	 */
	if ((optind < (int) argc) && ((0 != opts->remote) || (0 != opts->query))) {
		/*@-mustfreefresh@ *//* see above */
		fprintf(stderr, "%s: %s: %s\n", opts->program_name, 0 != opts->remote ? "-R" : "-Q",
			_("files cannot be specified with this option"));
		opts_free(opts);
		return NULL;
		/*@+mustfreefresh@ */
	}

	/*
	 * Store remaining command-line arguments.
	 */
	while (optind < (int) argc) {
		if (!opts_add_file(opts, argv[optind++])) {
			opts_free(opts);
			return NULL;
		}
	}

	return opts;
}
