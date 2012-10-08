/*	@(#)exit.c	1.3	93/11/03 17:10:37 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include <stdio.h>
#include <stdlib.h>
#include "_ARGS.h"

#ifdef PROFILING
extern void prof_dump _ARGS((FILE *fp));
#endif

#define	NEXITS	32

void (*__functab[NEXITS]) _ARGS((void));
int __funccnt = 0;

extern void _exit _ARGS((int));

/* only flush output buffers when necessary */
void (*_clean) _ARGS((void)) = NULL;

static void
_calls()
{
	register int i = __funccnt;
	
	/* "Called in reversed order of their registration" */
	while (--i >= 0)
		(*__functab[i])();
}

void
exit(status)
int status;
{
#ifdef PROFILING
	prof_dump((FILE *) NULL);
#endif
	_calls();
	if (_clean) _clean();
	_exit(status);
	/*NOTREACHED*/
}
