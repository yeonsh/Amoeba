/*	@(#)wd80x3.c	1.1	92/06/25 14:49:54 */
/*
 * wd80x3.c
 *
 * A very simple network driver for WD80x3 boards that polls.
 *
 * Copyright (c) 1992 by Leendert van Doorn
 */
#include "assert.h"
#include "types.h"
#include "packet.h"
#include "ether.h"
#include "dp8390.h"

/* configurable parameters */
#define	WD_BASEREG	0x280		/* base register */
#define	WD_BASEMEM	0xD0000		/* base memory */

/* bit definitions for board features */
#define	INTERFACE_CHIP	01		/* has an WD83C583 interface chip */
#define	BOARD_16BIT	02		/* 16 bit board */
#define	SLOT_16BIT	04		/* 16 bit slot */

/* register offset definitions */
#define	WD_MSR		0x00		/* control (w) and status (r) */
#define WD_REG0		0x00		/* generic register definitions */
#define WD_REG1		0x01
#define WD_REG2		0x02
#define WD_REG3		0x03
#define WD_REG4		0x04
#define WD_REG5		0x05
#define WD_REG6		0x06
#define WD_REG7		0x07
#define	WD_EA0		0x08		/* most significant addr byte */
#define	WD_EA1		0x09
#define	WD_EA2		0x0A
#define	WD_EA3		0x0B
#define	WD_EA4		0x0C
#define	WD_EA5		0x0D		/* least significant addr byte */
#define	WD_LTB		0x0E		/* LAN type byte */
#define	WD_CHKSUM	0x0F		/* sum from WD_EA0 upto here is 0xFF */
#define	WD_DP8390	0x10		/* natsemi chip */

/* bits in control register */
#define	WD_MSR_MEMMASK	0x3F		/* memory enable bits mask */
#define	WD_MSR_MENABLE	0x40		/* memory enable */
#define	WD_MSR_RESET	0x80		/* software reset */

/* bits in bus size register */
#define	WD_BSR_16BIT	0x01		/* 16 bit bus */

/* bits in LA address register */
#define	WD_LAAR_A19	0x01		/* address lines for above 1Mb ram */
#define	WD_LAAR_LAN16E	0x40		/* enables 16bit shrd RAM for LAN */
#define	WD_LAAR_MEM16E	0x80		/* enables 16bit shrd RAM for host */

uint8 eth_myaddr[ETH_ADDRSIZE];

static int boardid;
static dpconf_t dpc;

extern uint32 dsseg();

/*
 * Initialize the WD80X3 board
 */
int
eth_init()
{
    register unsigned sum;
    int memsize;

    /* reset the ethernet card */
    out_byte(WD_BASEREG + WD_MSR, WD_MSR_RESET);
    longpause();
    out_byte(WD_BASEREG + WD_MSR, 0);

    /* determine whether the controller is there */
    sum = in_byte(WD_BASEREG + WD_EA0) + in_byte(WD_BASEREG + WD_EA1) +
	in_byte(WD_BASEREG + WD_EA2) + in_byte(WD_BASEREG + WD_EA3) +
	in_byte(WD_BASEREG + WD_EA4) + in_byte(WD_BASEREG + WD_EA5) +
	in_byte(WD_BASEREG + WD_LTB) + in_byte(WD_BASEREG + WD_CHKSUM);
    if ((sum & 0xFF) != 0xFF) return 0;

    /*
     * Determine the type of board
     */
    boardid = 0;
    if (!aliasing()) {
	if (board16bit()) {
	    boardid |= BOARD_16BIT;
	    if (slot16bit())
		boardid |= SLOT_16BIT;
	}
    }
    memsize = (boardid & BOARD_16BIT) ? 0x4000 : 0x2000; /* 16 or 8 Kb */

    /* special setup needed for WD8013 boards */
    if (boardid & SLOT_16BIT)
	out_byte(WD_BASEREG + WD_REG5, WD_LAAR_A19|WD_LAAR_LAN16E);

    /* get ethernet address */
    eth_myaddr[0] = in_byte(WD_BASEREG + WD_EA0);
    eth_myaddr[1] = in_byte(WD_BASEREG + WD_EA1);
    eth_myaddr[2] = in_byte(WD_BASEREG + WD_EA2);
    eth_myaddr[3] = in_byte(WD_BASEREG + WD_EA3);
    eth_myaddr[4] = in_byte(WD_BASEREG + WD_EA4);
    eth_myaddr[5] = in_byte(WD_BASEREG + WD_EA5);

    /* save settings for future use */
    dpc.dc_reg = WD_BASEREG + WD_DP8390;
    dpc.dc_mem = WD_BASEMEM;
    dpc.dc_tpsr = 0;
    dpc.dc_pstart = 6;
    dpc.dc_pstop = (memsize >> 8) & 0xFF;

    eth_reset();
    return 1;
}

