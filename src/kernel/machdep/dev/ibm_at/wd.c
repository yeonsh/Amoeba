/*	@(#)wd.c	1.11	96/02/27 13:52:23 */
/*
 * Copyright 1996 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * wd.c
 *
 * FLIP driver for Western Digital's WD8003E, WD8003EB, WD8013EB, and
 * WD8013EP ethernet controllers. The driver determines the type of the
 * board and does the proper initializations accordingly.
 *
 * Author:
 *	Leendert van Doorn
 * Modified:
 *	Gregory J. Sharp, Jan 1996 - don't print config warnings unless asked
 */
#include <bool.h>
#include <assert.h>
INIT_ASSERT
#include <amoeba.h>
#include <machdep.h>
#include <randseed.h>
#include <flip/packet.h>
#include <flip/ethpreamble.h>

#include "gdp8390.h"
#include "gdpinfo.h"

#include "i386_proto.h"

#ifndef WD_DEBUG
#define	WD_DEBUG	0
#endif


/* bit definitions for board features */
#define	ETHERNET_BIT	0x0001		/* ethernet media */
#define	STARLAN_BIT	0x0002		/* star lan media */
#define	INTERFACE_CHIP	0x0004		/* has an WD83C583 interface chip */
#define	BOARD_16BIT	0x0008		/* 16 bit board */
#define	SLOT_16BIT	0x0010		/* 16 bit slot */

/* register offset definitions (these are consistant accross all boards) */
#define	WD_REG0		0x00		/* generic register definitions */
#define	WD_REG1		0x01
#define	WD_REG2		0x02
#define	WD_REG3		0x03
#define	WD_REG4		0x04
#define	WD_REG7		0x07
#define	WD_EA0		0x08		/* most significant addr byte */
#define	WD_EA1		0x09
#define	WD_EA2		0x0A
#define	WD_EA3		0x0B
#define	WD_EA4		0x0C
#define	WD_EA5		0x0D		/* least significant addr byte */
#define	WD_LTB		0x0E		/* LAN type byte */
#define	WD_CHKSUM	0x0F		/* sum from WD_EA0 upto here is 0xFF */
#define	WD_DP8390	0x10		/* natsemi chip */

/* WD83C583 register offsets */
#define	WD_MSR		0x00		/* control (w) and status (r) */
#define	WD_ICR		0x01		/* interface config register */
#define	WD_IRR		0x04		/* interrupt request register */

/* bits in control register */
#define WD_MSR_MEMMASK	0x3F		/* memory enable bits mask */
#define WD_MSR_MENABLE	0x40		/* memory enable */
#define WD_MSR_RESET	0x80		/* software reset */

/* bits in configuration register */
#define	WD_ICR_IOPE	0x02		/* I/O port enable */
#define	WD_ICR_DMAE	0x04		/* DMA enable */
#define	WD_ICR_MSZ	0x08		/* shared memory size */

/* bits in interrupt request register */
#define WD_IRR_IR0	0x20		/* irq bit 1 */
#define WD_IRR_IR1	0x40		/* irq bit 0 */
#define WD_IRR_ENA	0x80		/* interrupt enable */

/* WD83C583 registers (WD8013 special) */
#define	WD_BSR		0x01		/* bus size register (read only) */
#define	WD_LAAR		0x05		/* LA address register (write only) */

/* bits in bus size register */
#define	WD_BSR_16BIT	0x01		/* 16 bit bus */

/* bits in LA address register */
#define	WD_LAAR_A19	0x01		/* address lines for above 1Mb ram */
#define	WD_LAAR_SOFTINT	0x20		/* enable software interrupt */
#define	WD_LAAR_LAN16E	0x40		/* enables 16bit shrd RAM for LAN */
#define	WD_LAAR_MEM16E	0x80		/* enables 16bit shrd RAM for host */

#ifndef NDEBUG
static int wd_debug;		/* current debug level */
#endif

static int wd_aliasing();
static int wd_interface();
static int wd_16bitboard();
static int wd_16bitslot();

