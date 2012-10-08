/*	@(#)ne.c	1.4	96/02/27 13:51:56 */
/*
 * Copyright 1996 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * ne.c
 *
 * FLIP driver for Novell NE1000 and NE2000 Ethernet controllers.
 * This driver is mainly based on the various public domain Novell
 * drivers that circulate on the net.
 *
 *  The NE1000 & NE2000 have an address prom in the first 32 bytes of the
 *  packet buffer address space.  The NE1000 has an 8k packet buffer from
 *  0x2000 to 0x4000 and the NE2000 has a 16k packet buffer from 0x4000
 *  to 0x8000.
 *
 *
 * Authors:
 *	Leendert van Doorn
 *	Gregory J Sharp		May-July 1993
 * Modified:
 *	Gregory J. Sharp, Jan 1996 - don't print config warnings unless asked
 */
#include <bool.h>
#include <assert.h>
INIT_ASSERT
#include <amoeba.h>
#include <debug.h>
#include <machdep.h>
#include <randseed.h>
#include <flip/packet.h>
#include <flip/ethpreamble.h>

#include "gdp8390.h"
#include "gdpinfo.h"

#include "i386_proto.h"

/* NE1000 parameters (page offsets) */
#define	NE1000_START	32	/* start buffer  0x2000 */
#define	NE1000_END	64	/* end buffer    0x4000 */

/* NE2000 parameters (page offsets) */
#define	NE2000_START	64	/* start buffer  0x4000 */
#define	NE2000_END	128	/* end buffer    0x8000 */

/* NE1000/NE2000 board specific registers */
#define	NE_DATA		0x10	/* data I/O port */
#define	NE_RESET	0x1F	/* reset board port */

static int net_debug;


/*
 * The only way to get the only way to the board's Ethernet
 * address is through page 0 of the dp8390. This requires us
 * to do a partial dp8390 initialization; just enough to get
 * the address, proper initialization is done in dp8390_init.
 * We also work out what kind of board it is: ne1000 or ne2000
 * or neither.
 */
static int
getaddr(dpi, dpc, ifaddr)
    dpi_p dpi;
    dpc_p dpc;
    char *ifaddr;
{
    register int reg = dpi->dpi_reg;
    register int timer, val;
    int i;
    char prom[32];

    /* Reset the Ethernet board */
    val = in_byte(reg + NE_RESET);
    pit_delay(2);
    out_byte(reg + NE_RESET, val);
    pit_delay(2);

    /* Reset the dp8390 chip */
    out_byte(reg + DP_CR, CR_STP|CR_DM_ABORT|CR_PS_P0);
    timer = 100000;
    while (!(in_byte(reg + DP_ISR) & ISR_RST)) {
	if (--timer < 0)
	    return 0;
    }
    out_byte(reg + DP_ISR, ISR_RST);

    /* Check whether the dp8390 is really there */
    out_byte(reg + DP_CR, CR_STP|CR_DM_ABORT|CR_PS_P0);
    pit_delay(10);
    if (in_byte(reg + DP_CR) != (CR_STP|CR_DM_ABORT|CR_PS_P0))
	return 0;

    /* Put it in loop-back & byte mode */
    out_byte(reg + DP_RCR, RCR_MON);
    out_byte(reg + DP_TCR, 0);
    out_byte(reg + DP_DCR, DCR_BYTEWIDE|DCR_8BYTES|DCR_BMS);
    out_byte(reg + DP_ISR, 0xFF);

    /* Set up a get on page 0 for the address prom */
    out_byte(reg + DP_RBCR0, 32);
    out_byte(reg + DP_RBCR1, 0);
    out_byte(reg + DP_RSAR0, 0);
    out_byte(reg + DP_RSAR1, 0 >> 8);
    out_byte(reg + DP_CR, CR_DM_RR|CR_PS_P0|CR_STA);

    /*
     * Now we read byte for byte the Address PROM.
     * Since we are in byte mode, if it is a 16-bit board then ever 2nd byte
     * will be the same.  We assume 16-bit till proven otherwise.
     */
    dpi->dpi_16bit = 1;
    for (i = 0; i < 32; i += 2) {
	prom[i]   = in_byte(reg + NE_DATA);
	prom[i+1] = in_byte(reg + NE_DATA);
	if (prom[i] != prom[i+1])
	    dpi->dpi_16bit = 0;
    }
    out_byte(reg + DP_ISR, ISR_RDC);

    /* Setup dp8390 configuration info */
    if (dpi->dpi_16bit) {
	/* NE2000: make it word-wide access and reset the board */
	out_byte(reg + DP_DCR, DCR_WORDWIDE|DCR_8BYTES|DCR_BMS);
	dpc->dpc_tpsr = NE2000_START;
	dpc->dpc_pstart = NE2000_START + 6;
	dpc->dpc_pstop = NE2000_END;
    } else {
	/* NE1000: it's already set up */
	dpc->dpc_tpsr = NE1000_START;
	dpc->dpc_pstart = NE1000_START + 6;
	dpc->dpc_pstop = NE1000_END;
    }
    /* Reset the beast again so it takes note of the data size (8 or 16 bit) */
    val = in_byte(reg + NE_RESET);
    pit_delay(2);
    out_byte(reg + NE_RESET, val);
    pit_delay(2);

    /* Now we tell it all about itself. */
    out_byte(reg + DP_TPSR, (int) dpc->dpc_tpsr);
    out_byte(reg + DP_PSTART, (int) dpc->dpc_pstart);
    out_byte(reg + DP_PSTOP, (int) dpc->dpc_pstop);
    out_byte(reg + DP_BNRY, (int) dpc->dpc_pstart);
    out_byte(reg + DP_CR, CR_PS_P1|CR_DM_ABORT);
    out_byte(reg + DP_CURR, (int) dpc->dpc_pstart + 1);
    out_byte(reg + DP_CR, CR_PS_P0|CR_DM_ABORT);

    /* Complete dp8390 configuration info */
    dpc->dpc_irq = dpi->dpi_irq;
    dpc->dpc_reg = reg;
    dpc->dpc_dpreg = reg;
    dpc->dpc_16bit = dpi->dpi_16bit;
    dpc->dpc_basemem = dpi->dpi_basemem;

    /*
     * Get board's Ethernet address from the prom.
     * Don't forget that if it is 16-bit then we want every 2nd byte!
     */
    for (i = 0; i < 6; i++)
	ifaddr[i] = prom[i << dpi->dpi_16bit];
    return 1;
}


