/*	@(#)processor.h	1.4	94/04/06 16:01:10 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __PROCESSOR_H__
#define __PROCESSOR_H__

#include "assyntax.h"

#define LABEL(l)	l:

#define INDIRECT(x)	REGINDIR(x)

#define IMPORT(v)	GLOBLDIR v
#define EXPORT(f)	GLOBLDIR f

#define SWITCHTHREAD	_switchthread
#define FORKTHREAD	_forkthread
#define ENABLE		_enable
#define DISABLE		_disable

#define SP		sp
#define REG		a0
#define NEWTHREAD		REGOFFSETTED(a6,8)
#define THREADPTR		_curthread

#define LINK()		link a6,#0
#define UNLINK()	/* movl sp@+,a6 | optimization, see RESTORE */
#define SAVEREGS()	MOVEML d2-d7/a2-a5,AUTODEC(sp)/* d2-d7/a2-a5 (0x3F3C)*/
#define RESTORE()	MOVEML AUTOINC(sp),d2-d7/a2-a6/* d2-d7/a2-a6 (0x7CFC)*/

#define MOVEA(a, b)	MOVAL	a,b

#define SAVESP()	MOVAL GLOBAL(_curthread),a0; MOVL sp,REGINDIR(a0)

#define MOVE(a,b)	MOVL a,b
#define RETVAL0()	CLRL d0
#define RETURN()	rts
#define PUSHARG()	CLRL AUTODEC(sp)	/* as good as: movl a0,sp@- */
#define RETVAL1()	MOVL #1,d0

#define FORKRETURN()	MOVAL REGOFFSETTED(a6,4),a0; MOVAL REGINDIR(a6),a6;\
    			jmp REGINDIR(a0)

#ifdef NDEBUG
#define CHECKSTACK()
#else
#define CHECKSTACK()	jsr GLOBAL(_checkstack)
#endif

#endif /* __PROCESSOR_H__ */
