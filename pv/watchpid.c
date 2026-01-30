/*
 * Functions for watching file descriptors in other processes.
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

#define _GNU_SOURCE 1
#include <limits.h>

#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#ifdef __APPLE__
#include <libproc.h>
#include <sys/proc_info.h>

#ifndef PROC_PIDLISTFDS
#define PROC_PIDLISTFDS            1
#endif

#ifndef PROC_PIDFDVNODEPATHINFO
#define PROC_PIDFDVNODEPATHINFO    2
#endif

#ifndef PROX_FDTYPE_VNODE
#define PROX_FDTYPE_VNODE          1
#endif

#ifndef PROC_PIDLISTFD_SIZE
#define PROC_PIDLISTFD_SIZE        sizeof(struct proc_fdinfo)
#endif

#ifndef PROC_PIDFDVNODEPATHINFO_SIZE
#define PROC_PIDFDVNODEPATHINFO_SIZE    sizeof(struct vnode_fdinfowithpath)
#endif
// struct vnode_fdinfowithpath vnodeInfo = { };
// struct proc_fdinfo *fd_infos = NULL;

#endif /* __APPLE__ */

/*@-type@*/
/* splint has trouble with off_t and mode_t. */

/*
 * Set info->size to the size of the file info->file_fdpath points to,
 * assuming that info->sb_fd has been populated by stat(), or to 0 if the
 * file size could not be determined or the file was opened in write mode;
 * returns false if the file was not a block device or regular file.
 */
static bool filesize(pvwatchfd_t info)
{
	if (NULL == info)
		return false;
	if (S_ISBLK(info->sb_fd.st_mode)) {
		int fd;

		/*
		 * Get the size of block devices by opening
		 * them and seeking to the end.
		 */
		fd = open(info->file_fdpath, O_RDONLY);	/* flawfinder: ignore */
		/*
		 * flawfinder: redirection check below; risk is minimal as
		 * we are not actually reading any data here.
		 */
		if (fd >= 0) {
			/*
			 * TOCTOU mitigation: check it's still a block
			 * device before trying to seek to the end. 
			 * Otherwise treat it as unreadable and set the size
			 * to 0.
			 */
			struct stat check_fd_sb;
			memset(&check_fd_sb, 0, sizeof(check_fd_sb));
			info->size = 0;
			if (0 == fstat(fd, &check_fd_sb) && S_ISBLK(check_fd_sb.st_mode)) {
				info->size = lseek(fd, 0, SEEK_END);
			}
			(void) close(fd);
		} else {
			info->size = 0;
		}
	} else if (S_ISREG(info->sb_fd.st_mode)) {
		if ((info->sb_fd_link.st_mode & S_IWUSR) == 0) {
			info->size = info->sb_fd.st_size;
		}
	} else {
		return false;
	}

	return true;
}

/*@+type@*/

#ifdef __APPLE__
int pv_watchfd_info(pvstate_t state, pvwatchfd_t info, bool automatic)
{
	struct vnode_fdinfowithpath vnodeInfo = { };

	if (NULL == state)
		return -1;
	if (NULL == info)
		return -1;

	if (kill(info->watch_pid, 0) != 0) {
		if (!automatic)
			pv_error("%s %u: %s", _("pid"), info->watch_pid, strerror(errno));
		return 1;
	}

	int32_t proc_fd = (int32_t) info->watch_fd;
	int size = proc_pidfdinfo(info->watch_pid, proc_fd,
				  PROC_PIDFDVNODEPATHINFO, &vnodeInfo,
				  PROC_PIDFDVNODEPATHINFO_SIZE);
	if (size != PROC_PIDFDVNODEPATHINFO_SIZE) {
		pv_error("%s %u: %s %d: %s", _("pid"), info->watch_pid, _("fd"), info->watch_fd, strerror(errno));
		return 3;
	}

	strlcpy(info->file_fdpath, vnodeInfo.pvip.vip_path, PV_SIZEOF_FILE_FDPATH);

	info->size = 0;

	if (!(0 == stat(info->file_fdpath, &(info->sb_fd)))) {
		if (!automatic)
			pv_error("%s %u: %s %d: %s: %s",
				 _("pid"),
				 info->watch_pid, _("fd"), info->watch_fd, info->file_fdpath, strerror(errno));
		return 3;
	}

	if (!filesize(info)) {
		if (!automatic)
			pv_error("%s %u: %s %d: %s: %s",
				 _("pid"),
				 info->watch_pid,
				 _("fd"), info->watch_fd, info->file_fdpath, _("not a regular file or block device"));
		return 4;
	}

	return 0;
}

