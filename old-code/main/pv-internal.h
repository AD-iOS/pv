/*
 * Functions internal to the PV library.  Include "config.h" first.
 *
 * Copyright 2002-2008, 2010, 2012-2015, 2017, 2021, 2023-2025 Andrew Wood
 *
 * License GPLv3+: GNU GPL version 3 or later; see `docs/COPYING'.
 */

#ifndef _PV_INTERNAL_H
#define _PV_INTERNAL_H 1

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RATE_GRANULARITY	100000000	 /* nsec between -L rate chunks */
#define RATE_BURST_WINDOW	5	 	 /* rate burst window (multiples of rate) */
#define REMOTE_INTERVAL		100000000	 /* nsec between checks for -R and -Q */
#define BUFFER_SIZE		(size_t) 409600	 /* default transfer buffer size */
#define BUFFER_SIZE_MAX		(size_t) 524288	 /* max auto transfer buffer size */
#define MAX_READ_AT_ONCE	(size_t) 524288	 /* max to read() in one go */
#define MAX_WRITE_AT_ONCE	(size_t) 524288	 /* max to write() in one go */
#define TRANSFER_READ_TIMEOUT	0.09L		 /* seconds to time reads out at */
#define TRANSFER_WRITE_TIMEOUT	0.9L		 /* seconds to time writes out at */
#define MAX_LINE_POSITIONS	100000		 /* number of lines to remember positions of */

#define MAXIMISE_BUFFER_FILL	1

#define PV_SIZEOF_DEFAULT_FORMAT	512
#define PV_SIZEOF_CWD			4096
#define PV_SIZEOF_LASTWRITTEN_BUFFER	256
#define PV_SIZEOF_PREVLINE_BUFFER	1024
#define PV_FORMAT_ARRAY_MAX		100
#define PV_SIZEOF_FORMAT_SEGMENTS_BUF	4096
#define PV_SIZEOF_CRS_LOCK_FILE		1024

#define PV_SIZEOF_FILE_FDINFO		4096
#define PV_SIZEOF_FILE_FD		4096
#define PV_SIZEOF_FILE_FDPATH		4096
#define PV_SIZEOF_DISPLAY_NAME		512

#define PV_BARSTYLE_MAX			4	/* number of different styles allowed in a format */
#define PV_BARSTYLE_SIZEOF_STRING	10	/* max length of a bar constituent component in bytes */
#define PV_BARSTYLE_MAX_FILLERS		10	/* max number of bar filler strings */

#define PV_DISPLAY_WINDOWTITLE		1
#define PV_DISPLAY_PROCESSTITLE		2


/*
 * Structure for data shared between multiple "pv -c" instances.
 */
struct pvipccursorstate_s {
	int y_topmost;		/* terminal row of topmost "pv" instance */
	bool tty_tostop_added;	/* whether any instance had to set TOSTOP on the terminal */
};

/*
 * Types of transfer count - bytes, decimal bytes or lines.
 */
typedef enum {
	PV_TRANSFERCOUNT_BYTES,
	PV_TRANSFERCOUNT_DECBYTES,
	PV_TRANSFERCOUNT_LINES
} pvtransfercount_t;


/*
 * Structure describing a short string used as part of a progress bar, whose
 * width in display characters may not be the same as its length in bytes.
 */
struct pvbarstring_spec_s {
	char string[PV_BARSTYLE_SIZEOF_STRING];
	uint8_t width;
	uint8_t bytes;
};

