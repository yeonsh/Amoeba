/*	@(#)f30ether.c	1.7	96/02/27 13:47:43 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <string.h>
#include "amoeba.h"
#include "sys/proto.h"
#include "machdep.h"
#include "global.h"
#include "memaccess.h"
#include "../../dev/generic/filtabyte.h"
#include "flip/eth_if.h"
#include "assert.h"
INIT_ASSERT
#include "map.h"
#include "randseed.h"
#include "sys/debug.h"

#define DVADR_LANCE       0xFEF80000
#define LANCE_MEM_BASE	  0xFEF00000
#define LANCE_MEM_SIZE	  0x00010000

/************************************************************************/
/*      NETWORK                                                         */
/************************************************************************/

/* ARGSUSED */
static unsigned long
force30_ether_addr_conv(a, size)
unsigned long a;
uint32 size;
{
        if (a <LANCE_MEM_BASE)
                return 0x1000000;
        return (a - LANCE_MEM_BASE);
}

/*ARGSUSED*/
static unsigned long
force30_filt_ether_addr_conv(a, size)
phys_bytes a;
uint32 size;
{
	return a;
}

#include "interrupt.h"
#include "glainfo.h"


static
char *piggy_alloc(size,align)
int size;
int align;
{
	phys_bytes b;
	static phys_bytes start;

	if(align == 0) align = sizeof(long);
	if(start == 0) start = LANCE_MEM_BASE;
	if (start%align)
		start = start + align - start % align;
        (void) memset((_VOIDSTAR) start, 0, (size_t) size);
	b = start;
	start += size;
        compare(start, <=, LANCE_MEM_BASE+LANCE_MEM_SIZE);
        return (char *) b;
}

/*ARGSUSED*/
static
char *uncached_alloc(size, align)
int size;
int align;
{
        char *base;

        base = (char *) getblk((vir_bytes) size);
	assert(base != 0);
        disable_caching((vir_bytes) base, (vir_bytes) size);
        (void) memset((_VOIDSTAR) base, 0, (size_t) size);
        return base;
}

#define VMEMEMBASE	0xFC000000
#define VMEMEMSIZE	0x100000

static
char *vme_alloc(size,align)
int size;
int align;
{
	phys_bytes b;
	static phys_bytes start;

	if(align == 0) align = sizeof(long);
	if(start == 0) start = VMEMEMBASE;
	if (start%align)
		start = start + align - start % align;
        (void) memset((_VOIDSTAR) start, 0, (size_t) size);
	b = start;
	start += size;
        compare(start, <=, VMEMEMBASE+VMEMEMSIZE);
        return (char *) b;
}

static unsigned long
vme_ether_addr_conv(a)
unsigned long a;
{
	if (a >= VMEMEMBASE && a < VMEMEMBASE+VMEMEMSIZE)
		return a - VMEMEMBASE;
	return 0x1000000;
}

static
etheraddr(dev, eaddr)
char *dev;
register char *eaddr;
{
	register i;
	register char *p;

	p = (char *) dev;
	for (i=0; i<6; i++) {
		if (i>2)	/* First bytes always the same */
			randseed((unsigned long) *p, 8, RANDSEED_HOST);
		*eaddr = *p;
		eaddr++;
		p += 4;		/* Use only EPROM 0 */
	}
	return 1;
}

extern int filtaddr();

static
memfiltaddr(dev, eaddr)
char *dev;
register char *eaddr;
{
	if (probe((vir_bytes) VMEMEMBASE, 1))
		return filtaddr((struct filtabyte *) dev, eaddr);
	return 0;
}

static
nomemfiltaddr(dev, eaddr)
char *dev;
register char *eaddr;
{
	if (!probe((vir_bytes) VMEMEMBASE, 1))
		return filtaddr((struct filtabyte *) dev, eaddr);
	return 0;
}

#define ROM_UPPER_HALF	0xFF020000

#define NEL(ar) (sizeof(ar)/sizeof(ar[0]))
lai_t la_info[]= {
	{ DVADR_LANCE,	{246},	4,	5,	3,	3, 	0,	100,
	  etheraddr,	ROM_UPPER_HALF,
	  piggy_alloc, force30_ether_addr_conv },
	{ 0xFCFF00E0,	{179},	4,	5,	3,	3, 	1,	100,
	  nomemfiltaddr,0xFCFF00E0,
	  uncached_alloc, force30_filt_ether_addr_conv },
	{ 0xFCFF00E0,	{179},	4,	5,	3,	3, 	0,	100,
	  memfiltaddr,	0xFCFF00E0,
	  vme_alloc, vme_ether_addr_conv },
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
