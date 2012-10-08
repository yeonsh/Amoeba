/*	@(#)exception.h	1.5	96/02/27 10:25:38 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__

/*
** Exceptions and light-weight signals.
*/
typedef long	signum;

/* Predefined light-weight signals */
#define SIG_TRANS	1	/* Transaction signal */

/* Exceptions */
#define EXC_ILL	((signum) -2)		/* Illegal instruction */
#define EXC_ODD	((signum) -3)		/* Mis-aligned reference */
#define EXC_MEM	((signum) -4)		/* Non-existent memory */
#define EXC_BPT	((signum) -5)		/* Breakpoint instruction */
#define EXC_INS	((signum) -6)		/* Undefined instruction */
#define EXC_DIV	((signum) -7)		/* Division by zero */
#define EXC_FPE	((signum) -8)		/* Floating exception */
#define EXC_ACC	((signum) -9)		/* Memory access control violation */
#define EXC_SYS	((signum) -10)		/* Bad system call */
#define EXC_ARG ((signum) -11)		/* Illegal operand (to instruction) */
#define EXC_EMU	((signum) -12)		/* System call emulation */
#define EXC_ABT	((signum) -13)		/* abort() called */
#define EXC_LAST EXC_ABT		/* Last defined exception value */

#define EXC_NONE ((signum) 0)		/* Terminator for list of exceptions */

/* Argument array element type for sys_sigvec */
typedef struct {
    signum	sv_type;	/* exception number */
    long	sv_pc;		/* PC of catcher routine */
    long	sv_arg3;	/* third arg to sv_pc() */
    long	sv_arg4;	/* fourth arg to sv_pc() */
} sig_vector;

#define	exc_name	_exc_name

char * exc_name _ARGS((signum));

#endif /* __EXCEPTION_H__ */
