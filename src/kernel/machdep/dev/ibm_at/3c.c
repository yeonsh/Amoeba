/*	@(#)3c.c	1.6	96/02/27 13:50:19 */
/*
 * Copyright 1996 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * 3c.c
 *
 * FLIP driver for 3Com Etherlink/II adaptor.
 *
 * Author:
 *	Aart van Halteren, March 1993
 * Modified:
 *	Leendert van Doorn, April 1993: converted to generic dp8930 form
 *	Gregory J. Sharp, July 1993: added some tricks to the e3c_init
 *				     routine to check data bus width and
 *				     set irq to personal preference.
 * Modified:
 *	Gregory J. Sharp, Jan 1996 - don't print config warnings unless asked
 */
#include <bool.h>
#include <assert.h>
INIT_ASSERT
#include <amoeba.h>
#include <sys/debug.h>
#include <machdep.h>
#include <randseed.h>
#include <flip/packet.h>
#include <flip/ethpreamble.h>

#include <gdp8390.h>
#include <gdpinfo.h>
#include <sys/proto.h>
#include "3c.h"
#include "i386_proto.h"

static int net_debug;

static int
regval(base, reg)
    int base, reg;
{
    return in_byte(base + reg);
}

static int
e3c_io_addr(base)
    int base;
{
    switch (regval(base, E3C_GA_BCFR)) {
    case 0x80: return 0x300;
    case 0x40: return 0x310;
    case 0x20: return 0x330;
    case 0x10: return 0x350;
    case 0x08: return 0x250;
    case 0x04: return 0x280;
    case 0x02: return 0x2A0;
    case 0x01: return 0x2E0;
    default: return 0;
    }
}

static phys_bytes
e3c_mem_addr(base)
    int base;
{
    switch (regval(base, E3C_GA_PCFR)) {
    case 0x80: return 0xDC000;
    case 0x40: return 0xD8000;
    case 0x20: return 0xCC000;
    case 0x10: return 0xC8000;
    default: return 0;
    }
}

/*
 * Initialize the 3Com board and initialize the
 * random generator on the fly.
 */
