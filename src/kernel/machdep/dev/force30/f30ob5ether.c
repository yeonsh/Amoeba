/*	@(#)f30ob5ether.c	1.4	94/04/06 09:04:32 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <string.h>
#include "amoeba.h"
#include "machdep.h"
#include "global.h"
#include "assert.h"
INIT_ASSERT
#include "map.h"

/************************************************************************/
/*      NETWORK                                                         */
/************************************************************************/

/* ARGSUSED */
static phys_bytes
force30_ether_addr_conv(a, size)
phys_bytes a;
uint32 size;
{

        if (a <0xFEF00000)
                return 0x1000000;
        return (a - 0xFEF00000);
}

#include "interrupt.h"
#include "../../../flip/dev/glainfo.h"

static phys_bytes start;

static
char *bmalloc(size,align)
int size;
int align;
{
	phys_bytes b;

	if(align == 0) align = sizeof(long);
	if(start == 0) start = 0xFEF00000;
        compare(size, <=, 0x10000);
        (void) memset((_VOIDSTAR) start, 0, (size_t) (size+align));
	b = start;
	start += size + align;
	b = b + align - b % align;
	assert(b % align == 0);
        return (char *) b;
}

#define ROM_UPPER_HALF	0xFF020000

static
etheraddr(dev, eaddr)
char *dev;
register char *eaddr;
{
	register i;
	register char *p;

	p = (char *) ROM_UPPER_HALF;
	for (i=0; i<6; i++) {
		*eaddr = *p;
		eaddr++;
		p += 4;		/* Use only EPROM 0 */
	}
	return 1;
}

lai_t la_info[1]= {
	{ DVADR_LANCE,	{246},	4,	5,	3,	3, 	100,
	  etheraddr,	DVADR_LANCE,	bmalloc, force30_ether_addr_conv },
};

#include "ethif.h"

int la_init(); void la_send();
int la_setmc(), la_stop();

hei_t heilist[] = {
{ "lance", 1, la_init, la_send, la_setmc, la_stop },
{ 0 }
};