/*
 * Structure describing a style of progress bar.
 *
 * The part that moves back and forth when the size isn't known, such as
 * "<=>", is "indicator".
 *
 * The string used to populate the rest of the bar is one of "filler", which
 * contains "filler_entries" array items.  The first item "filler[0]" is
 * used for an empty part such as " ".  The last item
 * "filler[filler_entries-1]" is used for a 100% full part, such as "=".
 *
 * If there are more than the minimum of 2 entries, then the intervening
 * entries are used for partial fills.
 *
 * When there are only 2 entries and the bar isn't completely empty or full,
 * the last filled portion of the bar is filled with "tip" instead of the
 * last filler item to indicate the tip of the bar, such as ">".
 *
 * Note that only the indicator is expected to ever be wider than 1 display
 * character.  All other items are expected to have a width of 0 or 1.
 *
 * The "style_id" is an opaque identifier that should only be used to
 * determine whether two styles are the same.  It is always nonzero.
 */
struct pvbarstyle_s {
	uint8_t style_id;
	uint8_t filler_entries;
	struct pvbarstring_spec_s indicator;
	struct pvbarstring_spec_s tip;
	struct pvbarstring_spec_s filler[PV_BARSTYLE_MAX_FILLERS];
};
typedef struct pvbarstyle_s *pvbarstyle_t;


/* Display format component type, -1 for static string. */
typedef int8_t pvdisplay_component_t;
#define PVDISPLAY_COMPONENT_MAX (127)	/* INT8_MAX */

/* Byte count of a part of the display. */
typedef uint16_t pvdisplay_bytecount_t;
#define PVDISPLAY_BYTECOUNT_MAX (65535)	/* UINT16_MAX */

/* Width of a part of the display. */
typedef uint16_t pvdisplay_width_t;
#define PVDISPLAY_WIDTH_MAX (65535)	/* UINT16_MAX */

/*
 * Structure defining the current state of a single watched file descriptor.
 */
struct pvwatchfd_s;
typedef /*@null@*/ struct pvwatchfd_s *pvwatchfd_t;

/* String pointer, that is the only pointer to this resource, that can be null. */
typedef /*@only@*/ /*@null@*/ char * nullable_string_t;

/*
 * Structure for holding PV internal state. Opaque outside the PV library.
 *
 * In general, members are ordered by size, to minimise padding.
 */
struct pvstate_s {
	/******************
	 * Program status *
	 ******************/
	struct pvprogramstatus_s {
		char cwd[PV_SIZEOF_CWD];	 /* current working directory for relative path */
		int current_input_file;		 /* index of current file being read */
		int exit_status; 		 /* exit status to give (0=OK) */
		bool terminal_supports_utf8;	 /* whether the terminal supports UTF-8 */
		bool terminal_supports_colour;	 /* whether the terminal supports colour */
		bool checked_colour_support;	 /* whether we have checked colour support yet */
	} status;

	/***************
	 * Input files *
	 ***************/
	struct pvinputfiles_s {
		/*@only@*/ /*@null@*/ nullable_string_t *filename; /* input filenames */
		unsigned int file_count;	 /* number of input files */
	} files;

	/*********************************
	 * Items to watch with --watchfd *
	 *********************************/
	struct {
		/*@only@*/ /*@null@*/
		struct pvwatcheditem_s {	/* array of PID or PID:FD items */
			pid_t pid;		 /* watched PID */
			int fd;			 /* watched fd, or -1 for all */
			pvwatchfd_t info_array;	 /* watch information for each fd */
			int array_length;	 /* length of watch info array */
			bool finished;		 /* "PID:FD": fd closed; or PID gone */
		} *watching;
		unsigned int count;	/* number of items in these arrays */
		bool multiple_pids;	/* true if more than one distinct PID */
	} watchfd;