#else

/*
 * Fill in the given information structure with the file paths and stat
 * details of the given file descriptor within the given process (given
 * within the info structure).
 *
 * Returns nonzero on error - error codes are:
 *
 *  -1 - info or state were NULL
 *   1 - process does not exist
 *   2 - readlink on /proc/pid/fd/N failed
 *   3 - stat or lstat on /proc/pid/fd/N failed
 *   4 - file descriptor is not opened on a regular file
 *
 * If "automatic" is true, then this fd was picked automatically, and so if
 * it's not readable or not a regular file, no error is displayed and the
 * function just returns an error code.
 */
int pv_watchfd_info(pvstate_t state, pvwatchfd_t info, bool automatic)
{
	if (NULL == state)
		return -1;
	if (NULL == info)
		return -1;

	if (kill(info->watch_pid, 0) != 0) {
		if (!automatic)
			pv_error("%s %u: %s", _("pid"), info->watch_pid, strerror(errno));
		return 1;
	}
	(void) pv_snprintf(info->file_fdinfo, PV_SIZEOF_FILE_FDINFO,
			   "/proc/%u/fdinfo/%d", info->watch_pid, info->watch_fd);
	(void) pv_snprintf(info->file_fd, PV_SIZEOF_FILE_FD, "/proc/%u/fd/%d", info->watch_pid, info->watch_fd);

	memset(info->file_fdpath, 0, PV_SIZEOF_FILE_FDPATH);
	if (readlink(info->file_fd, info->file_fdpath, PV_SIZEOF_FILE_FDPATH - 1) < 0) {	/* flawfinder: ignore */
		/*
		 * flawfinder: memset() has put \0 at the end already, and
		 * we tell readlink() to use 1 byte less than the buffer
		 * length, so \0 termination is assured.  There is no
		 * mitigation for the risk of the link changing while we
		 * read it, but we are only reading from the destination,
		 * and then only if it's a block device - see filesize().
		 */
		if (!automatic)
			pv_error("%s %u: %s %d: %s",
				 _("pid"), info->watch_pid, _("fd"), info->watch_fd, strerror(errno));
		return 2;
	}

	if (!((0 == stat(info->file_fd, &(info->sb_fd)))
	      && (0 == lstat(info->file_fd, &(info->sb_fd_link))))) {
		if (!automatic)
			pv_error("%s %u: %s %d: %s: %s",
				 _("pid"),
				 info->watch_pid, _("fd"), info->watch_fd, info->file_fdpath, strerror(errno));
		return 3;
	}

	info->size = 0;

	if (!filesize(info)) {
		if (!automatic)
			pv_error("%s %u: %s %d: %s: %s",
				 _("pid"),
				 info->watch_pid,
				 _("fd"), info->watch_fd, info->file_fdpath, _("not a regular file or block device"));
		return 4;
	}

	return 0;
}
#endif

#ifdef __APPLE__
bool pv_watchfd_changed(pvwatchfd_t info)
{
	return true;
}
#else
/*
 * Return true if the given file descriptor has changed in some way since
 * we started looking at it (i.e. changed destination or permissions).
 */
bool pv_watchfd_changed(pvwatchfd_t info)
{
	struct stat sb_fd, sb_fd_link;

	if (NULL == info)
		return false;

	memset(&sb_fd, 0, sizeof(sb_fd));
	memset(&sb_fd_link, 0, sizeof(sb_fd_link));

	if ((0 == stat(info->file_fd, &sb_fd))
	    && (0 == lstat(info->file_fd, &sb_fd_link))) {
		if ((sb_fd.st_dev != info->sb_fd.st_dev)
		    || (sb_fd.st_ino != info->sb_fd.st_ino)
		    || (sb_fd_link.st_mode != info->sb_fd_link.st_mode)
		    ) {
			return true;
		}
	} else {
		return true;
	}

	return false;
}
#endif


/*
 * Return the current file position of the given file descriptor, or -1 if
 * the fd has closed or has changed in some way.
 */
