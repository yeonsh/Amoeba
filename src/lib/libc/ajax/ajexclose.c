/*	@(#)ajexclose.c	1.4	94/04/07 10:22:48 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Close close-on-exec files after successful execve
 * NB.  If a program is multithreaded then this probably won't work,
 *      especially if one of the threads is doing a read.
 */

#include "ajax.h"
#include "fdstuff.h"

void
_ajax_execlose()
{
	int i;
	struct fd *f;
	
	FDINIT();
	for (i = 0; i < NOFILE; ++i) {
		f = FD(i);
		if (CLEANFD(f) != NILFD) {
			if (GETCLEXEC(f)) {
TRACENUM("execlose victim", i);
				(void) _ajax_close(i, NILFD, 1);
			}
		}
	}
}