/*
 * Get board's dp8390 configuration and ethernet address.
 * And, while we're at it, use the ethernet address as a
 * seed for the kernel's random generator.
 */
int
wd_init(dpi, dpc, ifaddr)
    dpi_p dpi;
    dpc_p dpc;
    char *ifaddr;
{
    int boardid, rambit, revision;
    register int i, wdreg;
    unsigned checksum;
    uint32 memsize;

#ifndef NDEBUG
    if ((wd_debug = kernel_option("netdebug")) == 0)
	wd_debug = WD_DEBUG;
#endif

    /* reset the ethernet card */
    wdreg = dpi->dpi_reg;
    out_byte(wdreg + WD_MSR, WD_MSR_RESET);
    pit_delay(5); /* 5 msec delay */
    out_byte(wdreg + WD_MSR, 0);

    /* probe controller and setup interrupt vector */
    checksum = in_byte(wdreg + WD_EA0) + in_byte(wdreg + WD_EA1) +
	in_byte(wdreg + WD_EA2) + in_byte(wdreg + WD_EA3) +
	in_byte(wdreg + WD_EA4) + in_byte(wdreg + WD_EA5) +
	in_byte(wdreg + WD_LTB) + in_byte(wdreg + WD_CHKSUM);
    if ((checksum & 0xFF) != 0xFF) {
#ifndef NDEBUG
	if (wd_debug) {
	    printf("wd: configuration warning: no ethernet board at 0x%x\n",
									wdreg);
	}
#endif /* NDEBUG */
	return 0;
    }

    /* get our own ethernet address */
    ifaddr[0] = in_byte(wdreg + WD_EA0);
    ifaddr[1] = in_byte(wdreg + WD_EA1);
    ifaddr[2] = in_byte(wdreg + WD_EA2);
    ifaddr[3] = in_byte(wdreg + WD_EA3);
    ifaddr[4] = in_byte(wdreg + WD_EA4);
    ifaddr[5] = in_byte(wdreg + WD_EA5);

#ifndef NORANDOM
    for (i = 0; i < 6; i++)
	randseed((unsigned long) ifaddr[i], 8, RANDSEED_HOST);
#endif

    /*
     * Determine the type of board. This is far from trivial since
     * obtaining the board id varies among the various revisions
     * of the card, and I don't have full info about the board id
     * register layout.
     */
    boardid = ETHERNET_BIT; /* assume ethernet */
    dpi->dpi_16bit = FALSE; /* assume 8 bit */
    if (!wd_aliasing(wdreg)) {
	if (wd_interface(wdreg))
	    boardid |= INTERFACE_CHIP;
	if (wd_16bitboard(wdreg)) {
	    boardid |= BOARD_16BIT;
	    if (wd_16bitslot(wdreg)) {
		dpi->dpi_16bit = TRUE;
		boardid |= SLOT_16BIT;
	    }
	}
    }

    /*
     * Determine size of on board ram. Here we have to trust
     * the validity of the board id register. Assume worst
     * case (8 Kb) if we can't figure it out.
     */
    revision = (in_byte(wdreg + WD_LTB) >> 1) & 0x0F;
    rambit = in_byte(wdreg + WD_LTB) & 0x40; /* ram size bit */
    if (revision < 2) {
	memsize = 0x2000; /* 8 Kb */
	if (boardid & BOARD_16BIT)
	    memsize = 0x4000; /* 16 Kb */
	else if (boardid & INTERFACE_CHIP) {
	    if (in_byte(wdreg + WD_REG1) & 0x08) /* 583 mem size mask */
		memsize = 0x8000; /* 32 Kb */
	}
    } else {
	if (boardid & BOARD_16BIT)
	    memsize = rambit ? 0x8000 : 0x4000; /* 32Kb or 16Kb */
	else
	    memsize = rambit ? 0x8000 : 0x2000; /* 32Kb or 8Kb */
    }

#ifndef NDEBUG
    if (wd_debug) {
	printf("wd at 0x%x: ", wdreg);
	if (boardid & ETHERNET_BIT)
	    printf("[ethernet] ");
	if (boardid & STARLAN_BIT)
	    printf("[startlan] ");
	if (boardid & INTERFACE_CHIP)
	    printf("[interface chip] ");
	if (boardid & BOARD_16BIT)
	    printf("[16-bit board] ");
	if (boardid & SLOT_16BIT)
	    printf("[16-bit slot] ");
	printf("[0x%lx] [%d Kb]\n", dpi->dpi_basemem, memsize >> 10);
    }
#endif

#ifdef WD_SOFTWARE
    /* setup the WD83C583 interface chip */
    if (boardid & INTERFACE_CHIP) {
	if (rambit)
	    out_byte(wdreg + WD_ICR, WD_ICR_IOPE|WD_ICR_DMAE|WD_ICR_MSZ);
	else
	    out_byte(wdreg + WD_ICR, WD_ICR_IOPE|WD_ICR_DMAE);
	if (dpi->dpi_irq == 2)
	    out_byte(wdreg + WD_IRR, WD_IRR_ENA);
	else if (dpi->dpi_irq == 3)
	    out_byte(wdreg + WD_IRR, WD_IRR_ENA|WD_IRR_IR0);
	else if (dpi->dpi_irq == 4)
	    out_byte(wdreg + WD_IRR, WD_IRR_ENA|WD_IRR_IR1);
	else if (dpi->dpi_irq == 7)
	    out_byte(wdreg + WD_IRR, WD_IRR_ENA|WD_IRR_IR0|WD_IRR_IR1);
	else {
	    printf("wd_init: wd at 0x%x: Illegal irq level\n", wdreg);
	    return 0;
	}
    }
#endif

    /* special setup needed for WD8013 boards */
    if (boardid & BOARD_16BIT) {
	out_byte(wdreg + WD_LAAR, dpi->dpi_16bit ?
	    WD_LAAR_A19|WD_LAAR_SOFTINT|WD_LAAR_LAN16E|WD_LAAR_MEM16E :
	    WD_LAAR_A19|WD_LAAR_SOFTINT|WD_LAAR_LAN16E);
    }

    /* initialize the board */
    out_byte(wdreg + WD_MSR, WD_MSR_RESET);
    pit_delay(5); /* 5 msec delay */
    out_byte(wdreg + WD_MSR,
	(int) (WD_MSR_MENABLE | ((dpi->dpi_basemem >> 13) & WD_MSR_MEMMASK)));

    /* setup dp8390 configuration info */
    dpc->dpc_irq = dpi->dpi_irq;
    dpc->dpc_reg = wdreg;
    dpc->dpc_dpreg = wdreg + WD_DP8390;
    dpc->dpc_tpsr = 0;
    dpc->dpc_pstart = 6;
    dpc->dpc_pstop = ((memsize >> 8) & 0xff);
    dpc->dpc_16bit = dpi->dpi_16bit;
    dpc->dpc_basemem = dpi->dpi_basemem;

    /* map in on board memory */
    page_mapin(dpc->dpc_basemem, memsize);

    return 1;
}

