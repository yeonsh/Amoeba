/*	@(#)ajconf.c	1.2	94/04/07 10:22:29 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* fpathconf() POSIX 5.7.1
 * last modified apr 22 93 Ron Visser
 */

#include "ajax.h"
#include "sys/stat.h"
#include "limits.h"
#include "unistd.h"
#include "unistd.h"
#include "tty/term.h"

long
_ajax_conf(name)
int name;
{
/* This function handles the simple commands that pathconf() and
 * fpathconf() have in common.
 */

  switch (name) {
#ifdef NOT_YET
	case _PC_LINK_MAX:
		/* Fstat the file.  If that fails, return -1. */
		if (fstat(fd, &stbuf) != 0) return(-1L);
		if (S_ISDIR(stbuf.st_mode))
			return(1L);	/* no links to directories */
		else
			return( (long) LINK_MAX);

	case _PC_MAX_CANON:
		return( (long) MAX_CANON);

	case _PC_MAX_INPUT:
		return( (long) MAX_INPUT);
#endif

	case _PC_NAME_MAX:
		return( (long) NAME_MAX);

	case _PC_PATH_MAX:
		return( (long) PATH_MAX);

	case _PC_CHOWN_RESTRICTED:
		return( (long) _POSIX_CHOWN_RESTRICTED);

	case _PC_NO_TRUNC:
		return( (long) _POSIX_NO_TRUNC);

	case _PC_VDISABLE:
		return( (long) _POSIX_VDISABLE);

	default:
		ERR(EINVAL, "[f]pathconf: illegal property name");
  }
}