/*
 * Determine whether wd8003 hardware performs register aliasing
 * (i.e. whether it is an old WD8003E board).
 */
static int
aliasing()
{
    if (in_byte(WD_BASEREG + WD_REG1) != in_byte(WD_BASEREG + WD_EA1))
	return 0;
    if (in_byte(WD_BASEREG + WD_REG2) != in_byte(WD_BASEREG + WD_EA2))
	return 0;
    if (in_byte(WD_BASEREG + WD_REG3) != in_byte(WD_BASEREG + WD_EA3))
	return 0;
    if (in_byte(WD_BASEREG + WD_REG4) != in_byte(WD_BASEREG + WD_EA4))
	return 0;
    if (in_byte(WD_BASEREG + WD_REG7) != in_byte(WD_BASEREG + WD_CHKSUM))
	return 0;
    return 1;
}

/*
 * Determine whether this board has 16-bit capabilities
 */
static int
board16bit()
{
    uint8 bsreg = in_byte(WD_BASEREG + WD_REG1);

    out_byte(WD_BASEREG + WD_REG1, bsreg ^ WD_BSR_16BIT);
    longpause();
    if (in_byte(WD_BASEREG + WD_REG1) == bsreg) {
	/*
	 * Pure magic: LTB is 0x05 indicates that this is a WD8013EB board,
	 * 0x27 indicates that this is an WD8013 Elite board, and 0x29
	 * indicates an SMC Elite 16 board.
	 */
	uint8 tlb = in_byte(WD_BASEREG + WD_LTB);
	return tlb == 0x05 || tlb == 0x27 || tlb == 0x29;
    }
    out_byte(WD_BASEREG + WD_REG1, bsreg);
    return 1;

}

/*
 * Determine whether the 16 bit capable board is plugged
 * into a 16 bit slot.
 */
static int
slot16bit()
{
    return in_byte(WD_BASEREG + WD_REG1) & WD_BSR_16BIT;
}

/*
 * This trick is stolen from the clarkson packet driver
 */
static int
longpause()
{
    register int i;

    for (i = 1600; i > 0; i++)
	(void) in_byte(0x61);
}

/*
 * Reset ethernet board after a timeout
 */
