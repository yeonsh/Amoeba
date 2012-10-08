/*	@(#)sun3ether.c	1.7	96/02/27 13:53:15 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "machdep.h"
#include "global.h"
#include "assert.h"
INIT_ASSERT
#include "map.h"
#include "randseed.h"
#include "sys/flip/eth_if.h"
#include "sys/debug.h"

#define PROMADDR 0L
#define EAOFFSET 2

/*ARGSUSED*/
sunethaddr(param, eaddr)
char *param;
char *eaddr;
{
        register i;

        for (i=0;i<6;i++) {
                eaddr[i] = fctlb(PROMADDR+EAOFFSET+i);
		if (i>2)
			randseed((unsigned long) eaddr[i], 8, RANDSEED_HOST);
	}
        return 1;
}

extern char *aalloc();

/* ARGSUSED */
static unsigned long
busaddr(addr, size)
long addr;
uint32 size;
{
        return(addr - IVECBASE);
}

#include "interrupt.h"
#include "glainfo.h"

#define NEL(ar) (sizeof(ar)/sizeof(ar[0]))

lai_t la_info[]= {
        { IOBASE+0x12000,       {27},     4, 	6,      6,      0,      0, 100,
          sunethaddr,   IOBASE+0x12000,         aalloc,         busaddr },
};

#include "ethif.h"

int la_alloc();
int la_init(); void la_send();
int la_setmc(), la_stop();

hei_t heilist[] = {
{ "lance", NEL(la_info), la_alloc, la_init, la_send, la_setmc, la_stop },
{ 0 }
};


#ifndef NO_KERNEL_SECURITY

#include "kparam.h"
 
fillkparam(kp)
struct kparam *kp;
{
     /* Nothing in the prom so use ethernet address */
     DPRINTF(1, ("Using insecure kernel capability\n"));
     eth_info(0, (char *) &kp->kp_port, (int *) 0, (int *) 0);
     eth_info(0, (char *) &kp->kp_chkf, (int *) 0, (int *) 0);
}

#else
/*
 * Temporary hack
 */
kernel_port(p)
char *p;
{
    eth_info(0, p, (int *) 0, (int *) 0);
}
#endif