int
e3c_init(dpi, dpc, ifaddr)
    dpi_p dpi;
    dpc_p dpc;
    char *ifaddr;
{
    register int i, e3creg;
    uint32 memsize;
    int width16;

#ifndef NDEBUG
    net_debug = kernel_option("netdebug");
#endif

    /* Reset the ethernet card */
    e3creg = dpi->dpi_reg;
    out_byte(e3creg + E3C_GA_CTRL, E3C_GA_CTRL_RST);
    pit_delay(5); /* 5 msec delay */
    out_byte(e3creg + E3C_GA_CTRL, 0);
    pit_delay(5);

    /* Map in I/O registers */
    out_byte(e3creg + E3C_GA_CTRL, 0);

    /* Check whether the dp8390 is really there */
    out_byte(e3creg + DP_CR, CR_STP|CR_DM_ABORT|CR_PS_P0);
    pit_delay(10);
    if (in_byte(e3creg + DP_CR) != (CR_STP|CR_DM_ABORT|CR_PS_P0))
	return 0;

    /*
     * Check width of data bus:
     * 1. Write 0 to WTS bit.  The board will drive it to 1 if it is a
     *    16-bit card.
     * 2. Select page 2
     * 3. See if it is a 16-bit card
     * 4. Select page 0
     */
    out_byte(e3creg + DP_DCR, 0);
    out_byte(e3creg + DP_CR, CR_PS_P2|CR_DM_ABORT|CR_STP);
    width16 = in_byte(e3creg + DP_DCR) & DCR_WTS;
    out_byte(e3creg + DP_CR, CR_PS_P0|CR_DM_ABORT|CR_STP);

    /*
     * Switch to address PROM. The PROM is 'in window' by 
     * setting bits 2 and 3 of the CTRL-reg to 0,1
     */
    out_byte(e3creg + E3C_GA_CTRL, 0x06);

    /* Get the ethernet address */
    ifaddr[0] = in_byte(e3creg + E3C_EA0);
    ifaddr[1] = in_byte(e3creg + E3C_EA1);
    ifaddr[2] = in_byte(e3creg + E3C_EA2);
    ifaddr[3] = in_byte(e3creg + E3C_EA3);
    ifaddr[4] = in_byte(e3creg + E3C_EA4);
    ifaddr[5] = in_byte(e3creg + E3C_EA5);

    /* Check base I/O port address  */
    if (e3c_io_addr(e3creg) != e3creg) {
#ifndef NDEBUG
	if (net_debug) {
	    printf("3c: configuration warning: no ethernet board at 0x%x\n",
			e3creg);
	}
#endif
	return 0;  
    }

    /*
     * Set pagestart and pagestop registers. The values are the 
     * recommended values in the Technical reference. 
     * They must be the same as the values for PSTART en PSTOP
     * of the dp8390 chip!
     */
    out_byte(e3creg + E3C_GA_PSTR, GA_PageStart);
    out_byte(e3creg + E3C_GA_PSPR, GA_PageStop);

/*XXX - any irq but 3 doesn't seem to work!!! */
    /* Setup interrupt request vector, to irq */
    switch (dpi->dpi_irq)
    {
    case 2:
	out_byte(e3creg + E3C_GA_IDCFR, E3C_GA_IDCFR_IRQ2);
	break;
    case 3:
	out_byte(e3creg + E3C_GA_IDCFR, E3C_GA_IDCFR_IRQ3);
	break;
    case 4:
	out_byte(e3creg + E3C_GA_IDCFR, E3C_GA_IDCFR_IRQ4);
	break;
    case 5:
	out_byte(e3creg + E3C_GA_IDCFR, E3C_GA_IDCFR_IRQ5);
	break;
    default:
	printf("3c: invalid irq configuration: %d\n", dpi->dpi_irq);
	return 0;
    }

    /* Enable bank 0 as shared memory */
    out_byte(e3creg + E3C_GA_CFR,
		    E3C_GA_GACFR_TCM | E3C_GA_GACFR_RSEL | E3C_GA_GACFR_MBS0);

    /*
     * Set the control register for on-board transceiver. Also,
     * make onboard RAM visable by setting bits 2,3 of CTRL-reg
     * to 0, 0
     */
    out_byte(e3creg + E3C_GA_CTRL, E3C_GA_CTRL_THIN);  

    /* Point the 'Vector Pointer' registers to 0xffff0 */
    out_byte(e3creg+E3C_GA_VPTR2, 0xff);
    out_byte(e3creg+E3C_GA_VPTR1, 0xff);
    out_byte(e3creg+E3C_GA_VPTR0, 0x00);

    /* Set DMA vectors */
    out_byte(e3creg+E3C_GA_DAMSB, 0x20);
    out_byte(e3creg+E3C_GA_DALSB, 0x00);

    /* Setup dp8390 configuration info */
    dpc->dpc_irq = dpi->dpi_irq;
    dpc->dpc_reg = e3creg;
    dpc->dpc_dpreg = e3creg;
    dpc->dpc_tpsr = NIC_TPSR;
    dpc->dpc_pstart = NIC_PSTART;
    dpc->dpc_pstop = NIC_PSTOP;
    dpc->dpc_16bit = width16;

    /* Map in on board memory - Base address is jumper settable (J1) */
    memsize = 0x2000; /* memsize is fixed */
    dpc->dpc_basemem = e3c_mem_addr(e3creg);
    page_mapin(dpc->dpc_basemem, memsize);

#ifndef NORANDOM
    for (i = 0; i < 6; i++)
	randseed(ifaddr[i], 8, RANDSEED_HOST);
#endif

    return 1;
}


/*
 * Get dp8390 message header
 */
/* ARGSUSED */
dphead_p
e3c_gethead(dpc, page, dph)
    dpc_p dpc;
    int page;
    dphead_p dph;
{
    return (dphead_p) (dpc->dpc_basemem + ((page - dpc->dpc_tpsr) << 8));
}

/*
 * Get packet from the board,
 * and store it into the designated packet.
 */
int
e3c_getpkt(dpc, pkt, page, length)
    dpc_p dpc;
    pkt_p pkt;
    int page;
    int length;
{
    register char *ptr;
    phys_bytes addr;
    int next;

    /*
     * The dp8390 maintains a circular buffer of 256 byte blocks
     * which are allocated to store the received ethernet packets
     * in. When a packet wraps around we have to copy it in two
     * chunks instead of one.
     */
    ptr = pkt_offset(pkt);
    addr = dpc->dpc_basemem + ((page-NIC_TPSR) << 8);
 
    next = ((dphead_p) addr)->dph_next;
    if (next < page && next > dpc->dpc_pstart) {
	int n = ((dpc->dpc_pstop - page) << 8) - sizeof(dphead_t);
	assert(dpc->dpc_pstop - page + next - dpc->dpc_pstart <= 6);
	eth_bcopy(dpc->dpc_16bit, (char *) addr + sizeof(dphead_t), ptr, n);
	if ((length - n) > 0) {
	    assert((length - n) < (6 * 256));
	    eth_bcopy(dpc->dpc_16bit,
		(char *) dpc->dpc_basemem+ (((dpc->dpc_pstart)-NIC_TPSR) << 8),
		ptr + n, length - n);
	}
    } else {
	assert((next - page) <= 6);
	assert(length >= 0 && length <= (6 * 256));
	eth_bcopy(dpc->dpc_16bit,
	    (char *) addr + sizeof(dphead_t), ptr, length);
    }
    return 1;
}