void
eth_reset()
{
    register int dpreg = dpc.dc_reg;

    /* initialize the board */
    out_byte(WD_BASEREG + WD_MSR,
	WD_MSR_MENABLE | (((uint32)WD_BASEMEM >> 13) & WD_MSR_MEMMASK));

    /* reset dp8390 ethernet chip */
    out_byte(dpreg + DP_CR, CR_STP|CR_DM_ABORT);

    /* initialize first register set */
    out_byte(dpreg + DP_IMR, 0);
    out_byte(dpreg + DP_CR, CR_PS_P0|CR_STP|CR_DM_ABORT);
    out_byte(dpreg + DP_TPSR, dpc.dc_tpsr);
    out_byte(dpreg + DP_PSTART, dpc.dc_pstart);
    out_byte(dpreg + DP_PSTOP, dpc.dc_pstop);
    out_byte(dpreg + DP_BNRY, dpc.dc_pstart);
    out_byte(dpreg + DP_RCR, RCR_MON);
    out_byte(dpreg + DP_TCR, TCR_NORMAL|TCR_OFST);
    if (boardid & SLOT_16BIT)
	out_byte(dpreg + DP_DCR, DCR_WORDWIDE|DCR_8BYTES);
    else
	out_byte(dpreg + DP_DCR, DCR_BYTEWIDE|DCR_8BYTES);
    out_byte(dpreg + DP_RBCR0, 0);
    out_byte(dpreg + DP_RBCR1, 0);
    out_byte(dpreg + DP_ISR, 0xFF);

    /* initialize second register set */
    out_byte(dpreg + DP_CR, CR_PS_P1|CR_DM_ABORT);
    out_byte(dpreg + DP_PAR0, eth_myaddr[0]);
    out_byte(dpreg + DP_PAR1, eth_myaddr[1]);
    out_byte(dpreg + DP_PAR2, eth_myaddr[2]);
    out_byte(dpreg + DP_PAR3, eth_myaddr[3]);
    out_byte(dpreg + DP_PAR4, eth_myaddr[4]);
    out_byte(dpreg + DP_PAR5, eth_myaddr[5]);
    out_byte(dpreg + DP_CURR, dpc.dc_pstart+1);

    /* and back to first register set */
    out_byte(dpreg + DP_CR, CR_PS_P0|CR_DM_ABORT);
    out_byte(dpreg + DP_RCR, RCR_AB);

    /* flush counters */
    (void) in_byte(dpreg + DP_CNTR0);
    (void) in_byte(dpreg + DP_CNTR1);
    (void) in_byte(dpreg + DP_CNTR2);

    /* and go ... */
    out_byte(dpreg + DP_CR, CR_STA|CR_DM_ABORT);
}

/*
 * Stop ethernet board
 */
void
eth_stop()
{
    /* stop dp8390, followed by a board reset */
    out_byte(dpc.dc_reg + DP_CR, CR_STP|CR_DM_ABORT);
    out_byte(WD_BASEREG + WD_MSR, WD_MSR_RESET);
    out_byte(WD_BASEREG + WD_MSR, 0);
}

/*
 * Send an ethernet packet to destination 'dest'
 */
void
eth_send(pkt, proto, dest)
    packet_t *pkt;
    uint16 proto;
    uint8 *dest;
{
    register ethhdr_t *ep;

    pkt->pkt_len += sizeof(ethhdr_t);
    pkt->pkt_offset -= sizeof(ethhdr_t);
    ep = (ethhdr_t *) pkt->pkt_offset;
    ep->eth_proto = htons(proto);
    bcopy(dest, ep->eth_dst, ETH_ADDRSIZE);
    bcopy(eth_myaddr, ep->eth_src, ETH_ADDRSIZE);
    if (pkt->pkt_len < 60)
	pkt->pkt_len = 60;

    wdcopy((dsseg() << 4) + (uint32)pkt->pkt_offset,
	dpc.dc_mem + (uint32)(dpc.dc_tpsr << 8), pkt->pkt_len);
    out_byte(dpc.dc_reg + DP_TPSR, dpc.dc_tpsr);
    out_byte(dpc.dc_reg + DP_TBCR0, pkt->pkt_len & 0xFF);
    out_byte(dpc.dc_reg + DP_TBCR1, (pkt->pkt_len >> 8) & 0xFF);
    out_byte(dpc.dc_reg + DP_CR, CR_TXP);
}

/*
 * Poll the dp8390 just see if there's an Ethernet packet
 * available. If there is, its contents is returned in a
 * pkt structure, otherwise a nil pointer is returned.
 */
