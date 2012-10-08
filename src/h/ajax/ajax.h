/*	@(#)ajax.h	1.11	96/03/04 13:22:21 */
/*
 * Copyright 1996 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __AJAX_H__
#define __AJAX_H__

/* Ajax -- the ultimate Amoeba Unix emulation */

/* TO DO: reorganize debugging and tracing help */

/* This file is included by almost all implementation modules, and
   contains many shared definitions. */

/* Naming conventions:
   All externally visible names that are used internally only, are
   prefixed with _ajax_.  All externally visible names that are part of
   the interface are prefixed with ajax_, unless they are part of the
   emulated Unix interface (e.g., Unix system calls and 'errno').
   Names that aren't externally visible (e.g., types and macros) don't
   have a prefix.  Struct member names and flag names pertaining to
   particular structs have a prefix deribed from the struct name (e.g.,
   members of struct fd have prefix fd_, their flags (should) have
   prefix FD_). */

/* Amoeba includes */
#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "capset.h"
#include "direct/direct.h"
#include "soap/soap.h"
#include "bullet/bullet.h"
#include "file.h"

#include "module/stdcmd.h"
#include "module/prv.h"
#include "module/mutex.h"
#include "module/name.h"
#include "module/direct.h"
#include "module/rnd.h"
#include "module/tod.h"
#include "module/proc.h"
#include "module/signals.h"

/* Posix includes */
#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE /* to get NAME_MAX and OPEN_MAX with __STDC__ */
#endif
#include "posix/sys/types.h"
#include "posix/sys/wait.h"
#include "signal.h"
#include <errno.h>
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "posix/limits.h"

/* Ajax includes */
#include "forklevel.h"
#include "fdstuff.h"

/* Ajax internal functions */
extern errstat _ajax_append	_ARGS((capability *, const char*, capability*));
extern char *  _ajax_breakpath	_ARGS((capability *, const char*, capability*));
extern int     _ajax_capstat	_ARGS(());
extern long    _ajax_conf	_ARGS((int));
extern int     _ajax_error	_ARGS((errstat));
extern void    _ajax_execlose	_ARGS((void));
extern errstat _ajax_getowner	_ARGS((capability *));
extern int     _ajax_session_owner _ARGS((capability *owner));
extern int     _ajax_getshared	_ARGS((int, int [], capability [], int));
extern int *   _ajax_idmapping	_ARGS((void));
extern errstat _ajax_lookup	_ARGS((capability *, const char *, capability *,
				       int *, long *));
extern void    _ajax_makecapv	_ARGS((int, int [], struct caplist *,
				       int, char *, int));
extern int     _ajax_modemtime	_ARGS((capability *, int *, long *));
extern int     _ajax_newfd	_ARGS((struct fd **));
extern capability *_ajax_origin	_ARGS((capability *, const char *));
extern errstat _ajax_csorigin	_ARGS((capability *, const char *, capset *));
extern int     _ajax_puts	_ARGS((char *));
extern int     _ajax_putnum	_ARGS((char *, int));

extern errstat _ajax_install	_ARGS((int, capability *, const char *,
				       capability *));
extern void    _ajax_setstdcap	_ARGS((int, capability *));
extern errstat _ajax_time	_ARGS((long *pt));
extern void    _ajax_fd_watchdog _ARGS((void));
extern int     _ajax_close	_ARGS((int, struct fd *, int));
extern int     _ajax_commitfd	_ARGS((struct fd *));
extern void    _ajax_fdinit	_ARGS((void));
extern int     _ajax_unlink	_ARGS((capability *, const char *));


/* Random useful things */
#ifndef NULL
#define NULL 0
#endif
#ifndef __STDC__
#define const /* const */
#endif
#define NILCAP ((capability *)NULL)
#define NILPD ((process_d *)NULL)
#define TICKSPERSEC 1000 /* Amoeba timer granularity (mu_trylock) */
#ifndef CAPZERO
#define CAPZERO(cap) (void) memset((_VOIDSTAR) (cap), 0, (size_t) CAPSIZE)
#endif
#define CAPEQ(a, b) (memcmp((_VOIDSTAR)(a), (_VOIDSTAR)(b), (size_t) CAPSIZE) == 0)
#define SUPER_USER	((uid_t) 0)

/* environment variable and template to pass user and group ids */
#define ID_ENV		"_IDS"
#define ID_TEMPL	"%d:%d:%d:%d"

/* Variables defined by Ajax */
extern int errno;		/* Error returned by last failing call */
extern char *_ajax_error_msg;	/* Error string from last failing call */
extern int   _ajax_umask;	/* Current umask value */
extern uid_t _ajax_uid;		/* real user id */
extern uid_t _ajax_euid;	/* effective user id */
extern uid_t _ajax_gid;		/* real group id */
extern uid_t _ajax_egid;	/* effective group id */

/* Mutex/signal macros */
#define MU_LOCK(pmu) (mu_lock(pmu), sig_block())
#define MU_UNLOCK(pmu) (mu_unlock(pmu), sig_unblock())

/* Fake uid and gid values */
#define FAKE_UID 1
#define FAKE_GID 1

/* Macro to print an error */
#ifdef DEBUG
#define PUTS(str) _ajax_puts(str)
#define PUTNUM(str, num) _ajax_putnum(str, num)
#else
#ifndef NDEBUG
#define PUTS(str) (getenv("AJAX_DEBUG") && _ajax_puts(str))
#define PUTNUM(str, num) (getenv("AJAX_DEBUG") && _ajax_putnum(str, num))
#endif
#endif

#ifndef PUTS
#define PUTS(str) /*empty*/
#endif

#ifndef PUTNUM
#define PUTNUM(str, num) /*empty*/
#endif

/* Macros to return an error */
#define ERR(code, msg) { ERR2(code, msg, -1); }
#ifdef NDEBUG
#define ERR2(code, msg, retval) { errno = code; return retval; }
#else
#define ERR2(code, msg, retval) { \
	errno = code; \
	_ajax_error_msg = msg; \
	PUTS(_ajax_error_msg); \
	return retval; \
}
#endif

/* Tracing help */
#ifndef NDEBUG
#define TRACELINE() (getenv("AJAX_TRACE") && _ajax_putnum(__FILE__, __LINE__))
#define TRACE(str) (getenv("AJAX_TRACE") && _ajax_puts(str))
#define TRACENUM(str, num) (getenv("AJAX_TRACE") && _ajax_putnum(str, num))
#endif

#ifndef TRACELINE
#define TRACELINE() /*empty*/
#endif

#ifndef TRACE
#define TRACE(str) 0 /*empty*/
#endif

#ifndef TRACENUM
#define TRACENUM(str, num) 0 /*empty*/
#endif

#endif /* __AJAX_H__ */