off_t pv_watchfd_position(pvwatchfd_t info)
{
	off_t position;
#ifdef __APPLE__
	struct vnode_fdinfowithpath vnodeInfo = { };
	int32_t proc_fd = (int32_t) info->watch_fd;

	int size = proc_pidfdinfo(info->watch_pid, proc_fd,
				  PROC_PIDFDVNODEPATHINFO, &vnodeInfo,
				  PROC_PIDFDVNODEPATHINFO_SIZE);
	if (size != PROC_PIDFDVNODEPATHINFO_SIZE) {
		return -1;
	}

	position = (off_t) vnodeInfo.pfi.fi_offset;
#else
	long long pos_long;
	FILE *fptr;

	if (NULL == info)
		return -1;

	if (pv_watchfd_changed(info))
		return -1;

	fptr = fopen(info->file_fdinfo, "r");	/* flawfinder: ignore */
	/* flawfinder: trusted location (/proc). */
	if (NULL == fptr)
		return -1;
	pos_long = -1;
	position = -1;
	if (1 == fscanf(fptr, "pos: %lld", &pos_long)) {
		position = (off_t) pos_long;
	}
	(void) fclose(fptr);
#endif

	/*
	 * This debugging line can be quite noisy - turned off for now,
	 * uncomment if needed.
	 *
	 * debug("pid:%d fd:%d position:%lld", (int) (info->watch_pid), info->watch_fd, position);
	 */

	return position;
}


#ifdef __APPLE__
static int pidfds(pvstate_t state, unsigned int pid, struct proc_fdinfo **fds, int *count)
{
	int size_needed = proc_pidinfo(pid, PROC_PIDLISTFDS, 0, 0, 0);
	if (size_needed == -1) {
		pv_error("%s: unable to list pid fds: %s", _("pid"), strerror(errno));
		return -1;
	}

	*count = size_needed / PROC_PIDLISTFD_SIZE;

	*fds = (struct proc_fdinfo *) malloc(size_needed);
	if (*fds == NULL) {
		pv_error("%s: alloc failed: %s", _("pid"), strerror(errno));
		return -1;
	}
	memset(*fds, 0, size_needed);

	proc_pidinfo(pid, PROC_PIDLISTFDS, 0, *fds, size_needed);

	return 0;
}
#endif

/*@-compdestroy @ */
/*@-usereleased @ */
/*@-compdef @ */

/*
 * splint: extending or creating entries and then zeroing them makes splint
 * believe that data previously there may refer to pointers that are now
 * lost - but these are newly added array entries so there is no loss.
 */

/*
 * Extend the info array by one, returning false on error.
 */
static bool extend_info_array(int *array_length_ptr, pvwatchfd_t * info_array_ptr)
{
	int array_length = 0;
	struct pvwatchfd_s *info_array = NULL;
	struct pvwatchfd_s *new_info_array;

	array_length = *array_length_ptr;
	info_array = *info_array_ptr;

	array_length++;

	if (NULL == info_array) {
		new_info_array = malloc(array_length * sizeof(*info_array));
		if (NULL != new_info_array)
			memset(new_info_array, 0, array_length * sizeof(*info_array));
	} else {
		new_info_array = realloc(info_array, array_length * sizeof(*info_array));
		if (NULL != new_info_array)
			memset(&(new_info_array[array_length - 1]), 0, sizeof(*info_array));
	}

	if (NULL == new_info_array) {
		return false;
	}

	new_info_array[array_length - 1].unused = true;

	debug("%s", "extended info array");

	*info_array_ptr = new_info_array;
	*array_length_ptr = array_length;
	return true;
}

/*@+compdestroy @ */
/*@+usereleased @ */
/*@+compdef @ */


/*
 * Reset calculated values in the given watchfd info structure.
 */
void pv_reset_watchfd(pvwatchfd_t info)
{
	if (NULL == info)
		return;
	pv_reset_calc(&(info->calc));
	pv_reset_transfer(&(info->transfer));
	pv_reset_flags(&(info->flags));
	pv_reset_display(&(info->display));
}


/*
 * Free dynamically allocated areas in the given watchfd info structure.
 */
void pv_freecontents_watchfd(pvwatchfd_t info)
{
	if (NULL == info)
		return;
	pv_freecontents_calc(&(info->calc));
	pv_freecontents_transfer(&(info->transfer));
	pv_freecontents_display(&(info->display));
}


/*
 * Comparison function for qsort, to compare two pvwatchfd_s structures.
 */
static int pv_compare_watchfd(const void *a, const void *b)
{
	int fd_a, fd_b;

	fd_a = 0;
	fd_b = 0;
	if (NULL != a)
		fd_a = ((pvwatchfd_t) a)->watch_fd;
	if (NULL != b)
		fd_b = ((pvwatchfd_t) b)->watch_fd;

	if (fd_a < fd_b)
		return -1;
	if (fd_a > fd_b)
		return 1;
	return 0;
}


