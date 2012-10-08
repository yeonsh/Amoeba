/*	@(#)mymonitor.h	1.3	96/02/27 10:27:11 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __MYMONITOR_H__
#define __MYMONITOR_H__

#ifndef FILE
#include <stdio.h>
#endif

#ifndef DEBUG
#ifndef NDEBUG
#include <monitor.h>
#endif
#endif

#ifdef DEBUG
/* Heavy debugging: turn MON_EVENT calls into printf's */

extern int debugging; /* to be defined in the program being debugged */

#undef MON_EVENT

#define putnum(str, num) printf("%s: %d\n", str, num)

#define MON_EVENT(event) \
	{ if (debugging) { puts(event); fflush(stdout); } }
#define MON_NUMEVENT(event, num) \
	{ if (debugging) { putnum(event, num); fflush(stdout); } }

#define MON_ERROR(event) \
	{ puts(event); fflush(stdout); }
#define MON_NUMERROR(event, num) \
	{ putnum(event, num); fflush(stdout); }
#endif

#ifdef NDEBUG
/* No debugging: disable MON_EVENT calls completely */
#undef MON_EVENT
/* A null definition is given below */
#endif

#ifndef MON_EVENT
/* Make sure MON_EVENT calls don't cause errors */
#define MON_EVENT(event) { }
#endif

#ifndef MON_ERROR
#define MON_ERROR(event) MON_EVENT(event)
#endif

#ifndef MON_NUMEVENT
#define MON_NUMEVENT(event, num) MON_EVENT(event)
#endif

#ifndef MON_NUMERROR
#define MON_NUMERROR(event, num) MON_ERROR(event)
#endif

#endif /* __MYMONITOR_H__ */
