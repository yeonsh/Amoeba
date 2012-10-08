/*	@(#)sysconf.c	1.2	94/04/07 09:53:57 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* sysconf() POSIX 4.8.1
 * last modified apr 22 93 Ron Visser
 *
 *	long int sysconf(int name);
 *
 *	POSIX allows some of the values in <limits.h> to be increased at
 *	run time.  The sysconf() function allows such values to be checked
 *	at run time.  AMOEBA does not use this facility - the run time
 *	limits are those given in <limits.h>.
 */

#include "ajax.h"
#include "stdio.h"
#include "unistd.h"
#include "time.h"
#include "class/sessvr.h"

/* this constant is taken from exec_file, and should be defined global
*/

#define NEEDSTACK       (13*1024)

long int sysconf(name)
int name;			/* property being inspected */
{
  switch(name) {
	case _SC_ARG_MAX:
		/* the rationale for this value is taken from the implementation
		 * of exec_file, a segment is mallocced and there must be 
		 * NEEDSTACK bytes left for the program.
		 */
		return (UINT_MAX - NEEDSTACK);
		/*return (long) _POSIX_ARG_MAX;*/

	case _SC_CHILD_MAX:
		return (long) MAXNPROC;

	case _SC_CLK_TCK:
		return (long) CLOCKS_PER_SEC;

	case _SC_NGROUPS_MAX:
		return (long) NGROUPS_MAX;

	case _SC_OPEN_MAX:
		return (long) OPEN_MAX;

	case _SC_STREAM_MAX:	/* STREAM_MAX should be defined with the same value */
		return (long) FOPEN_MAX;

	case _SC_TZNAME_MAX:
		return (long) _POSIX_TZNAME_MAX;

	case _SC_JOB_CONTROL:
		return -1L;			/* no job control */

	case _SC_SAVED_IDS:
		return -1L;			/* no saved uid/gid */

	case _SC_VERSION:
		return (long) _POSIX_VERSION;

	default:
		ERR(EINVAL,"sysconf: illegal property name");
  }
}
