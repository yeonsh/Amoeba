/*	@(#)taskswitch.S	1.4	94/04/06 10:08:29 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "assyntax.h"
#include "processor.h"
AS_BEGIN

#ifndef MOVTOSP
#define MOVTOSP(x)	MOVEA(x, SP)
#endif /* MOVTOSP */

#ifndef MOVTOMEM
#define	MOVTOMEM(a, b)	MOVE(a, b)
#endif	/* MOVTOMEM */

#ifndef LF
#define LF ;
#endif /* LF */

IMPORT(THREADPTR)
EXPORT(SWITCHTHREAD)
EXPORT(FORKTHREAD)

#define SWITCH()		/* switch stacks */			    \
	LINK() LF		/* entry in this routine */		    \
	SAVEREGS() LF		/* save the registers */		    \
	SAVESP() LF		/* save the sp in the thread table */	    \
	CHECKSTACK() LF		/* check value of the sp */		    \
	MOVEA(NEWTHREAD, REG) LF	/* get the argument into a register */	\
	MOVTOMEM(REG, GLOBAL(THREADPTR)) LF				    \
				/* this argument is the new thread pointer */ \
	MOVTOSP(INDIRECT(REG))	/* and points to the new sp. get it */

LABEL(SWITCHTHREAD)
	SWITCH()		/* switch stacks */
	RESTORE()		/* restore the registers from this stack */
	UNLINK()		/* ok, all things are done */
	RETVAL0()		/* return 0 */
	RETURN()

LABEL(FORKTHREAD)
	SWITCH()		/* switch stacks */
	PUSHARG()		/* push arg on new stack */
	RETVAL1()		/* child returns 1 */
	FORKRETURN()		/* unlink and return */
