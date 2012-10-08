/*	@(#)processor.h	1.3	94/04/06 15:56:12 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Fill in pre-processor magic for taskswitch.S, the
 * machine independant thread switcher.
 *
 * Probably the most complicated to explain.
 * Take some time over this one.
 * Keep taskswitch.S next to it.
 */

#ifndef __PROCESSOR_H__
#define __PROCESSOR_H__


/* Place label l */
#define LABEL(l)	l:

/* Instruction separator */
#define LF		;

/*
 * Assembly syntax for indirect through register:
 * On vaxen it is (x), on some mc68000 assemblers x@
 */
#define INDIRECT(x)

/* Import and export symbols */
#define IMPORT(v)	.globl v
#define EXPORT(f)	.globl f

/* Names of routines, do not change the names, but only the syntax */
#define SWITCHTHREAD	_switchthread
#define FORKTHREAD	_forkthread
#define ENABLE		_enable
#define DISABLE		_disable

/* External global variable, same as above */
#define THREADPTR	_curthread

#define REG		/* A scratch register */

#define NEWTHREAD	/* First parameter of SWITCHTHREAD */

#define LINK()		/* Routine entry */

#define UNLINK()	/* Routine exit */

#define SAVEREGS()	/* Save registers */

#define RESTORE()	/* restore registers */

#define SAVESP()	/* Save stackpointer to indirect THREADPTR */

#define MOVE(a,b)	/* Move a (argument) to b (register) */

#define MOVEA(a,b)	/* Move a (argument) to b (address register) */

#define	MOVTOSP( a )	/* Move a to stack pointer */

#define	MOVTOMEM( who, where )	/* Move who to where (memory) */

#define RETVAL0()	/* Move 0 to returnvalue area */

#define RETVAL1()	/* Move 1 to returnvalue area */

#define RETURN()	/* Actually return from subroutine */

#define PUSHARG()	/* push arguments on new stack */

#define FORKRETURN()	/* unlink and return */

#ifdef NDEBUG
#define CHECKSTACK()
#else
#define CHECKSTACK()	/* call routine checkstack, no parameters */
#endif	/* NDEBUG */

#endif /* __PROCESSOR_H__ */