	/*******************
	 * Program control *
	 *******************/
	struct pvcontrol_s {
		char default_format[PV_SIZEOF_DEFAULT_FORMAT];	 /* default format string */
		double interval;                 /* interval between updates */
		double delay_start;              /* delay before first display */
		/*@only@*/ /*@null@*/ char *name;		 /* display name */
		/*@only@*/ /*@null@*/ char *format_string;	 /* output format string */
		/*@only@*/ /*@null@*/ char *extra_display_spec;  /* full spec for extra displays */
		/*@only@*/ /*@null@*/ char *extra_format_string; /* extra format string alone */
		/*@null@*/ char *output_name;    /* name of the output, for diagnostics */
		/*@null@*/ char *default_bar_style; /* which bar style to use by default */
		off_t error_skip_block;          /* skip block size, 0 for adaptive */
		off_t rate_limit;                /* rate limit, in bytes per second */
		size_t target_buffer_size;       /* buffer size (0=default) */
		off_t size;                      /* total size of data */
		unsigned int skip_errors;        /* skip read errors counter */
		int output_fd;                   /* fd to write output to */
		unsigned int average_rate_window; /* time window in seconds for average rate calculations */
		unsigned int history_interval;	 /* seconds between each average rate calc history entry */
		pvdisplay_width_t width;         /* screen width */
		unsigned int height;             /* screen height */
		unsigned int extra_displays;	 /* bitmask of extra display destinations */
		struct {			 /* old-style format options (used by -R) */
			size_t lastwritten;	  /* --last-written (amount) */
			bool progress;		  /* --progress */
			bool timer;		  /* --timer */
			bool eta;		  /* --eta */
			bool fineta;		  /* --fineta */
			bool rate;		  /* --rate */
			bool average_rate;	  /* --average-rate */
			bool bytes;		  /* --bytes */
			bool bufpercent;	  /* --buffer-percent */
		} format_option;
		bool force;                      /* display even if not on terminal */
		bool cursor;                     /* use cursor positioning */
		bool numeric;                    /* numeric output only */
		bool wait;                       /* wait for data before display */
		bool rate_gauge;                 /* if size unknown, show rate vs max rate */
		bool linemode;                   /* count lines instead of bytes */
		bool bits;			 /* report bits instead of bytes */
		bool decimal_units;		 /* use decimal prefixes */
		bool null_terminated_lines;      /* lines are null-terminated */
		bool no_display;                 /* do nothing other than pipe data */
		bool no_splice;                  /* never use splice() */
		bool stop_at_size;               /* set if we stop at "size" bytes */
		bool sync_after_write;           /* set if we sync after every write */
		bool direct_io;                  /* set if O_DIRECT is to be used */
		bool direct_io_changed;          /* set when direct_io is changed */
		bool sparse_output;		 /* set if we leave holes in the output */
		bool discard_input;              /* write nothing to stdout */
		bool show_stats;		 /* show statistics on exit */
		bool width_set_manually;	 /* width was set manually, not detected */
		bool height_set_manually;	 /* height was set manually, not detected */
	} control;

	/*******************
	 * Signal handling *
	 *******************/
	struct pvsignal_s {
		/* old signal handlers to restore in pv_sig_fini(). */
		struct sigaction old_sigpipe;
		struct sigaction old_sigttou;
		struct sigaction old_sigtstp;
		struct sigaction old_sigcont;
		struct sigaction old_sigwinch;
		struct sigaction old_sigint;
		struct sigaction old_sighup;
		struct sigaction old_sigterm;
#ifdef PV_REMOTE_CONTROL
		struct sigaction old_sigusr2;
		struct sigaction old_sigusr1;
#endif
		struct sigaction old_sigalrm;
		struct timespec when_tstp_arrived;	 /* see pv_sig_tstp() / __cont() */
		struct timespec total_stoppage_time;	 /* total time spent stopped */
#ifdef PV_REMOTE_CONTROL
		volatile sig_atomic_t rxusr2;	 /* whether SIGUSR2 was received */
		volatile pid_t sender_usr2;	 /* PID of sending process for SIGUSR2 */
		volatile sig_atomic_t rxusr1;	 /* whether SIGUSR1 was received */
		volatile pid_t sender_usr1;	 /* PID of sending process for SIGUSR1 */
#endif
	} signal;

