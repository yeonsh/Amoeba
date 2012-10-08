/*	@(#)ajgetsh.c	1.2	94/04/07 10:22:59 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Return a list of capabilities of shared files */

#include "ajax.h"
#include "fdstuff.h"

int
_ajax_getshared(nfd, fdlist, shared, maxnshared)
	int nfd;
	int fdlist[];
	capability shared[];
	int maxnshared;
{
	int i, j;
	struct fd *f;
	int nshared = 0;
	
	FDINIT();
	for (i = 0; i < nfd; ++i) {
		if (fdlist[i] < 0 || fdlist[i] >= NOFILE)
			continue;
		f = CLEANFD(FD(fdlist[i]));
		if (f == NILFD || !(f->fd_flags & FUNLINK))
			continue;
		if (f->fd_refcnt > 1) {
			for (j = 0; j < i; ++j) {
				if (fdlist[j] < 0 || fdlist[j] >= NOFILE)
					continue;
				if (f == CLEANFD(FD(fdlist[j])))
					goto next; /* 2-level continue */
			}
		}
		if (nshared < maxnshared)
			shared[nshared++] = f->fd_cap;
	next:	; /* used for 2-level continue */
	}
	return nshared;
}
