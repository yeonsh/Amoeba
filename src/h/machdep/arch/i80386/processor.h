/*	@(#)processor.h	1.5	94/04/06 15:59:37 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __PROCESSOR_H__
#define __PROCESSOR_H__

#define	GLOBAL(x)	CONTENT(x)

#define IMPORT(v)
#define EXPORT(f)	GLOBL f
#define LABEL(l)	l:

#define SWITCHTHREAD	GLNAME(switchthread)
#define FORKTHREAD	GLNAME(forkthread)
#define ENABLE		GLNAME(enable)
#define DISABLE		GLNAME(disable)

#undef	SP
#define SP		ESP
#define REG		EBX
#define NEWTHREAD	REGOFF(8,EBP)	/* check out this offset */
#define THREADPTR	GLNAME(curthread)
#define INDIRECT(x)	REGIND(x)     /* counting on this being a register */

#define LINK()		PUSH_L (EBP); MOV_L (ESP,EBP)
#define UNLINK()	MOV_L (EBP,ESP); POP_L (EBP)
#define SAVEREGS()	PUSHA_L ; NOP
#define RESTORE()	CALL (GLNAME(ndp_switchthread)); POPA_L; NOP
#define SAVESP()	MOV_L (GLOBAL(THREADPTR),EBX); MOV_L (ESP,REGIND(EBX))
#define MOVE(a,b)	MOV_L (a,b) /* one of these must be a register */
#define MOVEA(a,b)	MOV_L (a,b) /* one of these must be a register */
#define RETVAL0()	XOR_L (EAX,EAX)
#define RETURN()	RET
#define PUSHARG()	PUSH_L (CONST(0))
#define RETVAL1()	MOV_L (CONST(1),EAX)
#define FORKRETURN()	PUSH_L (REGOFF(4,EBP)); MOV_L (REGIND(EBP),EBP); RET

#ifdef NDEBUG
#define CHECKSTACK()
#else
#define CHECKSTACK()	CALL (GLNAME(checkstack))
#endif

#endif /* __PROCESSOR_H__ */
