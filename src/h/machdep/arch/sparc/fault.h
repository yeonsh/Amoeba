/*	@(#)fault.h	1.3	96/02/27 10:30:14 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * FILE: fault.h -- define various stack frame and trap constants
 *
 * Author: Philip Shafer <phil@procyon.ics.Hawaii.Edu>
 *	   (Univerity of Hawai'i, Manoa) November 1990
 */

#ifndef _FAULT_H
#define _FAULT_H

/* We don't use this, but folks who #include us expect to see it's contents */
#include "defarch.h"

/*
 * Descriptions of traps
 */

#define	TRAPV_RESET		0	/* Full reset (untrapable, eh?) */
#define	TRAPV_INSN_ACCESS	1	/* instruction_access_exception */
#define	TRAPV_ILLEGAL_INSN	2	/* illegal_instruction */
#define	TRAPV_PRIV_INSN		3	/* priviledged_instruction */
#define	TRAPV_FP_DISB		4	/* fp_disabled */
#define	TRAPV_CP_DISB		36	/* cp_disabled */
#define	TRAPV_WND_OVER		5	/* window_overflow */
#define	TRAPV_WND_UNDER		6	/* window_underflow */
#define	TRAPV_ALIGNMENT		7	/* mem_address_not_aligned */
#define	TRAPV_FP_EXPT		8	/* fp_exception */
#define	TRAPV_CP_EXPT		40	/* cp_exception */
#define	TRAPV_DATA_ACCESS	9	/* data_access_exception */
#define	TRAPV_TAG_OVER		10	/* tag_overflow */
#define	TRAPV_INTBASE		16	/* interrupt_level_+x */
#define TRAPV_INTFIRST		17	/* Lowest interrupt */
#define TRAPV_INTLAST		31	/* Highest interrupt */

/* These other traps are relative to TRAPV_TRAPBASE */
#define	TRAPV_TRAPBASE		128	/* trap_instruction base (Ticc) */
#define TRAPV_BPTFLT		1	/* Breakpoint */
#define TRAPV_DIVZERO		2	/* Division by zero */
#define TRAPV_SYSCALL		3	/* Syscall trap */

#define INTERRUPT( x )		((x) + TRAPV_INTBASE )

#define NUM_TRAPV		256	/* Number of traps in table */
#define NINSN_TRAPV		4	/* The four instructions in table */

#ifndef _ASM

struct trap_vector {
    int		tv_insn[ NINSN_TRAPV ];	/* Actually code, not data */
};
#define	SIZE_TRAPV	sizeof( struct trap_vector )

#endif	/* _ASM */

#define TBR_SHIFT		4	/* Shift to get trap # from tbr */
#define TBR_MASK		0xFF0	/* Mask to get trap # from tbr */

#define ARCH_NUMREGS		8	/* Number of registers per type */
#define ARCH_MAXREGARGS		6	/* Number of register arguments */
#define ARCH_NUMFPREGS		32	/* Number of floating point regs */
#define ARCH_FPQSIZE		16	/* Max outstanding fp operations */

#ifndef _ASM

struct stack_frame {
    int		sf_local[ ARCH_NUMREGS ];	/* %l0-%l7 aka %r16-%r23 */
    int		sf_in[ ARCH_NUMREGS ];		/* %i0-%i7 aka %r24-%r31 */
    int	       	sf_aggregate;			/* Pointer to aggr ret val */
    int		sf_regsave[ ARCH_MAXREGARGS ];	/* %i0-%i6 for varargs */
    int		sf_addarg[ 1 ];		       	/* Any additional arguments */
};

typedef	struct thread_ustate {
    struct stack_frame	tu_sf;			/* Minimum frame */
    int		tu_global[ ARCH_NUMREGS ];	/* %g0-%g7 aka %r0-%r7 */
    int		tu_fsr;				/* Floating status register */
    int		tu_qsize;			/* FP Queue size */
    int		tu_float[ ARCH_NUMFPREGS ];	/* Floating point registers */
    int		tu_fqueue[ ARCH_FPQSIZE ];	/* FP Queue */
} thread_ustate;

/*
 * Defines to allow us to ignore stack_frame while dealing with thread_ustate
 */
#define tu_local tu_sf.sf_local
#define tu_in tu_sf.sf_in
#define tu_aggregate tu_sf.sf_aggregate
#define tu_regsave tu_sf.sf_regsave
#define tu_addarg tu_sf.sf_addarg

#define USTATE_SIZE_MIN	sizeof(thread_ustate)
#define USTATE_SIZE_MAX	sizeof(thread_ustate)
#define USTATE_SIZE(us)	sizeof(thread_ustate)

#define	MINFRAME	sizeof( struct stack_frame )

#else	/* _ASM */

#define MINFRAME	SIZEOF_STACK_FRAME	/* Defined in "offset.h" */

#endif	/* _ASM */

/*
 * Defines for our usage whilst handling traps and such
 */
#ifdef	_ASM
#define TU_PSR		(SF_LOCAL+4*0)
#define TU_PC		(SF_LOCAL+4*1)
#define TU_NPC		(SF_LOCAL+4*2)
#define TU_TBR		(SF_LOCAL+4*3)
#define TU_Y		(SF_LOCAL+4*4)
#define TU_PID		(SF_LOCAL+4*5)
#define TU_FP		(SF_IN+4*6)
#define TU_RETADDR 	(SF_IN+4*7)
#else	/* _ASM */
#define tu_psr		tu_local[ 0 ]
#define tu_pc		tu_local[ 1 ]
#define tu_npc		tu_local[ 2 ]
#define tu_tbr		tu_local[ 3 ]
#define tu_y		tu_local[ 4 ]
#define tu_pid		tu_local[ 5 ]
#define tu_fp		tu_in[ 6 ]
#define tu_retaddr	tu_in[ 7 ]

#define sf_fp		sf_in[ 6 ]
#define sf_retaddr	sf_in[ 7 ]
#endif	/* _ASM */

#define MAKE_NPC( x )  	( x + 4 )	/* First next instruction */

#endif	/* _FAULT_H */