packet_t *
eth_receive()
{
    uint8 pageno, curpage, nextpage;
    int dpreg = dpc.dc_reg;
    packet_t *pkt;
    dphdr_t dph;
    uint32 addr;

    pkt = (packet_t *)0;
    if (in_byte(dpreg + DP_RSR) & RSR_PRX) {
	/* get current page numbers */
	pageno = in_byte(dpreg + DP_BNRY) + 1;
	if (pageno == dpc.dc_pstop)
	    pageno = dpc.dc_pstart;
	out_byte(dpreg + DP_CR, CR_PS_P1);
	curpage = in_byte(dpreg + DP_CURR);
	out_byte(dpreg + DP_CR, CR_PS_P0);
	if (pageno == curpage)
	    return (packet_t *) 0;

	/* get packet header */
	addr = dpc.dc_mem + (pageno << 8);
	getheader(addr, &dph);
	nextpage = dph.dh_next;

	/* allocate packet */
	pkt = pkt_alloc(0);
	pkt->pkt_len = ((dph.dh_rbch & 0xFF) << 8) | (dph.dh_rbcl & 0xFF);
	pkt->pkt_len -= sizeof(dphdr_t);
	if (pkt->pkt_len > 1514) /* bug in dp8390 */
	    pkt->pkt_len = 1514;

	/*
	 * The dp8390 maintains a circular buffer of pages (256 bytes)
	 * in which incomming ethernet packets are stored. The following
	 * if detects wrap arounds, and copies the ethernet packet to
	 * our local buffer in two chunks if necesarry.
	 */
	assert(pkt->pkt_offset);
	assert(pkt->pkt_len >= 0);
	assert(pkt->pkt_len <= (6 << 8));
	if (nextpage < pageno && nextpage > dpc.dc_pstart) {
	    int nbytes = ((dpc.dc_pstop - pageno) << 8) - sizeof(dphdr_t);

	    assert(nbytes >= 0 && nbytes <= (6 << 8));
	    wdcopy(addr + sizeof(dphdr_t), (dsseg() << 4) +
		(uint32)pkt->pkt_offset, nbytes);
	    if ((pkt->pkt_len - nbytes) > 0)
		wdcopy(dpc.dc_mem + (dpc.dc_pstart << 8),
		    (dsseg() << 4) + (uint32)pkt->pkt_offset + nbytes,
		    pkt->pkt_len - nbytes); 
	} else {
	     wdcopy(addr + sizeof(dphdr_t), (dsseg() << 4) +
		(uint32)pkt->pkt_offset, pkt->pkt_len);
	}

	/* release occupied pages */
	if (nextpage == dpc.dc_pstart)
	    nextpage = dpc.dc_pstop;
	out_byte(dpreg + DP_BNRY, nextpage - 1);
    }

    return pkt;
}

/*
 * Copy dp8390 packet header for observation
 */
void
getheader(haddr, dph)
    uint32 haddr;
    dphdr_t *dph;
{
    wdcopy(haddr, (dsseg() << 4) + (uint32)dph, sizeof(dphdr_t));
}

void
wdcopy(src, dst, count)
    uint32 src, dst;
    int count;
{
    assert(count >= 0);
    assert(count <= 1514);
    if (boardid & SLOT_16BIT)
	out_byte(WD_BASEREG + WD_REG5,
	    WD_LAAR_MEM16E|WD_LAAR_LAN16E|WD_LAAR_A19);
    phys_copy(src, dst, (uint32)count);
    if (boardid & SLOT_16BIT)
	out_byte(WD_BASEREG + WD_REG5, WD_LAAR_LAN16E|WD_LAAR_A19);
}

/*
 * Print an ethernet address in human readable form
 */
void
eth_printaddr(addr)
    uint8 *addr;
{
    printf("%x:%x:%x:%x:%x:%x",
	addr[0] & 0xFF, addr[1] & 0xFF, addr[2] & 0xFF,
	addr[3] & 0xFF, addr[4] & 0xFF, addr[5] & 0xFF);
}