	/*******************
	 * Transient flags *
	 *******************/
	struct pvtransientflags_s {
		volatile sig_atomic_t reparse_display;	 /* whether to re-check format string */
		volatile sig_atomic_t terminal_resized;	 /* whether we need to get term size again */
		volatile sig_atomic_t trigger_exit;	 /* whether we need to abort right now */
		volatile sig_atomic_t clear_tty_tostop_on_exit;	/* whether to clear tty TOSTOP on exit */
		volatile sig_atomic_t suspend_stderr;	 /* whether writing to stderr is suspended */
		volatile sig_atomic_t skip_next_sigcont; /* whether to ignore the next SIGCONT */
		volatile sig_atomic_t pipe_closed;	 /* whether the output pipe was closed */
	} flags;

	/*****************
	 * Display state *
	 *****************/
	struct pvdisplay_s {

		struct pvdisplay_segment_s {	/* format string broken into segments */
			/* See pv__format_init() for more details. */
			/*@dependent@*/ /*@null@*/ const char *string_parameter; /* parameter after colon in %{x:} */
			pvdisplay_bytecount_t string_parameter_bytes;	/* number of bytes in string_parameter */
			pvdisplay_component_t type;	/* component type, -1 for static string */
			int8_t parameter;		/* component parameter, such as bar style index */
			pvdisplay_width_t chosen_size;	/* "n" from %<n>A, or 0 */
			pvdisplay_bytecount_t offset;	/* start offset of this segment in the build buffer */
			pvdisplay_bytecount_t bytes;	/* length of segment in bytes in the build buffer */
			pvdisplay_width_t width;	/* displayed width of segment */
		} format[PV_FORMAT_ARRAY_MAX];

		struct pvbarstyle_s barstyle[PV_BARSTYLE_MAX];

		/* The last-written "n" bytes. */
		char lastwritten_buffer[PV_SIZEOF_LASTWRITTEN_BUFFER];

		/* The most recently output complete line. */
		char previous_line[PV_SIZEOF_PREVLINE_BUFFER];
		/* The line being received now. */
		char next_line[PV_SIZEOF_PREVLINE_BUFFER];

		/*@only@*/ /*@null@*/ char *display_buffer;	/* buffer for display string */
		off_t initial_offset;			 /* offset when first opened (when watching fds) */
		size_t next_line_len;				 /* length of currently receiving line so far */

		size_t format_segment_count;	 /* number of format string segments */

		pvtransfercount_t count_type;	 /* type of count for transfer, rate, etc */

		pvdisplay_width_t prev_screen_width;	 /* screen width last time we were called */

		pvdisplay_bytecount_t display_buffer_size;	/* size allocated to display buffer */
		pvdisplay_bytecount_t display_string_bytes;	/* byte length of string in display buffer */
		pvdisplay_width_t display_string_width;		/* displayed width of string in display buffer */
		pvdisplay_bytecount_t lastwritten_bytes;	 /* largest number of last-written bytes to show */

		bool showing_timer;		 /* set if showing timer */
		bool showing_bytes;		 /* set if showing byte/line count */
		bool showing_rate;		 /* set if showing transfer rate */
		bool showing_last_written;	 /* set if displaying the last few bytes written */
		bool showing_previous_line;	 /* set if displaying the previously output line */

		bool format_uses_colour;	 /* set if the format string uses colours */
		bool colour_permitted;		 /* whether colour is permitted for this display */
		bool sgr_code_active;		 /* set while SGR code is active in a display line */
		bool final_update;		 /* set internally on the final update */
		bool output_produced;		 /* set once anything written to terminal */

	} display;

	/* Extra display for alternate outputs like a window title. */
	struct pvdisplay_s extra_display;

	/************************************
	 * Calculated state of the transfer *
	 ************************************/
	struct pvtransfercalc_s {
		long double transfer_rate;	 /* calculated transfer rate */
		long double average_rate;	 /* calculated average transfer rate */