/*
 * Determine whether wd8003 hardware performs register
 * aliasing. This implies that it is an old WD8003E board.
 */
static int
wd_aliasing(wdreg)
    int wdreg;
{
    if (in_byte(wdreg + WD_REG1) != in_byte(wdreg + WD_EA1))
	return 0;
    if (in_byte(wdreg + WD_REG2) != in_byte(wdreg + WD_EA2))
	return 0;
    if (in_byte(wdreg + WD_REG3) != in_byte(wdreg + WD_EA3))
	return 0;
    if (in_byte(wdreg + WD_REG4) != in_byte(wdreg + WD_EA4))
	return 0;
    if (in_byte(wdreg + WD_REG7) != in_byte(wdreg + WD_CHKSUM))
	return 0;
    return 1;
}

/*
 * Determine whether the board has an interface chip
 */
static int
wd_interface(wdreg)
    int wdreg;
{
    /* try writing a value and see whether it sticks */
    out_byte(wdreg + WD_REG7, 0x35);
    if (in_byte(wdreg + WD_REG7) != 0x35)
	return 0;
    out_byte(wdreg + WD_REG7, 0x3A);
    if (in_byte(wdreg + WD_REG7) != 0x3A)
	return 0;
    return 1;
}

/*
 * Determine whether the board is capable of doing 16 bits
 * memory moves. If the 16 bit enable bit is unchangable by
 * software we'll assume an 8 bit board.
 */
