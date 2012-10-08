/*	@(#)am2.c	1.6	96/02/27 13:50:50 */
/*
 * Copyright 1996 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * am2.c
 *
 * Machine dependent part for AMD's AM2XXX ethernet boards. This driver
 * works for Schneider and Koch's SK-Y-NET boards, it should work for
 * Eagle Technologies NE2100, and probably a lot of similar boards.
 *
 * Author:
 *	Leendert van Doorn
 * Modified:
 *	Gregory J. Sharp, Jan 1996 - don't print config warnings unless asked
 */
#include <amoeba.h>
#include <sys/debug.h>
#include <machdep.h>
#include <randseed.h>
#include "i386_proto.h"

static int net_debug;

/*
 * Get board's ethernet address
 */
int
am2_ethaddr(iobase, eaddr)
    long iobase;
    char *eaddr;
{
    uint16 checksum, sum;
    register int i;

#ifndef NDEBUG
    net_debug = kernel_option("netdebug");
#endif

    /*
     * Get ethernet address and compute checksum to be sure
     * that there is a board present at this address.
     */
    sum = 0;
    for (i = 0x00; i <= 0x05; i++)
	sum += (((uint8 *)eaddr)[i] = in_byte((int) (iobase + i)));
    for (i = 0x06; i <= 0x0B; i++)
	sum += in_byte((int) (iobase + i));
    for (i = 0x0E; i <= 0xF; i++)
	sum += in_byte((int) (iobase + i));
    checksum = in_byte((int) (iobase + 0x0C)) |
	      (in_byte((int) (iobase + 0x0D)) << 8);
    if (sum != checksum) {
#ifndef NDEBUG
	if (net_debug) {
	    printf("am2: configuration warning: no ethernet board at 0x%x\n",
			iobase);
	}
#endif /* NDEBUG */
	return 0;
    }

    /* use ethernet address as seed for the random generator */
    for (i = 0; i <= 0x05; i++)
	randseed((unsigned long) eaddr[i], 8, RANDSEED_HOST);

    return 1;
}

/*
 * Convert address to an address reachable by the lance. The generic
 * lance driver itself will take care of addresses that are outside
 * its range.
 */
uint32
am2_addrconv(addr, size)
    uint32 addr, size;
{
    if (addr + size >= 0x1000000)
	return 0x1000000; /* out of lance's reach */
    return addr;
}
