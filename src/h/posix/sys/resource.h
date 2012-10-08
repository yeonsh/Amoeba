/*	@(#)resource.h	1.2	94/04/06 16:56:42 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __RESOURCE_H__
#define __RESOURCE_H__

/* Not in any sense compliant with anything, but currently needed to compile
 * libajax under amoeba.
 */

#define PRIO_PROCESS	0

/* fake resource definitions */
struct rusage {
	int fake_rusage;
};
struct rlimit {
	int fake_rlimit;
};

#endif /* __RESOURCE_H__ */
