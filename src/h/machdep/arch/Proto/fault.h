/*	@(#)fault.h	1.2	94/04/06 15:55:45 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __FAULT_H__
#define __FAULT_H__

/*
 * definition of fault frame the transition from user to kernel mode
 * leaves on the stack. This could be variable sized.
 */

struct fault {
	long	some_thing_or_other;
};

/*
 * The actual interface is the thread_ustate typedef. This should be
 * worst case sized.
 */

typedef struct fault thread_ustate;
#define USTATE_SIZE_MIN	(sizeof(struct fault))
#define USTATE_SIZE_MAX USTATE_SIZE_MIN
#define USTATE_SIZE(us)	USTATE_SIZE_MIN

/*
 * String defining architecture. Ends up in binaries, process descriptors.
 */
#define ARCHITECTURE "Proto"

#endif /* __FAULT_H__ */
