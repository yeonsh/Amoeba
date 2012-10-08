/*	@(#)noether.c	1.3	96/02/27 13:49:01 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "randseed.h"
#include "string.h"

#ifndef NO_KERNEL_SECURITY
#include "amoeba.h"
#include "kparam.h"
 
fillkparam(kp)
struct kparam *kp;
{
    strncpy((char *) &kp->kp_port, "system", 6);
    strncpy((char *) &kp->kp_chkf, "system", 6);
}

#else /* NO_KERNEL_SECURITY */


kernel_port(p)
char *p;
{
    strncpy(p, "system", 6);
}
#endif /* NO_KERNEL_SECURITY */

void
init_noether()
{
    randseed((unsigned long) 0, 16, RANDSEED_RANDOM);
}