static int
getdata(dpc, page, data, length)
    dpc_p dpc;		/* Device from which to obtain data */
    int page;		/* Page address in bytes! */
    char *data;		/* Buffer to receive data */
    int length;		/* # bytes to read */
{
    register int reg = dpc->dpc_dpreg;
    register int i;

    DPRINTF(1, ("getdata(dpc 0x%x, page %d, data %x, length %d)\n",
	    dpc, page, data, length));
    assert((in_byte(reg + DP_CR) & CR_PS) == CR_PS_P0);

    /* If word mode then make sure length is even */
    if (dpc->dpc_16bit)
	length = (length + 1) & ~1;

    /* setup dp8390 DMA feature */
    out_byte(reg + DP_ISR, ISR_RDC);
    out_byte(reg + DP_RBCR0, length);
    out_byte(reg + DP_RBCR1, length >> 8);
    out_byte(reg + DP_RSAR0, page);
    out_byte(reg + DP_RSAR1, page >> 8);
    out_byte(reg + DP_CR, CR_DM_RR|CR_PS_P0|CR_STA);

    /* Get the actual data */
    if (dpc->dpc_16bit)
	ins_word(reg + NE_DATA, data, length);
    else
	ins_byte(reg + NE_DATA, data, length);

    /* Wait for remote DMA to complete - watch the low byte of the dma addr */
    page += length;
    page &= 0xff;
    i = 25;
    do {
	if (in_byte(reg + DP_RSAR0) == page)
	    break;
    } while (--i >= 0);

    /* Switch off DMA mode */
    out_byte(reg + DP_ISR, ISR_RDC);
    if (i < 0)
	printf("ne_get: DMA did not complete\n");
    return i > 0;
}


/*
 * EXTERNAL INTERFACE ROUTINES
 */

/*
 * Get board's ethernet address, and use these as
 * a seed for the kernel's random generator.
 */
int
ne_init(dpi, dpc, ifaddr)
    dpi_p dpi;
    dpc_p dpc;
    char *ifaddr;
{
    register int i;

#ifndef NDEBUG
    net_debug = kernel_option("netdebug");
#endif

    assert(dpi->dpi_16bit == 0 || dpi->dpi_16bit == 1); /* for now ! */

    if (!getaddr(dpi, dpc, ifaddr)) {
#ifndef NDEBUG
	if (net_debug) {
	    printf("ne: configuration warning: no ethernet board at 0x%x\n",
			dpi->dpi_reg);
	}
#endif /* NDEBUG */
	return 0;
    }

#ifndef NORANDOM
    for (i = 0; i < 6; i++)
	randseed((unsigned long) ifaddr[i], 8, RANDSEED_HOST);
#endif

    return 1;
}


/*
 * Get dp8390 message header.
 * For this board it is necessary to use the storage in dph.
 */