/*
 * Scan the given process and update the arrays with any new file
 * descriptors.  If "watch_fd" is not -1, then all other file descriptor
 * numbers will be ignored,
 *
 * Returns 0 on success, 1 if the process no longer exists or could not be
 * read, or 2 for a memory allocation error.
 */
int pv_watchpid_scanfds(pvstate_t state, pid_t watch_pid, int watch_fd, int *array_length_ptr,
			pvwatchfd_t * info_array_ptr)
{
	int array_length = 0;
	struct pvwatchfd_s *info_array = NULL;
	bool changes_made;

#ifdef __APPLE__
	struct proc_fdinfo *fd_infos = NULL;
	int fd_infos_count = 0;

	if (pidfds(state, watch_pid, &fd_infos, &fd_infos_count) != 0) {
		pv_error("%s: pidfds failed", _("pid"));
		return -1;
	}
#else
	char fd_dir[512];		 /* flawfinder: ignore - zeroed, bounded with pv_snprintf(). */
	DIR *dptr;
	struct dirent *d;

	memset(fd_dir, 0, sizeof(fd_dir));
	(void) pv_snprintf(fd_dir, sizeof(fd_dir), "/proc/%u/fd", watch_pid);

	dptr = opendir(fd_dir);
	if (NULL == dptr)
		return 1;
#endif

	array_length = *array_length_ptr;
	info_array = *info_array_ptr;

	changes_made = false;

#ifdef __APPLE__
	if (fd_infos_count < 1) {
		pv_error("%s: no fds found", _("pid"));
		return -1;
	}
	for (int i = 0; i < fd_infos_count; i++) {
#else
	while ((d = readdir(dptr)) != NULL) {
#endif
		int fd, check_idx, found_idx, use_idx, rc;
		off_t position_now;

		fd = -1;
#ifdef __APPLE__
		fd = fd_infos[i].proc_fd;
#else
		if (sscanf(d->d_name, "%d", &fd) != 1)
			continue;
#endif

		/* Skip if the fd is negative. */
		if (fd < 0)
			continue;

		/* If a watch_fd was specified, skip if this isn't it. */
		if (watch_fd >= 0 && watch_fd != fd)
			continue;

		/*
		 * Skip if this fd is already known to us.
		 */
		found_idx = -1;
		for (check_idx = 0; check_idx < array_length && NULL != info_array; check_idx++) {
			if (info_array[check_idx].unused)
				continue;
			if (info_array[check_idx].watch_fd != fd)
				continue;
			if (info_array[check_idx].closed) {
				/*
				 * If the fd is known but closed,
				 * immediately free it for re-use.
				 */
				info_array[check_idx].unused = true;
				info_array[check_idx].displayable = false;
				pv_freecontents_watchfd(&(info_array[check_idx]));
				continue;
			}
			found_idx = check_idx;
			break;
		}
		if (found_idx >= 0)
			continue;

		/*
		 * See if there's an empty slot we can re-use.
		 */
		use_idx = -1;
		for (check_idx = 0; check_idx < array_length && NULL != info_array; check_idx++) {
			if (info_array[check_idx].unused) {
				use_idx = check_idx;
				break;
			}
		}

		/*
		 * If there's no empty slot, extend the array.
		 */
		if (use_idx < 0) {
			if (!extend_info_array(array_length_ptr, info_array_ptr))
				return 2;
			array_length = *array_length_ptr;
			info_array = *info_array_ptr;
			use_idx = array_length - 1;
		}

		/* At this point, the array should exist. */
		if (NULL == info_array)
			return 2;

		debug("%s: %d => index %d", "found new fd", fd, use_idx);

		changes_made = true;

		/*
		 * Initialise the details of this new entry.
		 */
		memset(&(info_array[use_idx]), 0, sizeof(info_array[use_idx]));

		pv_reset_watchfd(&(info_array[use_idx]));
		info_array[use_idx].watch_pid = watch_pid;
		info_array[use_idx].watch_fd = fd;
		info_array[use_idx].closed = false;
		info_array[use_idx].unused = false;
		info_array[use_idx].displayable = true;

		/*
		 * Set the average rate window so that a new history buffer
		 * is allocated for this state.
		 */
		(void) pv_update_calc_average_rate_window(&(info_array[use_idx].calc),
							  state->control.average_rate_window);

#ifdef __APPLE__
		if (fd_infos[i].proc_fdtype != PROX_FDTYPE_VNODE) {
			continue;
		}
#endif
		/* Retrieve the details of this file descriptor. */
		rc = pv_watchfd_info(state, &(info_array[use_idx]), -1 == watch_fd ? true : false);

		/*
		 * Lookup failed - mark this slot as being free for re-use.
		 */
		if ((rc != 0) && (rc != 4)) {
			debug("%s %d: %s: %d", "fd", fd, "lookup failed - marking slot for re-use", use_idx);
			pv_freecontents_watchfd(&(info_array[use_idx]));
			info_array[use_idx].unused = true;
			info_array[use_idx].displayable = false;
			continue;
		}

		/*
		 * Not displayable - mark it as such so the main loop
		 * doesn't show it.
		 */
		if (rc != 0) {
			debug("%s %d: %s", "fd", fd, "marking as not displayable");
			info_array[use_idx].displayable = false;
		}

		/* Set the info display_name appropriately. */
		pv_watchpid_setname(state, &(info_array[use_idx]));

		/* Force the display to be re-parsed. */
		info_array[use_idx].flags.reparse_display = 1;

		pv_elapsedtime_read(&(info_array[use_idx].start_time));

		/*
		 * Set the starting position (and initial offset, for the
		 * display), if known, so that ETA and so on are calculated
		 * correctly.
		 */
		info_array[use_idx].display.initial_offset = 0;
		info_array[use_idx].position = 0;
		position_now = pv_watchfd_position(&(info_array[use_idx]));
		if (position_now >= 0) {
			info_array[use_idx].display.initial_offset = position_now;
			info_array[use_idx].position = position_now;
		}
	}


#ifdef __APPLE__
	free(fd_infos);
#else
	(void) closedir(dptr);
#endif

	/*
	 * If any changes were made (i.e. new file descriptors were found),
	 * sort the array so that file descriptors are always displayed in
	 * ascending numerical order.
	 */
	if (changes_made && NULL != info_array && array_length > 1)
		qsort(info_array, (size_t) array_length, sizeof(info_array[0]), pv_compare_watchfd);

	return 0;
}


/*
 * Set the display name for the given watched file descriptor, truncating at
 * the relevant places according to the current screen width.
 *
 * If more than one distinct PID is being watched, include the PID in the
 * name as well as the file descriptor number.
 *
 * If the file descriptor is pointing to a file under the current working
 * directory, show its relative path, not the full path.
 */
void pv_watchpid_setname(pvstate_t state, pvwatchfd_t info)
{
	size_t path_length, cwd_length;
	int max_display_length;
	char *file_fdpath;

	if (NULL == info)
		return;

	file_fdpath = info->file_fdpath;

	memset(info->display_name, 0, PV_SIZEOF_DISPLAY_NAME);

	path_length = strlen(info->file_fdpath);	/* flawfinder: ignore */
	cwd_length = strlen(state->status.cwd);	/* flawfinder: ignore */
	/* flawfinder: both strings are always \0 terminated. */
	if (cwd_length > 0 && path_length > cwd_length) {
		if (0 == strncmp(info->file_fdpath, state->status.cwd, cwd_length)) {
			file_fdpath += cwd_length + 1;
			path_length -= cwd_length + 1;
		}
	}

	max_display_length = (int) (state->control.width / 2) - 6;
	if (state->watchfd.multiple_pids)
		max_display_length -= 9;
	if (max_display_length >= (int) path_length) {
		if (state->watchfd.multiple_pids) {
			(void) pv_snprintf(info->display_name,
					   PV_SIZEOF_DISPLAY_NAME, "%8d:%4d:%.498s", (int) (info->watch_pid),
					   info->watch_fd, file_fdpath);
		} else {
			(void) pv_snprintf(info->display_name,
					   PV_SIZEOF_DISPLAY_NAME, "%4d:%.498s", info->watch_fd, file_fdpath);
		}
	} else {
		int prefix_length, suffix_length;

		prefix_length = max_display_length / 4;
		suffix_length = max_display_length - prefix_length - 3;

		if (state->watchfd.multiple_pids) {
			(void) pv_snprintf(info->display_name,
					   PV_SIZEOF_DISPLAY_NAME,
					   "%8d:%4d:%.*s...%.*s",
					   (int) (info->watch_pid), info->watch_fd, prefix_length,
					   file_fdpath, suffix_length, file_fdpath + path_length - suffix_length);
		} else {
			(void) pv_snprintf(info->display_name,
					   PV_SIZEOF_DISPLAY_NAME,
					   "%4d:%.*s...%.*s",
					   info->watch_fd, prefix_length,
					   file_fdpath, suffix_length, file_fdpath + path_length - suffix_length);
		}
	}

	debug("%s: %d: [%s]", "set name for fd", info->watch_fd, info->display_name);
}
