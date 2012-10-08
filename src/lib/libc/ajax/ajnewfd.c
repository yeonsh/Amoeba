/*	@(#)ajnewfd.c	1.3	94/04/07 10:23:31 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Allocate a new file handle and descriptor block.
   The new handle is grabbed by setting it to NOFD (in NEWFD3);
   the new fd block is grabbed by setting its reference count to 1,
   but it is not locked (since its existence is only known to the
   current thread, there are no concurrent accesses possible). 

   last modified apr 22 93 Ron Visser
*/

#include "ajax.h"
#include "fdstuff.h"

int
_ajax_newfd(pf)
	struct fd **pf;
{
	int i;
	struct fd *f;
	
	FDINIT();
	i = 0;
	NEWFD3(i);
	for (f = _ajax_fd; ; ++f) {
		if (f >= &_ajax_fd[NOFILE]) {
			_ajax_fdmap[i] = NILFD;
			MU_UNLOCK(&_ajax_fdmu);
			ERR(EMFILE, "newfd: too many fd blocks");
		}
		if (f->fd_refcnt == 0)
			break;
	}
	f->fd_refcnt = 1;
	MU_UNLOCK(&_ajax_fdmu);
	mu_init(&f->fd_mu);
	*pf = f;
	return i;
}
