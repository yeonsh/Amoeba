/*	@(#)processor.h	1.4	96/02/27 10:30:21 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * FILE: processor.h -- fill in pre-processor magic for taskswitch.S, the
 * machine independant thread switcher.
 *
 * Author: Philip Shafer <phil@procyon.ics.Hawaii.Edu>
 *	   (Univerity of Hawai'i, Manoa) November 1990
 */

#ifndef __PROCESSOR_H__
#define __PROCESSOR_H__

#include "assyntax.h"
#include "fault.h"
#include "psr.h"
#include "offset.h"

#define LABEL(l)	l:

#define INDIRECT(x)	x

#define IMPORT(v)	GLOBLDIR v
#define EXPORT(f)	GLOBLDIR f

#define SWITCHTHREAD	GLNAME(switchthread)
#define FORKTHREAD	GLNAME(forkthread)
#define ENABLE		GLNAME(enable)
#define DISABLE		GLNAME(disable)

#define THREADPTR	GLNAME(curthread)

#define SP		%fp
#define REG		%o0
#define NEWTHREAD	%i0
#define FAKESP		%l0

#define LINK()					\
	save	%sp, -MINFRAME, %sp;		\
	mov	SP, %o1; /* Copy to new wndw */	\
	mov	%i0, %o0;			\
	save	%sp, -MINFRAME, %sp;

#define SAVEREGS()				\
	call	GLNAME(fast_flush_windows),0;	\
	nop;	/* delay slot */

#define SAVESP()				\
	LOAD( GLNAME(curthread), %o5 );		\
	st SP, [ %o5 ];

#ifdef NDEBUG
#define CHECKSTACK()
#else
#define CHECKSTACK()				\
	call	GLNAME(checkstack);		\
	nop;
#endif	/* NDEBUG */

#define MOVE(a,b)				\
	mov	a, b;

#define MOVEA(a,b)				\
	mov	a, b;

#define	MOVTOMEM( who, where )			\
	STORE( who, %l7, where );

#define	MOVTOSP( a )				\
	ld	[ a ], FAKESP;

#define RESTORE()				\
	mov	FAKESP, SP;

#define UNLINK()				\
	restore;

#define RETVAL0()				\
	mov	0, %i0;

#define RETURN()				\
	ret;					\
	restore;

#define PUSHARG()				\
	sub	FAKESP, MINFRAME, FAKESP;	\
	mov	%i1, %o1;			\
	mov	FAKESP, %o0;			\
	call	GLNAME(memmove),3;		\
	mov	MINFRAME, %o2;	/* delay */	\
	sub	FAKESP, MINFRAME, FAKESP;	\
	mov	SP, %o1;			\
	mov	FAKESP, %o0;			\
	call	GLNAME(memmove),3;		\
	mov	MINFRAME, %o2;	/* delay */	\
	mov	FAKESP, SP;			\
	restore;				\
	add	%sp, MINFRAME, %fp;

#define RETVAL1()				\
	mov	1, %i0;

#define FORKRETURN()				\
	ret;					\
	restore;

#define INTSON()				\
	PSR_LOWPIL( %o5 );

#define INTSOFF()				\
	PSR_HIGHPIL( %o5 );

#endif /* __PROCESSOR_H__ */