		long double prev_elapsed_sec;	 /* elapsed sec at which rate last calculated */
		long double prev_rate;		 /* last calculated instantaneous transfer rate */
		long double prev_trans;		 /* amount transferred since last rate calculation */
		long double current_avg_rate;    /* current average rate over last history intervals */

		long double rate_min;		 /* minimum measured transfer rate */
		long double rate_max;		 /* maximum measured transfer rate */
		long double rate_sum;		 /* sum of all measured transfer rates */
		long double ratesquared_sum;	 /* sum of the squares of each transfer rate */
		unsigned long measurements_taken; /* how many times the rate was measured */

		/* Keep track of progress over last intervals to compute current average rate. */
		/*@null@*/ struct {	 /* state at previous intervals (circular buffer) */
			long double elapsed_sec;	/* time since start of transfer */
			off_t transferred;		/* amount transferred by that time */
		} *history;
		size_t history_len;		 /* total size of history array */
		size_t history_first;		 /* index of oldest entry */
		size_t history_last;		 /* index of newest entry */

		off_t prev_transferred;		 /* total amount transferred when called last time */

		double percentage;		 /* transfer percentage completion */
	} calc;

	/********************
	 * Cursor/IPC state *
	 ********************/
	struct pvcursorstate_s {
		char lock_file[PV_SIZEOF_CRS_LOCK_FILE];
#ifdef HAVE_IPC
		/*@keep@*/ /*@null@*/ struct pvipccursorstate_s *shared; /* data shared between instances */
		int shmid;		 /* ID of our shared memory segment */
		int pvcount;		 /* number of `pv' processes in total */
		int pvmax;		 /* highest number of `pv's seen */
		int y_lastread;		 /* last value of _y_top seen */
		int y_offset;		 /* our Y offset from this top position */
		int needreinit;		 /* counter if we need to reinit cursor pos */
#endif				/* HAVE_IPC */
		int lock_fd;		 /* fd of lockfile, -1 if none open */
		int y_start;		 /* our initial Y coordinate */
#ifdef HAVE_IPC
		bool noipc;		 /* set if we can't use IPC */
#endif				/* HAVE_IPC */
		bool disable;		 /* set if cursor positioning can't be used */
	} cursor;

	/*******************
	 * Transfer state  *
	 *******************/
	/*
	 * The transfer buffer is used for moving data from the input files
	 * to the output when splice() is not available.
	 *
	 * If buffer_size is smaller than control.target_buffer_size, then
	 * pv_transfer() will try to reallocate transfer_buffer to make
	 * buffer_size equal to control.target_buffer_size.
	 *
	 * Data from the input files is read into the buffer; read_position
	 * is the offset in the buffer that we've read data up to.
	 *
	 * Data is written to the output from the buffer, and write_position
	 * is the offset in the buffer that we've written data up to.  It
	 * will always be less than or equal to read_position.
	 */
	struct pvtransferstate_s {
		long double elapsed_seconds;	 /* how long we have been transferring data for */
		/*@only@*/ /*@null@*/ char *transfer_buffer;	 /* data transfer buffer */
		size_t buffer_size;		 /* size of buffer */
		size_t read_position;		 /* amount of data in buffer */
		size_t write_position;		 /* buffered data written */

		ssize_t to_write;		 /* max to write this time around */
		ssize_t written;		 /* bytes sent to stdout this time */

		size_t written_but_not_consumed; /* bytes in the output pipe, unread */

		off_t total_bytes_read;		 /* total bytes read */
		off_t total_written;		 /* total bytes or lines written */
		off_t transferred;		 /* amount transferred (written - unconsumed) */

		/* Keep track of line positions to backtrack written_but_not_consumed. */
		/*@only@*/ /*@null@*/ off_t *line_positions; /* line separator write positions (circular buffer) */
		size_t line_positions_capacity;	 /* total size of line position array */
		size_t line_positions_length;	 /* number of positions stored in array */
		size_t line_positions_head;	 /* index to use for next position */
		off_t last_output_position;	 /* write position last sent to output */

