/*	@(#)kernelport.c	1.6	96/02/27 13:51:39 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * kernelport.c
 *
 * Get the port to which this kernel is supposed to listen. For now it
 * just listens to the ethernet address of the first ethernet interface
 * it can find, but in the near future it should get it from CMOS, ROM,
 * BOOT, or something like that.
 *
 * Author:
 *	Leendert van Doorn
 */
#ifndef NONET
#include <amoeba.h>
#include <ethif.h>
#include <string.h>

#ifndef NO_KERNEL_SECURITY

#include "kparam.h"
#include "sys/debug.h"
 
fillkparam(kp)
struct kparam *kp;
{
     /* Nothing in the prom so use ethernet address */
     DPRINTF(1, ("Using insecure kernel capability\n"));
     eth_info(0, (char *) &kp->kp_port, (int *) 0, (int *) 0);
     eth_info(0, (char *) &kp->kp_chkf, (int *) 0, (int *) 0);
}

#else
extern sei_t seilist[];
extern uint16 eth_maxeth;

kernel_port(p)
    char *p;
{
    if (eth_maxeth >= 1)
	(void) memmove((_VOIDSTAR) p, (_VOIDSTAR) seilist[0].sei_ifaddr,
		       sizeof(port));
    else
	(void) memmove((_VOIDSTAR) p, (_VOIDSTAR) "system", sizeof(port));
}
#endif /* NO_KERNEL_SECURITY */
#endif /* NONET */
