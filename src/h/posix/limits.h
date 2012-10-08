/*	@(#)limits.h	1.3	94/04/06 16:53:39 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __LIMITS_H__
#define __LIMITS_H__

/*
** Various implementation-dependent limits.
** This file is shared by Standard C and POSIX 1003.1.
*/

/*
** Definitions mandated by Standard C.
** These are all architecture- and possibly compiler-dependent.
** This version should work for "normal" machines with:
**	two's complement arithmetic
**	char = byte = 8 bits
**	short = 2 bytes
**	int = long = 4 bytes
** See article by Ed Keizer in comp.lang.minix for an explanation of why
** UINT_MIN and ULONG_MIN must be expressed as they are here.
** Floating point characteristics are in <float.h>.
*/

#define CHAR_BIT	8
#define SCHAR_MIN	(-128)
#define SCHAR_MAX	127
#define UCHAR_MAX	255

#define SHRT_MIN	(-0x7fff-1)
#define SHRT_MAX	0x7fff
#define USHRT_MAX	0xffff

#define INT_MIN		(-0x7fffffff-1)
#define INT_MAX		0x7fffffff
#define UINT_MAX	0xffffffff

#define LONG_MIN	(-0x7fffffffL-1L)
#define LONG_MAX	0x7fffffffL
#define ULONG_MAX	0xffffffffL

/* Assume signed characters */
#define CHAR_MAX                SCHAR_MAX
#define CHAR_MIN                SCHAR_MIN

/*
** Numerical Limits mandated by POSIX 1003.1-1988, Section 2.9.
** NB: string lengths are exclusive a terminating null byte.
*/

/* Table 2-3. Minimum Values */
/* The values listed here are mandated by POSIX */

#define _POSIX_ARG_MAX		4096
#define _POSIX_CHILD_MAX	6
#define _POSIX_LINK_MAX		8
#define _POSIX_MAX_CANON	255
#define _POSIX_MAX_INPUT	255
#define _POSIX_NAME_MAX		14
#define _POSIX_NGROUPS_MAX	0
#define _POSIX_OPEN_MAX		16
#define _POSIX_PATH_MAX		255
#define _POSIX_PIPE_BUF		512
#define _POSIX_SSIZE_MAX	32767
#define _POSIX_STREAM_MAX	8
#define _POSIX_TZNAME_MAX	3


/*
** Because of the Standard C name space pollution rules, the following
** POSIX symbols may not be defined when Standard C is used, unless the
** feature test macro _POSIX_SOURCE is defined.
** For non-Standard C usage, these symbols are always defined.
*/

#if !defined(__STDC__) || defined(_POSIX_SOURCE)

/* Table 2-4. Run-Time Increasable Values */

#define NGROUPS_MAX	0	/* Max number of extra group IDs */

/* Table 2-5. Run-Time Invariant Values (Possibly Indeterminate) */
/* Values that depend on system configuration are omitted; see sysconf() */

/* #define ARG_MAX	...	*/
/* #define CHILD_MAX	...	*/
#define OPEN_MAX	20	/* Max number of open files per process */

/* Table 2-6. Pathname Variable Values */
/* Values that depend on the server are omitted; see [f]pathconf() */

/* #define LINK_MAX	...	*/
/* #define MAX_CANON	...	*/
/* #define MAX_INPUT	...	*/
#define NAME_MAX	255	/* Max pathname component lenth (SOAP) */
#define PATH_MAX	255	/* Max pathname length (SOAP) */
/* #define PIPE_BUF	...	*/

/* Table 2-7. Invariant Value */
#define SSIZE_MAX	_POSIX_SSIZE_MAX

#endif

#endif /* __LIMITS_H__ */
