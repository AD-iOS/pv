/*
 * Standard definitions to make use of the config.h values set by the
 * `configure' script.  Include this after config.h and before anything
 * else.
 *
 * Copyright 2002-2008, 2010, 2012-2015, 2017, 2021, 2023-2025 Andrew Wood
 *
 * License GPLv3+: GNU GPL version 3 or later; see `docs/COPYING'.
 */
 
#if ENABLE_NLS
# ifdef HAVE_LIBINTL_H
#  include <libintl.h>
# endif
# ifdef HAVE_LOCALE_H
#  include <locale.h>
# endif
# define _(String)	gettext (String)
# define N_(String)	(String)
#else
# define _(String) (String)
# define N_(String) (String)
#endif

#undef CURSOR_ANSWERBACK_BYTE_BY_BYTE
#ifndef _AIX
#define CURSOR_ANSWERBACK_BYTE_BY_BYTE 1
#endif

/* Boolean type support */
#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#else
# ifndef HAVE__BOOL
#  ifdef __cplusplus
typedef bool _Bool;
#  else
#   define _Bool signed char
#  endif
# endif
# define bool _Bool
# define false 0
# define true 1
# define __bool_true_false_are_defined 1
#endif

/* Whether "--remote" and "--query" should be available. */
#undef PV_REMOTE_CONTROL
#if HAVE_DECL_SA_SIGINFO
# if SIGINFO_PROVIDES_PID
#define PV_REMOTE_CONTROL 1
# endif
#endif