/*
 * Copy packet to on-board transmit buffer
 */
/*ARGSUSED*/
int
e3c_putpkt(dpc, pkt, tpsr)
    dpc_p dpc;
    pkt_p pkt;
    int tpsr;
{
    register phys_bytes addr;

    addr = dpc->dpc_basemem;

    /* direct data */
    eth_bcopy(dpc->dpc_16bit, pkt_offset(pkt),
	(char *) addr, pkt->p_contents.pc_dirsize);

    /* remaining indirect data */
    if (pkt->p_contents.pc_totsize > pkt->p_contents.pc_dirsize) {
	eth_bcopy(dpc->dpc_16bit, (char *) pkt->p_contents.pc_virtual,
	    (char *) addr + pkt->p_contents.pc_dirsize,
	    pkt->p_contents.pc_totsize - pkt->p_contents.pc_dirsize);
    }
    return 1;
}

/*
 * Stop E3C hardware
 */
void
e3c_stop(dpc)
    dpc_p dpc;
{
    register int e3creg;

    e3creg = dpc->dpc_reg;  /* Not very nice */
#ifndef NDEBUG
    printf("E3C at 0x%x: stopped ethernet board\n", e3creg);
#endif
    out_byte(e3creg + E3C_GA_CTRL, 0x03);
    pit_delay(5); /* 5 msec delay */
    out_byte(e3creg + E3C_GA_CTRL, 0x02);
    pit_delay(5);
}

#ifndef NDEBUG
/*
 * Dump dp8390 statistic information for every adapter
 */
int
e3c_regdump(begin, end)
    char *begin, *end;
{
    int e3cbase = E3C_BASEADDR;
    char *p;

    p = bprintf(begin, end, "======== E3Com GA registers =======\n");
    p = bprintf(p, end, "PSTR: %x\n", regval(e3cbase, E3C_GA_PSTR));
    p = bprintf(p, end, "PSPR: %x\n", regval(e3cbase, E3C_GA_PSPR));
    p = bprintf(p, end, "DQTR: %x\n", regval(e3cbase, E3C_GA_DQTR));

    p = bprintf(p, end, "The following register should be: 0x80. \n");
    p = bprintf(p, end, "BCFR: %x\n", regval(e3cbase, E3C_GA_BCFR));

    p = bprintf(p, end, "The following register should be: 0x20. \n");
    p = bprintf(p, end, "PCRF: %x\n", regval(e3cbase, E3C_GA_PCFR));

    p = bprintf(p, end, "CFR: %x\n", regval(e3cbase, E3C_GA_CFR));
    p = bprintf(p, end, "CTRL: %x\n", regval(e3cbase, E3C_GA_CTRL));
    p = bprintf(p, end, "STREG: %x\n", regval(e3cbase, E3C_GA_STREG));
    p = bprintf(p, end, "IDCFR: %x\n", regval(e3cbase, E3C_GA_IDCFR));
    p = bprintf(p, end, "DAMSB: %x\n", regval(e3cbase, E3C_GA_DAMSB));
    p = bprintf(p, end, "DALSB: %x\n", regval(e3cbase, E3C_GA_DALSB));
    p = bprintf(p, end, "VPTR2: %x\n", regval(e3cbase, E3C_GA_VPTR2));
    p = bprintf(p, end, "VPTR1: %x\n", regval(e3cbase, E3C_GA_VPTR1));
    p = bprintf(p, end, "VPTR0: %x\n", regval(e3cbase, E3C_GA_VPTR0));
    p = bprintf(p, end, "RFMSB: %x\n", regval(e3cbase, E3C_GA_RFMSB));
    p = bprintf(p, end, "RFLSB: %x\n", regval(e3cbase, E3C_GA_RFLSB));

    return p - begin;
}
#endif /* NDEBUG */
