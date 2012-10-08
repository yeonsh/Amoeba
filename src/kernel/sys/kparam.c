/*	@(#)kparam.c	1.1	96/02/27 14:22:06 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef NO_KERNEL_SECURITY

#include "amoeba.h"
#include "kparam.h"
#include "assert.h"
INIT_ASSERT

struct kparam kparam;

static int initialised;

void
initkparam()
{
	fillkparam(&kparam);
	initialised++;
}

kparam_port(p)
port *p;
{
	assert(initialised);
	*p = kparam.kp_port;
}

kparam_chkf(p)
port *p;
{
	assert(initialised);
	*p = kparam.kp_chkf;
}
#else /* NO_KERNEL_SECURITY */

#ifndef lint
static int nonempty;
#endif

#endif /* NO_KERNEL_SECURITY */