dphead_p
ne_gethead(dpc, page, dph)
    dpc_p dpc;
    int page;
    dphead_p dph;
{
    DPRINTF(1, ("ne_gethead(dpc 0x%x, page %d, dph 0x%x)\n", dpc, page, dph));
    assert(page >= dpc->dpc_pstart && page <= dpc->dpc_pstop);
    if (getdata(dpc, page << 8, (char *)dph, sizeof(dphead_t)))
	return dph;
    return (dphead_p) NULL;
}


/*
 * Get packet from the board,
 * and store it into the designated packet.
 */
int
ne_getpkt(dpc, pkt, page, length)
    dpc_p dpc;
    pkt_p pkt;
    int page;
    int length;
{
    int	addr;

    DPRINTF(1, ("ne_getpkt(dpc 0x%x, pkt 0x%x, page %d, length %d)\n",
	    dpc, pkt, page, length));
    assert(page >= dpc->dpc_pstart && page <= dpc->dpc_pstop);
    addr = (page << 8) + sizeof(dphead_t);
    return getdata(dpc, addr, pkt_offset(pkt), length);
}


/*
 * Put packet into the transmit buffer
 */
int
ne_putpkt(dpc, pkt, tpsr)
    dpc_p dpc;
    pkt_p pkt;	/* packet to send */
    int tpsr;	/* transmit page # */
{
    register int reg = dpc->dpc_reg;
    register int count;
    int addr;
    int	end;

    DPRINTF(1, ("ne_putpkt(dpc 0x%x, pkt 0x%x, tpsr %d)\n", dpc, pkt, tpsr));

    if (dpc->dpc_16bit && (pkt->p_contents.pc_dirsize & 1) != 0) {
	/* We cannot send odd sized packets on 16 bit cards.
	 * But since the direct data size should only be odd if there is no
	 * virtual data, we can just increment it in that case.
	 */
	if (pkt->p_contents.pc_totsize == pkt->p_contents.pc_dirsize) {
	    pkt->p_contents.pc_dirsize++;
	    pkt->p_contents.pc_totsize = pkt->p_contents.pc_dirsize;
	} else {
	    /* Currently fatal */
	    printf("ne_putpkt: totsize = %d, dirsize = %d, virtual 0x%lx\n",
		   pkt->p_contents.pc_totsize, pkt->p_contents.pc_dirsize,
		   pkt->p_contents.pc_virtual);
	    panic("NE: odd dirsize and nonzero virtual data");
	}
    }
    count = pkt->p_contents.pc_totsize;
    /* If 16-bit stores make sure bytecount is a multiple of 2 */
    if (dpc->dpc_16bit)
	count = (count + 1) & ~1;
    addr = tpsr << 8;
    end = (addr + count) & 0xff; /* For DMA termination check */

    /* Setup dp8390 DMA */
    out_byte(reg + DP_ISR, ISR_RDC);
    out_byte(reg + DP_RBCR0, count);
    out_byte(reg + DP_RBCR1, count >> 8);
    out_byte(reg + DP_RSAR0, addr);
    out_byte(reg + DP_RSAR1, addr >> 8);
    out_byte(reg + DP_CR, CR_DM_RW|CR_PS_P0|CR_STA);

    /* Calculate the size of the indirect data */
    count -= pkt->p_contents.pc_dirsize;
    assert(count >= 0);

    /* Send the data - first the direct and then the indirect, if any */
    if (dpc->dpc_16bit) {
	outs_word(reg + NE_DATA, pkt_offset(pkt), pkt->p_contents.pc_dirsize);
	if (count > 0)
	    outs_word(reg+NE_DATA, (char *) pkt->p_contents.pc_virtual, count);
    } else {
	outs_byte(reg + NE_DATA, pkt_offset(pkt), pkt->p_contents.pc_dirsize);
	if (count > 0)
	    outs_byte(reg+NE_DATA, (char *) pkt->p_contents.pc_virtual, count);
    }

    /*
     * Wait for remote DMA to complete -
     * watch the low byte of the dma addr since the bit we're supposed to
     * watch isn't reliable
     */
    count = 25;
    while (in_byte(reg + DP_RSAR0) != end) {
	if (--count <= 0) {
	    printf("ne_put: DMA did not complete\n");
	    break;
	}
    }

    /* Switch off DMA mode */
    out_byte(reg + DP_ISR, ISR_RDC);
    return count > 0;
}


/*
 * Stop NE hardware. That is disable 16-bit bus access,
 * and reset the board.
 */
void
ne_stop(dpc)
    dpc_p dpc;
{
    register int val, reg;

    reg = dpc->dpc_reg;
    DPRINTF(1, ("ne at 0x%x: stopped ethernet board\n", reg));
    val = in_byte(reg + NE_RESET);
    pit_delay(2);
    out_byte(reg + NE_RESET, val);
    pit_delay(2);
}