static int
wd_16bitboard(wdreg)
    int wdreg;
{
    register int bsreg = in_byte(wdreg + WD_REG1);

    out_byte(wdreg + WD_REG1, bsreg ^ WD_BSR_16BIT);
    pit_delay(5); /* 5 msec delay */
    if (in_byte(wdreg + WD_REG1) == bsreg) {
	/*
	 * Pure magic: LTB is 0x05 indicates that this is a WD8013EB board,
	 * 0x27 indicates that this is an WD8013 Elite board, and 0x29
	 * indicates an SMC Elite 16 board.
	 */
	uint8 ltb = in_byte(wdreg + WD_LTB);
#ifndef NDEBUG
	if (wd_debug > 0) printf("WDREG 0x%x: TLB 0x%x\n", wdreg, ltb);
#endif
	return ltb == 0x05 || ltb == 0x27 || ltb == 0x29;
    }
    out_byte(wdreg + WD_REG1, bsreg);
    return 1;
}

/*
 * Determine whether the 16 bit capable board is plugged
 * into a 16 bit slot.
 */
static int
wd_16bitslot(wdreg)
    int wdreg;
{
    return in_byte(wdreg + WD_REG1) & WD_BSR_16BIT;
}

/*
 * Get dp8390 message header
 */
/* ARGSUSED */
dphead_p
wd_gethead(dpc, page, dph)
    dpc_p dpc;
    int page;
    dphead_p dph;
{
    return (dphead_p) (dpc->dpc_basemem + (page << 8));
}

/*
 * Get packet from the board,
 * and store it into the designated packet.
 */
int
wd_getpkt(dpc, pkt, page, length)
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
     * in. When a packet wraps around whe have to copy it in two
     * chunks instead of one.
     */
    ptr = pkt_offset(pkt);
    addr = dpc->dpc_basemem + (page << 8);
    next = ((dphead_p) addr)->dph_next;
    if (next < page && next > dpc->dpc_pstart) {
	int n = ((dpc->dpc_pstop - page) << 8) - sizeof(dphead_t);
	assert(dpc->dpc_pstop - page + next - dpc->dpc_pstart <= 6);
	eth_bcopy(dpc->dpc_16bit, (char *) addr + sizeof(dphead_t), ptr, n);
	if ((length - n) > 0) {
	    assert((length - n) < (6 * 256));
	    eth_bcopy(dpc->dpc_16bit,
		(char *) dpc->dpc_basemem + (dpc->dpc_pstart << 8),
		ptr + n, length - n);
	}
    } else {
	assert((next - page) <= 6);
	assert(length >= 0 && length <= (6 * 256));
	eth_bcopy(dpc->dpc_16bit, (char *) addr + sizeof(dphead_t), ptr, length);
    }
    return 1;
}

/*
 * Copy packet to on-board transmit buffer
 */
int
wd_putpkt(dpc, pkt, tpsr)
    dpc_p dpc;
    pkt_p pkt;
    int tpsr;
{
    register phys_bytes addr;

    addr = dpc->dpc_basemem + (tpsr << 8);

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
 * Stop WD hardware. That is disable 16-bit bus access,
 * and reset the board.
 */
void
wd_stop(dpc)
    dpc_p dpc;
{
#ifndef NDEBUG
    if (wd_debug)
	printf("wd at 0x%x: stopped ethernet board\n", dpc->dpc_reg);
#endif
    if (dpc->dpc_16bit)
	out_byte(dpc->dpc_reg + WD_LAAR, WD_LAAR_A19|WD_LAAR_LAN16E);
    out_byte(dpc->dpc_reg + WD_MSR, WD_MSR_RESET);
    pit_delay(5); /* 5 msec delay */
    out_byte(dpc->dpc_reg + WD_MSR, 0);
}
