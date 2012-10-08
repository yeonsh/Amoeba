/*	@(#)_exit.c	1.4	96/02/27 11:04:52 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* _exit(2) system call emulation */
/* TO DO: move _ajax_on_exit to a separate file */

#include "ajax.h"
#include "module/proc.h"

void
_exit(n)
	int n;
{
	_ajax_call_exitprocs();
	exitprocess(n);
	/*NOTREACHED*/
}

/* There's a problem here -- exitproc really should be defined as
   'pointer to function returning exitproc', however, such recursive
   types don't exist in C, so we make it return a char pointer,
   hope the best, and ignore the lint complaint.
   We'll fix it when we get bitten, someday... */

typedef char * (*exitproc)();

static exitproc chainstart;

_ajax_call_exitprocs()
{
	exitproc proc;
	
	proc = chainstart;
	chainstart = (exitproc) NULL;
	while (proc != (exitproc) NULL)
		proc = (exitproc) (*proc)();
		/* Ignore lint complaint on this line -- see above */
}

exitproc
_ajax_on_exit(proc)
	exitproc proc;
{
	exitproc prev;
	
	prev = chainstart;
	chainstart = proc;
	return prev;
}
