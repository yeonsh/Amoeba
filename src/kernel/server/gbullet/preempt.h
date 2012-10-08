/*	@(#)preempt.h	1.1	96/02/27 14:07:33 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __PREEMPT_H__
#define	__PREEMPT_H__

/* The following macros must disable preemptive scheduling if it is present.
 * They don't suspend all scheduling.  You still block if you execute a
 * blocking primitive, but you won't be preempted.
 *
 * The property of these macros which is vital is that disable stacks the
 * current state and enable pops back to the previous state, not necessarily
 * reenabling preemption.
 */
#define	DISABLE_PREEMPTION
#define	ENABLE_PREEMPTION

#endif /* __PREEMPT_H__ */