		/*
		 * While reading from a file descriptor we keep track of how
		 * many times in a row we've seen errors
		 * (read_errors_in_a_row), and whether or not we have put a
		 * warning on stderr about read errors on this fd
		 * (read_error_warning_shown).
		 *
		 * Whenever the active file descriptor changes from
		 * last_read_skip_fd, we reset read_errors_in_a_row to 0 and
		 * read_error_warning_shown to false for the new file
		 * descriptor and set last_read_skip_fd to the new fd
		 * number.
		 *
		 * This way, we're treating each input file separately.
		 */
		off_t read_errors_in_a_row;
		int last_read_skip_fd;
		/* read_error_warning_shown is defined below. */
#ifdef HAVE_SPLICE
		/*
		 * These variables are used to keep track of whether
		 * splice() was used; splice_failed_fd is the file
		 * descriptor that splice() last failed on, so that we don't
		 * keep trying to use it on an fd that doesn't support it,
		 * and splice_used is set to true if splice() was used this
		 * time within pv_transfer().
		 */
		int splice_failed_fd;
		bool splice_used;
#endif
		bool read_error_warning_shown;
		bool output_not_seekable;	/* set if lseek() fails on output */
	} transfer;
};

typedef struct pvprogramstatus_s *pvprogramstatus_t;
typedef struct pvinputfiles_s *pvinputfiles_t;
typedef struct pvcontrol_s *pvcontrol_t;
typedef struct pvsignal_s *pvsignal_t;
typedef struct pvtransientflags_s *pvtransientflags_t;
typedef struct pvdisplay_s *pvdisplay_t;
typedef struct pvdisplay_segment_s *pvdisplay_segment_t;
typedef struct pvtransfercalc_s *pvtransfercalc_t;
typedef struct pvcursorstate_s *pvcursorstate_t;
typedef struct pvtransferstate_s *pvtransferstate_t;

/*
 * Structure defining the current state of a single watched file descriptor.
 * The full definition needs to go here as it refers to sub-structures of
 * the main state, defined above.
 */
struct pvwatchfd_s {
	struct pvtransientflags_s flags;	/* transient flags */
	struct pvtransferstate_s transfer;	/* transfer state */
	struct pvtransfercalc_s calc;	 /* calculated transfer state */
	struct pvdisplay_s display;	 /* display data */
#ifdef __APPLE__
#else
	char file_fdinfo[PV_SIZEOF_FILE_FDINFO]; /* path to /proc fdinfo file */
	char file_fd[PV_SIZEOF_FILE_FD];	 /* path to /proc fd symlink  */
#endif
	char file_fdpath[PV_SIZEOF_FILE_FDPATH]; /* path to file that was opened */
	/*@keep@ */ char display_name[PV_SIZEOF_DISPLAY_NAME]; /* name to show on progress bar */
	struct stat sb_fd;		 /* stat of fd symlink */
	struct stat sb_fd_link;		 /* lstat of fd symlink */
	off_t size;			 /* size of whole file, 0 if unknown */
	off_t position;			 /* position last seen at */
	struct timespec start_time;	 /* time we started watching the fd */
	struct timespec end_time;	 /* time the fd was marked as closed */
	struct timespec total_stoppage_time;	 /* total time spent stopped */
	pid_t watch_pid;		 /* PID the fd belongs to */
	int watch_fd;			 /* fd to watch */
	bool closed;			 /* true once the fd is closed */
	bool displayable;		 /* false if not displayable */
	bool unused;			 /* true if free for re-use */
};

/*
 * Read-only counterparts to the above structure pointers, to be used in
 * function declarations where the function definitely shouldn't be altering
 * the contents of the structure.
 */
