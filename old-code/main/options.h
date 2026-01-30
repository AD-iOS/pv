/*
 * Global program option structure and the parsing function prototype.
 *
 * Copyright 2002-2008, 2010, 2012-2015, 2017, 2021, 2023-2025 Andrew Wood
 *
 * License GPLv3+: GNU GPL version 3 or later; see `docs/COPYING'.
 */

#ifndef _OPTIONS_H
#define _OPTIONS_H 1

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct opts_s;
typedef struct opts_s *opts_t;

/*
 * Overall program actions to select.
 */
typedef enum {
	PV_ACTION_NOTHING,		/* do nothing, and exit */
	PV_ACTION_TRANSFER,		/* transfer data */
	PV_ACTION_STORE_AND_FORWARD,	/* store to file, then output from it */
	PV_ACTION_WATCHFD,		/* watch process file descriptors */
	PV_ACTION_REMOTE_CONTROL,	/* remotely control another pv */
	PV_ACTION_QUERY			/* watch the state of another pv */
} pvaction_t;

/*
 * Structure describing run-time options.
 *
 * Members are ordered by size to minimise padding.
 */
struct opts_s {
	double interval;               /* interval between updates */
	double delay_start;            /* delay before first display */
	/*@keep@*/ const char *program_name; /* name the program is running as */
	/*@keep@*/ /*@null@*/ char *output; /* fd to write output to */
	/*@keep@*/ /*@null@*/ char *name;    /* display name, if any */
	/*@keep@*/ /*@null@*/ char *default_bar_style; /* default bar style */
	/*@keep@*/ /*@null@*/ char *format;  /* output format, if any */
	/*@keep@*/ /*@null@*/ char *pidfile; /* PID file, if any */
	/*@keep@*/ /*@null@*/ char *store_and_forward_file; /* store and forward file, if any */
	/*@keep@*/ /*@null@*/ char *extra_display; /* extra display specifier, if any */
	/*@keep@*/ /*@null@*/ pid_t *watchfd_pid;  /* array of processes to watch fds of */
	/*@keep@*/ /*@null@*/ int *watchfd_fd;  /* array of fds to watch in each one (0=all) */
	/*@keep@*/ /*@null@*/ const char **argv;   /* array of non-option arguments */
	size_t lastwritten;            /* show N bytes last written */
	off_t rate_limit;              /* rate limit, in bytes per second */
	size_t buffer_size;            /* buffer size, in bytes (0=default) */
	off_t size;                    /* total size of data */
	off_t error_skip_block;        /* skip block size, 0 for adaptive */
	pid_t remote;                  /* PID of pv to update settings of */
	pid_t query;                   /* PID of pv to query progress of */
	unsigned int skip_errors;      /* skip read errors counter */
	unsigned int average_rate_window; /* time window in seconds for average rate calculations */
	unsigned int width;            /* screen width */
	unsigned int height;           /* screen height */
	unsigned int argc;             /* number of non-option arguments */
	unsigned int argv_length;      /* allocated array size */
	unsigned int watchfd_count;	       /* number of watchfd items */
	unsigned int watchfd_length;	       /* allocated array size */
	pvaction_t action;	       /* the program action to perform */
	bool progress;                 /* progress bar flag */
	bool timer;                    /* timer flag */
	bool eta;                      /* ETA flag */
	bool fineta;                   /* absolute ETA flag */
	bool rate;                     /* rate counter flag */
	bool average_rate;             /* average rate counter flag */
	bool bytes;                    /* bytes transferred flag */
	bool bits;                     /* report transfer size in bits */
	bool decimal_units;            /* decimal prefix flag */
	bool bufpercent;               /* transfer buffer percentage flag */
	bool force;                    /* force-if-not-terminal flag */
	bool cursor;                   /* whether to use cursor positioning */
	bool numeric;                  /* numeric output only */
	bool wait;                     /* wait for transfer before display */
	bool rate_gauge;               /* if size unknown, show rate vs max rate */
	bool linemode;                 /* count lines instead of bytes */
	bool null_terminated_lines;    /* lines are null-terminated */
	bool no_display;               /* do nothing other than pipe data */
	bool no_splice;                /* flag set if never to use splice */
	bool stop_at_size;             /* set if we stop at "size" bytes */
	bool sync_after_write;         /* set if we sync after every write */
	bool direct_io;                /* set if O_DIRECT is to be used */
	bool sparse_output;            /* set if we leave holes in the output */
	bool discard_input;            /* set to write nothing to output */
	bool show_stats;	       /* set to write statistics at the end */
	bool width_set_manually;       /* width was set manually, not detected */
	bool height_set_manually;      /* height was set manually, not detected */
};

/*@-exportlocal@*/
/* splint thinks opts_free is exported but not used - it is used. */

extern /*@null@*/ /*@only@*/ opts_t opts_parse(unsigned int, char **);
extern void opts_free(/*@only@*/ opts_t);
extern bool opts_add_file(opts_t, const char *);

#ifdef __cplusplus
}
#endif

#endif /* _OPTIONS_H */