typedef const struct pvprogramstatus_s * readonly_pvprogramstatus_t;
typedef const struct pvinputfiles_s * readonly_pvinputfiles_t;
typedef const struct pvcontrol_s * readonly_pvcontrol_t;
typedef const struct pvsignal_s * readonly_pvsignal_t;
typedef const struct pvtransientflags_s * readonly_pvtransientflags_t;
typedef const struct pvdisplay_s * readonly_pvdisplay_t;
typedef const struct pvdisplay_segment_s * readonly_pvdisplay_segment_t;
typedef const struct pvtransfercalc_s * readonly_pvtransfercalc_t;
typedef const struct pvcursorstate_s * readonly_pvcursorstate_t;
typedef const struct pvtransferstate_s * readonly_pvtransferstate_t;

/*
 * Structure containing the parameters used by formatters.
 */
struct pvformatter_args_s {
	/*@dependent@*/ pvdisplay_t display;		/* the display being updated */
	/*@dependent@*/ pvdisplay_segment_t segment;	/* the segment of the display */
	/*@dependent@*/ readonly_pvprogramstatus_t status;	/* program status */
	/*@dependent@*/ readonly_pvcontrol_t control;		/* control settings */
	/*@dependent@*/ readonly_pvtransferstate_t transfer;	/* transfer state */
	/*@dependent@*/ readonly_pvtransfercalc_t calc;		/* calculated transfer state */
	/*@dependent@*/ char *buffer;			/* buffer to write formatted segments into */
	pvdisplay_bytecount_t buffer_size;		/* size of the buffer */
	pvdisplay_bytecount_t offset;			/* current write position in the buffer */
};
typedef struct pvformatter_args_s *pvformatter_args_t;


/* Pointer to a formatter function. */
typedef pvdisplay_bytecount_t (*pvdisplay_formatter_t)(pvformatter_args_t);


/*
 * Structure defining a format string sequence following a %.
 */
struct pvdisplay_component_s {
	/*@null@ */ const char *match;			/* string to match */
	/*@null@ */ pvdisplay_formatter_t function;	/* function to call */
	bool dynamic;			 /* whether it can scale with screen size */
};

void pv_error(char *, ...);

int pv_main_loop(pvstate_t);
void pv_calculate_transfer_rate(pvtransfercalc_t, readonly_pvtransferstate_t, readonly_pvcontrol_t, readonly_pvdisplay_t, bool);

long pv_bound_long(long, long, long);
long pv_seconds_remaining(const off_t, const off_t, const long double);
void pv_si_prefix(long double *, char *, const long double, pvtransfercount_t);
void pv_describe_amount(char *, size_t, char *, long double, char *, char *, pvtransfercount_t);

int8_t pv_display_barstyle_index(pvformatter_args_t, const char *);

pvdisplay_bytecount_t pv_formatter_segmentcontent(char *, pvformatter_args_t);

/*
 * Formatting functions.
 *
 * Each formatting function takes a state, the current display, and the
 * segment it's for; it also takes a buffer, with a particular size, and an
 * offset at which to start writing to the buffer.
 *
 * If the component is dynamically sized (such as a progress bar with no
 * chosen_size constraint), the segment's "width" is expected to have
 * already been populated by the caller, with the target width.
 *
 * The function writes the appropriate string to the buffer at the offset,
 * and updates the segment's "offset" and "bytes".  The number of bytes
 * written ("bytes") is also returned; it will be 0 if the string would not
 * fit into the buffer.
 *
 * The caller is expected to update the segment's "width".
 *
 * If called with a buffer size of 0, only the side effects occur (such as
 * setting flags like display->showing_timer).
 */
pvdisplay_bytecount_t pv_formatter_progress(pvformatter_args_t);
pvdisplay_bytecount_t pv_formatter_progress_bar_only(pvformatter_args_t);
pvdisplay_bytecount_t pv_formatter_progress_amount_only(pvformatter_args_t);
pvdisplay_bytecount_t pv_formatter_bar_default(pvformatter_args_t);
pvdisplay_bytecount_t pv_formatter_bar_plain(pvformatter_args_t);
pvdisplay_bytecount_t pv_formatter_bar_block(pvformatter_args_t);
pvdisplay_bytecount_t pv_formatter_bar_granular(pvformatter_args_t);
pvdisplay_bytecount_t pv_formatter_bar_shaded(pvformatter_args_t);
pvdisplay_bytecount_t pv_formatter_timer(pvformatter_args_t);
pvdisplay_bytecount_t pv_formatter_eta(pvformatter_args_t);
pvdisplay_bytecount_t pv_formatter_fineta(pvformatter_args_t);
pvdisplay_bytecount_t pv_formatter_rate(pvformatter_args_t);
pvdisplay_bytecount_t pv_formatter_average_rate(pvformatter_args_t);
pvdisplay_bytecount_t pv_formatter_bytes(pvformatter_args_t);
pvdisplay_bytecount_t pv_formatter_buffer_percent(pvformatter_args_t);
pvdisplay_bytecount_t pv_formatter_last_written(pvformatter_args_t);
pvdisplay_bytecount_t pv_formatter_previous_line(pvformatter_args_t);
pvdisplay_bytecount_t pv_formatter_name(pvformatter_args_t);
pvdisplay_bytecount_t pv_formatter_sgr(pvformatter_args_t);

bool pv_format (pvprogramstatus_t, readonly_pvcontrol_t,
		readonly_pvtransferstate_t, readonly_pvtransfercalc_t,
		/*@null@ */ const char *, pvdisplay_t, bool, bool);
void pv_display (pvprogramstatus_t, readonly_pvcontrol_t, pvtransientflags_t,
		 readonly_pvtransferstate_t, pvtransfercalc_t,
		 pvcursorstate_t, pvdisplay_t, /*@null@ */ pvdisplay_t, bool);

ssize_t pv_transfer(pvstate_t, int, bool *, bool *, off_t, long *);
int pv_next_file(pvstate_t, unsigned int, int);
/*@keep@*/ const char *pv_current_file_name(pvstate_t);

unsigned int pv_update_calc_average_rate_window(pvtransfercalc_t, unsigned int);
void pv_reset_calc(pvtransfercalc_t);
void pv_reset_transfer(pvtransferstate_t);
void pv_reset_flags(pvtransientflags_t);
void pv_reset_display(pvdisplay_t);
void pv_reset_watchfd(pvwatchfd_t);
void pv_freecontents_display(pvdisplay_t);
void pv_freecontents_transfer(pvtransferstate_t);
void pv_freecontents_calc(pvtransfercalc_t);
void pv_freecontents_watchfd(pvwatchfd_t);
void pv_freecontents_watchfd_items(struct pvwatcheditem_s *, unsigned int);

void pv_write_retry(int, const char *, size_t);
void pv_tty_write(readonly_pvtransientflags_t, const char *, size_t);

void pv_crs_fini(pvcursorstate_t, readonly_pvcontrol_t, pvtransientflags_t);
void pv_crs_init(pvcursorstate_t, readonly_pvcontrol_t, pvtransientflags_t);
void pv_crs_update(pvcursorstate_t, readonly_pvcontrol_t, pvtransientflags_t, const char *);
#ifdef HAVE_IPC
void pv_crs_needreinit(pvcursorstate_t);
#endif

void pv_sig_allowpause(void);
void pv_sig_checkbg(void);
void pv_sig_nopause(void);

bool pv_remote_check(pvstate_t);

int pv_watchfd_info(pvstate_t, pvwatchfd_t, bool);
bool pv_watchfd_changed(pvwatchfd_t);
off_t pv_watchfd_position(pvwatchfd_t);
int pv_watchpid_scanfds(pvstate_t, pid_t, int, int *, pvwatchfd_t *);
void pv_watchpid_setname(pvstate_t, pvwatchfd_t);

#ifdef __cplusplus
}
#endif

#endif /* _PV_INTERNAL_H */
