/*	@(#)gdp8390.c	1.5	94/08/19 13:54:40 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * gdp8390.c
 *
 * FLIP driver for the dp8390 ethernet chip. This is the generic part for
 * the Western Digital, Novell, and 3Com Ethernet boards. This driver
 * supports multiple controllers, so it can be used for gateway machines.
 *
 * STILL TO DO:
 * - Multicast mask computation.
 * - Look at the statistic gathering code. Some of the statistics seem
 *   unreasonable (e.g. the # of collisions).
 *
 * Author:
 *	Leendert van Doorn
 */
#include <bool.h>
#include <amoeba.h>
#include <debug.h>
#include <type.h>
#include <machdep.h>
#include <internet.h>
#include <assert.h>
INIT_ASSERT
#include <global.h>
#include <byteorder.h>
#include <ethif.h>
#include <sys/flip/ethproto.h>
#include <sys/flip/ethpreamble.h>
#include <sys/flip/packet.h>
#include <sys/flip/flip.h>
#include <sys/proto.h>

#include "gdp8390.h"
#include "gdpinfo.h"

#include "i386_proto.h"

#ifndef DP_DEBUG
#define	DP_DEBUG	0
#endif

/*
 * When a packet cannot be send immediately it is placed on a transmit queue.
 * The packets in this queue are send in FIFO order when a packet-sent acknow-
 * ledgement occurs. A threshold of 50 packets ought to be enough.
 */
#define	TQUEUESIZE	50

/*
 * This driver allocates a packet pool which it uses to store received packets.
 * The size of the packet pool itself should be reasonable large, say, about
 * 80 entries. The buffer (part of the packet) should be capable of holding
 * an maximum Ethernet packet and some extra space for the packet begin header.
 */
#define	RCVETHPKTSIZE	(PKTBEGHDR + 1514)
#define	RCVPOOLSIZE	80

/* Upperbound on the number of interrupts to protect against faulty chips */
#define DP_MAX_INTR	256

/* When the transmit buffer is overflowing we throw a certain number
 * of new packets before we decide to reset the chip.
 */
#define DP_MAX_OVERFLOW	50

/*
 * Dp8390 adapter information
 */
extern struct dpinfo dp_info[];

/*
 * Per dp8390 ethernet chip we maintain the following device specific
 * information: software interface number, its ethernet address,
 * current multicast mask, transmit packet queue management variables,
 * receive packet pool, dp8390 chip configuration (and some board
 * dependent information), and, when enabled, statistic information.
 */
typedef struct dpdata {
    int	dp_softifno;		/* soft interface number */
    uint8 dp_eaddr[6];		/* ethernet address */
    uint8 dp_maddr[8];		/* multicast address */
    pkt_p *dp_tpkts;		/* transmit packet queue */
    int dp_thead;		/* head transmit packet queue */
    int dp_ttail;		/* tail transmit packet queue */
    int dp_tready;		/* transmitter ready to send */
    pool_t dp_rpool;		/* receive packet pool */
    dpc_t dp_conf;		/* chip & board configuration information */
    int dp_cur_toverflow;	/* current number of transmission overflows */
    int dp_tot_toverflow;	/* total number of overflows */
#ifdef STATISTICS
    dpstat_t dp_stat;		/* dp8390 statistic information */
#endif
} dpd_t, *dpd_p;

static int ndp;			/* # of dp8390 devices */
static dpd_p dp_data;		/* dp8390 device information */
#define dp_nr(dpp)		((dpp) - dp_data)

#ifndef NDEBUG
static int dp_debug;		/* current debug level */
static int dp_unitmask;		/* unit mask */
#endif

void dp_intr();
void dp8390_init();
void dp_ovw_intr();
void dp_received();
void dp_transmitted();
void dp_count();


/*
 * Routine to track the use of ISA bus addresses and IRQ for
 * dp8390 devices so that the init routine of one brand doesn't probe
 * the registers of a card of another brand that has already been found.
 */
static int
addr_in_use(reg, irq)
int	reg;
int	irq;
{
    int	i;
    register dpd_p dpd;

    DPRINTF(1, ("addr_in_use: checking addr %x, irq %d\n", reg, irq));
    dpd = dp_data;
    assert(dpd != 0);
    assert(ndp != 0);
    for (i = 0; i < ndp; dpd++, i++) {
	if (dpd->dp_conf.dpc_reg == reg || dpd->dp_conf.dpc_irq == irq) {
	    DPRINTF(1, ("addr_in_use: interface %d has addr %x irq %d\n",
			dpd->dp_conf.dpc_reg, dpd->dp_conf.dpc_irq));
	    return 1;
	}
    }
    return 0;
}


/*
 * Allocate device structures for every adapter
 */
int
dp_alloc(nif)
    int nif;
{
    ndp = nif;
    dp_data = (dpd_p) aalloc((vir_bytes) ndp * sizeof(dpd_t), 0);
#ifndef NDEBUG
    if ((dp_debug = kernel_option("dp")) == 0)
	dp_debug = DP_DEBUG;
    if ((dp_unitmask = kernel_option("dpum")) == 0)
	dp_unitmask = 0xFF;
#endif
}

/*
 * Initialize a dp8390 adapter
 */
int
dp_init(hardifno, softifno, ifaddr, nrcvpkt, nsndpkt)
    int hardifno, softifno;
    char *ifaddr;
    int *nrcvpkt, *nsndpkt;
{
    register dpd_p dpp;
    register dpi_p dpi;
    char *data;
    pkt_p bufs;

    assert(ifaddr);
    assert(nrcvpkt);
    assert(nsndpkt);
    assert(hardifno < ndp);

    DPRINTF(1, ("dp_init: hardifno=%d softifno=%d\n", hardifno, softifno));
    /*
     * Get dp8390 parameters, ethernet address,
     * and initialize the board
     */
    dpi = &dp_info[hardifno];
    /* Make sure this address and irq aren't already in use! */
    if (addr_in_use(dpi->dpi_reg, dpi->dpi_irq)) {
	DPRINTF(0, ("dp_init: skipping ether #%d, addr %x irq %d in use\n",
			hardifno, dpi->dpi_reg, dpi->dpi_irq));
	return 0;
    }

    dpp = &dp_data[hardifno];
    if ((*dpi->dpi_init)(dpi, &dpp->dp_conf, ifaddr) == 0)
	return 0;

    /*
     * Start allocating resources
     */
    dpp->dp_softifno = softifno;
    /* transmit queue management */
    dpp->dp_tready = TRUE;
    dpp->dp_thead = dpp->dp_ttail = 0;
    dpp->dp_tpkts = (pkt_p *) aalloc((vir_bytes) TQUEUESIZE * sizeof(pkt_p), 0);
    /* allocate data buffers and receive pool */
    data = aalloc((vir_bytes) RCVPOOLSIZE * RCVETHPKTSIZE, 0);
    bufs = (pkt_p) aalloc((vir_bytes) RCVPOOLSIZE * sizeof(pkt_t), 0);
    pkt_init(&dpp->dp_rpool, RCVETHPKTSIZE, bufs, RCVPOOLSIZE, data,
		(void (*)()) 0, 0L);

    /* save our own ethernet address */
    memcpy((_VOIDSTAR) dpp->dp_eaddr, (_VOIDSTAR) ifaddr, 6);

    /* zero multicast mask */
    dpp->dp_maddr[0] = 0;
    dpp->dp_maddr[1] = 0;
    dpp->dp_maddr[2] = 0;
    dpp->dp_maddr[3] = 0;
    dpp->dp_maddr[4] = 0;
    dpp->dp_maddr[5] = 0;
    dpp->dp_maddr[6] = 0;
    dpp->dp_maddr[7] = 0;

    /* for credit computations in higher level layer */
    *nsndpkt = TQUEUESIZE;
    *nrcvpkt = (dpp->dp_conf.dpc_pstop - dpp->dp_conf.dpc_pstart) / 6;
#ifndef NDEBUG
    if (dp_debug > 1)
	printf("dp #%d: nrcvpkt %d, nsndpkt %d\n",softifno, *nrcvpkt, *nsndpkt);
#endif

    /* initialize dp8390 ethernet chip */
    dp8390_init(dpp);

    /* set interrupt vector and enable interrupts */
    setirq(dpp->dp_conf.dpc_irq, dp_intr);
    pic_enable(dpp->dp_conf.dpc_irq);

    return 1;
}

/*
 * Initialize the dp8390 chip, based on the configuration
 * information found in the device structure.
 */
void
dp8390_init(dpp)
    register dpd_p dpp;
{
    register int dpreg = dpp->dp_conf.dpc_dpreg;

#ifndef NDEBUG
    if (dp_debug > 1 && ((1 << dp_nr(dpp)) & dp_unitmask))
	printf("dp8390_init(): dp #%d\n", dp_nr(dpp));
#endif

    /* reset dp8390 ethernet chip */
    out_byte(dpreg + DP_CR, CR_STP|CR_DM_ABORT);

    /* initialize first register set */
    out_byte(dpreg + DP_IMR, 0);
    out_byte(dpreg + DP_CR, CR_PS_P0|CR_STP|CR_DM_ABORT);
    out_byte(dpreg + DP_TPSR, (int) dpp->dp_conf.dpc_tpsr);
    out_byte(dpreg + DP_PSTART, (int) dpp->dp_conf.dpc_pstart);
    out_byte(dpreg + DP_PSTOP, (int) dpp->dp_conf.dpc_pstop);
    out_byte(dpreg + DP_BNRY, (int) dpp->dp_conf.dpc_pstart);
    out_byte(dpreg + DP_RCR, RCR_MON);
    out_byte(dpreg + DP_TCR, TCR_NORMAL|TCR_OFST);
    if (dpp->dp_conf.dpc_16bit)
	out_byte(dpreg + DP_DCR, DCR_WORDWIDE|DCR_8BYTES|DCR_BMS);
    else
	out_byte(dpreg + DP_DCR, DCR_BYTEWIDE|DCR_8BYTES|DCR_BMS);
    out_byte(dpreg + DP_RBCR0, 0);
    out_byte(dpreg + DP_RBCR1, 0);
    out_byte(dpreg + DP_ISR, 0xFF);

    /* initialize second register set */
    out_byte(dpreg + DP_CR, CR_PS_P1|CR_DM_ABORT);
    out_byte(dpreg + DP_PAR0, (int) dpp->dp_eaddr[0]);
    out_byte(dpreg + DP_PAR1, (int) dpp->dp_eaddr[1]);
    out_byte(dpreg + DP_PAR2, (int) dpp->dp_eaddr[2]);
    out_byte(dpreg + DP_PAR3, (int) dpp->dp_eaddr[3]);
    out_byte(dpreg + DP_PAR4, (int) dpp->dp_eaddr[4]);
    out_byte(dpreg + DP_PAR5, (int) dpp->dp_eaddr[5]);
    out_byte(dpreg + DP_CURR, (int) dpp->dp_conf.dpc_pstart + 1);
    out_byte(dpreg + DP_MAR0, (int) dpp->dp_maddr[0]);
    out_byte(dpreg + DP_MAR1, (int) dpp->dp_maddr[1]);
    out_byte(dpreg + DP_MAR2, (int) dpp->dp_maddr[2]);
    out_byte(dpreg + DP_MAR3, (int) dpp->dp_maddr[3]);
    out_byte(dpreg + DP_MAR4, (int) dpp->dp_maddr[4]);
    out_byte(dpreg + DP_MAR5, (int) dpp->dp_maddr[5]);
    out_byte(dpreg + DP_MAR6, (int) dpp->dp_maddr[6]);
    out_byte(dpreg + DP_MAR7, (int) dpp->dp_maddr[7]);

    /* and back to first register set */
    out_byte(dpreg + DP_CR, CR_PS_P0|CR_DM_ABORT);
    out_byte(dpreg + DP_RCR, RCR_AB|RCR_AM);

    /* flush counters */
    (void) in_byte(dpreg + DP_CNTR0);
    (void) in_byte(dpreg + DP_CNTR1);
    (void) in_byte(dpreg + DP_CNTR2);

    /* and go ... */
#ifdef STATISTICS
    out_byte(dpreg + DP_IMR,
	IMR_PRXE|IMR_PTXE|IMR_OVWE|IMR_RXEE|IMR_TXEE|IMR_CNTE);
#else
    out_byte(dpreg + DP_IMR, IMR_PRXE|IMR_PTXE|IMR_OVWE);
#endif
    out_byte(dpreg + DP_CR, CR_STA|CR_DM_ABORT);
}

/*
 * Send a packet for specified interface onto the wire. This function
 * will either send the packet directly or queue it up when the dp8390
 * is already sending a packet. In the later case the packet will be
 * transmitted upon a send interrupt. Once the packet data has been
 * copied into on board memory its discarded.
 */
void
dp_send(ifno, pkt)
    int ifno;
    pkt_p pkt;
{
    register int dpreg, length;
    register dpd_p dpp;
    int frozen = 0;
    int tnext;
    dpi_p dpi;

    assert(ifno < ndp);
    dpi = &dp_info[ifno];
    dpp = &dp_data[ifno];
    dpreg = dpp->dp_conf.dpc_dpreg;
    assert(dpp->dp_conf.dpc_irq != 0);

tryagain:
    if (!dpp->dp_tready) {
	/* stuff it into the transmit queue */
	if ((tnext = dpp->dp_thead+1) == TQUEUESIZE) tnext = 0;
	/*
	 * It sometimes happens that the chip will no longer generate ISR_PTX
	 * interupts, which results in filling up the transmit queue. Resetting
	 * the dp8930 chip hopefully gets us going again.
	 */
	if (tnext == dpp->dp_ttail) {
	    /* Don't break off the current transmission unless we're
	     * pretty sure the chip is hanging.  By default just drop
	     * the new packet like the Lance driver does.
	     */
	    dpp->dp_cur_toverflow++;
	    dpp->dp_tot_toverflow++;

	    if (dpp->dp_cur_toverflow < DP_MAX_OVERFLOW) {
		pkt_discard(pkt);
		return;
	    }

	    if (in_byte(dpreg + DP_CR) & CR_TXP) {
		printf("dp #%d: dp_send: stuck in transmission?\n",
			dp_nr(dpp));
		out_byte(dpreg + DP_CR, CR_STP|CR_DM_ABORT);
		dp8390_init(dpp);
	    } else {
		/* missed transmit intr; happens now and then */
#ifndef NDEBUG
		if (dp_debug > 0) {
		    printf("dp #%d: dp_send: missed transmission intr?\n",
			   dp_nr(dpp));
		}
#endif
	    }
	    dp_transmitted((long) dpp);
	    if (frozen++ > 10) {
		printf("dp #%d: transmitter frozen; check cable\n",
		       dp_nr(dpp));
		pkt_discard(pkt);
		return;
	    }
	    goto tryagain;
	} else {
	    /* we can queue it; reset current overflow counter */
	    dpp->dp_cur_toverflow = 0;
	}
	dpp->dp_tpkts[dpp->dp_thead] = pkt;
	dpp->dp_thead = tnext;
	return;
    }
    assert(dpp->dp_thead == dpp->dp_ttail);

    /* send packet immediately */
    dpp->dp_tready = FALSE;
    length = pkt->p_contents.pc_totsize;
    assert(length >= 60 && length <= 1514);
    (*dpi->dpi_putpkt)(&dpp->dp_conf, pkt, dpp->dp_conf.dpc_tpsr);
    assert((in_byte(dpreg + DP_CR) & CR_TXP) == 0);
    disable();
    out_byte(dpreg + DP_TBCR0, length & 0xFF);
    out_byte(dpreg + DP_TBCR1, (length >> 8) & 0xFF);
    out_byte(dpreg + DP_CR, CR_TXP|CR_DM_ABORT|CR_STA);
    enable();
    pkt_discard(pkt);
}

/*
 * Set ethernet multicast address
 */
int
dp_setmc(ifno, list)
    int ifno;
    ethmcast_p list;
{
#ifndef NOMULTICAST
    register int dpreg, creg;
    register dpd_p dpp;
#ifdef notyet
    uint32 crc, bits;
    ethmcast_p p;
#endif
    int i;

    assert(ifno < ndp);
    dpp = &dp_data[ifno];
    assert(dpp->dp_conf.dpc_irq != 0);

    /*
     * I know this is all wrong, but simple haven't got the time
     * to properly fix it right now, neither do I understand the
     * musterious ways of this chip. Two boards, both same type
     * go through exactly through the same type of events, set up
     * the same multicast mask, one of them receives all multicasts
     * and the other sees nothing ...
     */
#ifdef notyet
    for(i = 0; i < 8; i++) dpp->dp_maddr[i] = 0;
    for(p = list; p != (ethmcast_p) 0; p = p->em_next) {
        crc = EtherCRC((unsigned char *) p->em_addr, 6);
        bits = (crc >> 26) & 0x3F;
	dpp->dp_maddr[bits >> 3] |= 1 << (bits & 07);
    }
#else /* yet */
    if (list != (ethmcast_p) 0)
	for(i = 0; i < 8; i++) dpp->dp_maddr[i] = 0xFF;
#endif /* yet */

#ifndef NDEBUG
    if (dp_debug > 0) {
	printf("dp_setmc(%d), mask is ", ifno);
	for (i = 0; i < 8; i++)
	    printf(" %02x", dpp->dp_maddr[i] & 0xFF);
	printf("\n");
    }
#endif

    disable();
    dpreg = dpp->dp_conf.dpc_dpreg;
    creg = in_byte(dpreg + DP_CR);
    out_byte(dpreg + DP_CR, CR_PS_P1|creg);
    out_byte(dpreg + DP_MAR0, (int) dpp->dp_maddr[0]);
    out_byte(dpreg + DP_MAR1, (int) dpp->dp_maddr[1]);
    out_byte(dpreg + DP_MAR2, (int) dpp->dp_maddr[2]);
    out_byte(dpreg + DP_MAR3, (int) dpp->dp_maddr[3]);
    out_byte(dpreg + DP_MAR4, (int) dpp->dp_maddr[4]);
    out_byte(dpreg + DP_MAR5, (int) dpp->dp_maddr[5]);
    out_byte(dpreg + DP_MAR6, (int) dpp->dp_maddr[6]);
    out_byte(dpreg + DP_MAR7, (int) dpp->dp_maddr[7]);
    out_byte(dpreg + DP_CR, CR_PS_P0|creg);
    enable();
#endif
}

/*
 * An dp8390 interrupt has occurred. Try determine which adapter
 * caused the interrupt and handle according to the interrupt status. 
 */
/* ARGSUSED */
void
dp_intr(irq)
    int irq;
{
    register int dpreg, isr;
    register dpd_p dpp;

    for (dpp = dp_data; dpp < &dp_data[ndp]; dpp++) {
	if (dpp->dp_conf.dpc_irq == irq) {
	    register int count;
	    int nreceived = 0;

	    dpreg = dpp->dp_conf.dpc_dpreg;
	    /* For safety we put a limit on the number of interrupts
	     * we handle in one sweep.
	     */
	    for (count = 0;
		 count < DP_MAX_INTR && (isr = in_byte(dpreg + DP_ISR)) != 0;
		 count++)
	    {
		/* first the normal interrupts */
		if (isr & ISR_OVW) dp_ovw_intr(dpp);
		if (isr & ISR_PRX) {
		    /* Only need to enqueue a single dp_received, since that
		     * will pick up all packets available.
		     */
		    if (nreceived++ == 0) {
			enqueue(dp_received, (long)dpp);
		    }
		}
		if (isr & ISR_PTX) enqueue(dp_transmitted, (long) dpp);
		out_byte(dpreg + DP_ISR, isr); /* acknowledge */
#ifdef STATISTICS
		/* gather statistic information */
		if (isr & ISR_RXE) {
		    /* examine status register */
		    int rsr = in_byte(dpreg + DP_RSR);
		    if (rsr & RSR_CRC) dpp->dp_stat.dps_crc++;
		    if (rsr & RSR_FAE) dpp->dp_stat.dps_align++;
		    if (rsr & RSR_MPA) dpp->dp_stat.dps_lost++;
		}
		if (isr & (ISR_PTX|ISR_TXE)) {
		    /* examine status register */
		    int tsr = in_byte(dpreg + DP_TSR);
		    if (tsr & TSR_PTX) dpp->dp_stat.dps_written++;
		    /* deferred count seems to be garbaged */
		    if (tsr & TSR_DFR) dpp->dp_stat.dps_deferred++;
		    if (tsr & TSR_COL) dpp->dp_stat.dps_collisions++;
		    if (tsr & TSR_ABT) dpp->dp_stat.dps_xcollisions++;
		    if (tsr & TSR_CRS) dpp->dp_stat.dps_carlost++;
		    if (tsr & TSR_FU)  dpp->dp_stat.dps_fifo++;
		    if (tsr & TSR_CDH) dpp->dp_stat.dps_heartbeat++;
		    if (tsr & TSR_OWC) dpp->dp_stat.dps_lcol++;
		}
		if (isr & ISR_CNT) dp_count(dpp);
#endif /* STATISTICS */
	    }
#ifndef NDEBUG
	    if (count >= DP_MAX_INTR / 2) {
	        if (dp_debug > 0) {
		    printf("dp #%d: %d interrupts, %d packets\n",
			   dp_nr(dpp), count, nreceived);
		}
	    }
#endif
	    if (count >= DP_MAX_INTR) {
		/* Reboot chip if it seems to have gone berserk. */
	        printf("dp #%d: excessive interrupts\n", dp_nr(dpp));
#ifdef STATISTICS
		dp_netdump((char *) 0, (char *) 0);
#endif
		dp8390_init(dpp);
	    }
	}
    }
}

/*
 * Over write warning: On board buffer is full. This is very
 * serious since it implies that there are lost packets. This
 * deteriorates the performance of the FLIP blast protocol.
 */
void
dp_ovw_intr(dpp)
    register dpd_p dpp;
{
    register int dpreg, n;
    void dp_overwrite();

#ifndef NDEBUG
    if (dp_debug > 2 && ((1 << dp_nr(dpp)) & dp_unitmask))
	printf("dp_ovw_intr(): dp #%d\n", dp_nr(dpp));
#endif
    /* stop chip from receiving packets */
    dpreg = dpp->dp_conf.dpc_dpreg;
    out_byte(dpreg + DP_CR, CR_STP|CR_DM_ABORT);
    out_byte(dpreg + DP_RBCR0, 0);
    out_byte(dpreg + DP_RBCR1, 0);
    for (n = 0; (in_byte(dpreg + DP_ISR) & ISR_RST) == 0; n++) {
	if (n > 0x1000) {
	    printf("dp #%d: no status after overwrite\n", dp_nr(dpp));
	    break;
	}
    }

    out_byte(dpreg + DP_TCR, TCR_1EXTERNAL|TCR_OFST);
    out_byte(dpreg + DP_CR, CR_STA|CR_DM_ABORT);
    enqueue(dp_overwrite, (long) dpp);
}

void
dp_overwrite(lpar)
    long lpar;
{
    register dpd_p dpp = (dpd_p) lpar;
    register int dpreg = dpp->dp_conf.dpc_dpreg;

    dp_received(dpp);
#ifdef STATISTICS
    dpp->dp_stat.dps_ovw++;
    dpp->dp_stat.dps_lost += in_byte(dpreg + DP_CNTR2);
#endif
    out_byte(dpreg + DP_TCR, TCR_NORMAL|TCR_OFST);
#ifndef NDEBUG
    if (dp_debug > 0) {
	printf("dp #%d: over write warning\n", dp_nr(dpp));
    }
#endif
}

/*
 * Receive an Ethernet packet. We're very defensive here, as soon as
 * we detect an anomally we reset the chip the hard way.
 */
void
dp_received(dpp)
    register dpd_p dpp;
{
    register int dpreg = dpp->dp_conf.dpc_dpreg;
    register int curpage, pageno, nextpage;
    register dphead_p hp;
    register dpc_p dpc;
    register pkt_p pkt;
    register int length;
    int warned = FALSE;
    int count;
    dphead_t head;
    dpi_p dpi;

#ifndef NDEBUG
    if (dp_debug > 3 && ((1 << dp_nr(dpp)) & dp_unitmask))
	printf("dp_received(): dp #%d\n", dp_nr(dpp));
#endif

    /* some useful pointers */
    dpc = &dpp->dp_conf;
    dpi = &dp_info[dp_nr(dpp)];

    /* get current page number */
    pageno = in_byte(dpreg + DP_BNRY) + 1;
    if (pageno == dpc->dpc_pstop)
	pageno = dpc->dpc_pstart;
    compare(pageno, >=, dpc->dpc_pstart);
    compare(pageno, <, dpc->dpc_pstop);

    /*
     * Get all incoming packets from the dp8390 chip. I've done my best to
     * keep register set 0 current most of the time, but unfortunately the
     * current page register is in page set 1. So we have to disabled ints
     * so that the interrupt handler cannot intervene.
     */
    for (count = 0; count < DP_MAX_INTR; count++) {
	disable();
	out_byte(dpreg + DP_CR, CR_PS_P1);
	curpage = in_byte(dpreg + DP_CURR);
	out_byte(dpreg + DP_CR, CR_PS_P0);
	enable();
	assert(curpage >= dpc->dpc_pstart);
	assert(curpage <= dpc->dpc_pstop);
	if (pageno == curpage) break;

	/* get header for current packet */
	if (!(hp = (*dpi->dpi_gethead)(dpc, pageno, &head))) {
	    printf("dp #%d: get header failed\n", dp_nr(dpp));
	    dp8390_init(dpp);
	    return;
	}
	nextpage = hp->dph_next;

	/* The following check used to be an assertion */
	if (! (nextpage >= dpc->dpc_pstart && nextpage < dpc->dpc_pstop)) {
	    printf("dp #%d: nextpage = %d, dpc_pstart = %d, dpc_pstop = %d\n",
		   dp_nr(dpp), nextpage, dpc->dpc_pstart, dpc->dpc_pstop);
	    dp8390_init(dpp);
	    return;
	}

	/* sanity check for this message */
	if ((hp->dph_status & (RSR_PRX|RSR_CRC|RSR_FAE|RSR_MPA)) != RSR_PRX) {
	    printf("dp #%d: unexpected status 0x%x\n",
		   dp_nr(dpp), hp->dph_status);
	    dp8390_init(dpp);
	    return;
	}

#ifdef STATISTICS
	/* update received statistics */
	dpp->dp_stat.dps_read++;
#endif

	/* compute length of packet data */
	length = (((hp->dph_rbch & 0xFF) << 8) +
	    (hp->dph_rbcl & 0xFF)) - sizeof(dphead_t);

	/*
	 * DP8390 BUG: the length field is sometimes garbled. We
	 * try to make up for it by doing a best effort estimate
	 * (hoping that the higher layers will make up for the
	 * excess data).
	 */
	if (length > 1514) {
	    register int npages;

	    if (nextpage < pageno && nextpage > dpc->dpc_pstart)
		npages = dpc->dpc_pstop - pageno +
		    nextpage - dpc->dpc_pstart;
	    else
		npages = nextpage - pageno;

	    /*
	     * This is really weird, some sites really experience
	     * packets that are >> 1514. Reseting the chip and hope
	     * that the problem goes away is the only thing we can
	     * do right now.
	     */
	    if (npages < 0 || npages > 6) {
		printf("dp #%d: weird packet size %d (pages %d)\n",
		      dp_nr(dpp), length, npages);
		dp8390_init(dpp);
		return;
	    }
	    if ((length = npages << 8 + sizeof(*hp)) > 1514)
		length = 1514;
	}
	if (length < 60) {
	    printf("dp #%d: weird packet size %d\n", dp_nr(dpp), length);
	    /* don't trust this */
	    dp8390_init(dpp);
	    return;
	}

	/* store ethernet message into a packet */
	PKT_GET(pkt, &dpp->dp_rpool);
	if (pkt != (pkt_p)0) {
	    /*
	     * Here we pretend to know all about pkt's mysterious ways
	     */
	    assert(pkt->p_contents.pc_buffer);
	    pkt->p_admin.pa_size = RCVETHPKTSIZE;
	    proto_setup_input(pkt, eh_t);
	    pkt->p_contents.pc_totsize =
		pkt->p_contents.pc_dirsize = length;

	    /* collect packet ... and deliver it */
	    if (!(*dpi->dpi_getpkt)(dpc, pkt, pageno, length)) {
	        printf("dp #%d: get packet failed\n", dp_nr(dpp));
		pkt_discard(pkt);
	        dp8390_init(dpp);
	        return;
	    }
	    eth_arrived(dpp->dp_softifno, pkt);
	} else if (!warned) {
	    warned = TRUE;
	    printf("dp #%d: out of receive packets\n", dp_nr(dpp));
	}

	/* release on board buffer(s) */
	if (nextpage == dpc->dpc_pstart)
	    out_byte(dpreg + DP_BNRY, (int) dpc->dpc_pstop - 1);
	else
	    out_byte(dpreg + DP_BNRY, nextpage - 1);
	pageno = nextpage;

	/* packet out of receive ring bounds */
	if (pageno < dpc->dpc_pstart || pageno >= dpc->dpc_pstop) {
	    printf("dp #%d: dp8390: page 0x%x: ", dp_nr(dpp), pageno&0xFF);
	    disable();
	    out_byte(dpreg + DP_CR, CR_PS_P1);
	    curpage = in_byte(dpreg + DP_CURR);
	    out_byte(dpreg + DP_CR, CR_PS_P0);
	    enable();
	    pageno = in_byte(dpreg + DP_BNRY) + 1;
	    printf("curr = 0x%x, bnry = 0x%x, pstart = 0x%x, pstop = 0x%x\n",
		curpage&0xFF, pageno&0xFF, dpc->dpc_pstart, dpc->dpc_pstop);
	    dp8390_init(dpp);
	    break;
	}
    }
    if (count >= DP_MAX_INTR) {
	/* sanity check; shouldn't happen */
	printf("dp #%d: more than %d packets?\n", dp_nr(dpp), count);
    }
}

/*
 * Upon a packet sent interrupt the transmit packet queue is examined
 * for other packets to send. If none is available a flag is set so
 * that dp_send can do it immediately.
 */
void
dp_transmitted(lpar)
    long lpar;
{
    register int length;
    register dpd_p dpp;
    register int dpreg;
    register pkt_p pkt;
    dpi_p dpi;

    dpp = (dpd_p) lpar;
    dpreg = dpp->dp_conf.dpc_dpreg;
    dpi = &dp_info[dp_nr(dpp)];

#ifndef NDEBUG
    if (dp_debug > 3 && ((1 << dp_nr(dpp)) & dp_unitmask))
	printf("dp_transmitted(): dp #%d\n", dp_nr(dpp));
#endif
#ifdef STATISTICS
    dpp->dp_stat.dps_written++;
#endif

    /*
     * This is actually an error, but we're not going to worry about
     * that here. dp_send will detect the frozen transmitter eventually.
     */
    if (in_byte(dpreg + DP_CR) & CR_TXP) {
#ifdef STATISTICS
	dpp->dp_stat.dps_frozen++;
#endif
	return;
    }

    /*
     * This queue is managed by routines that all run at high level,
     * so there is no need for mutexes (the wonders of non-preemptive
     * scheduling).
     */
    if (dpp->dp_thead != dpp->dp_ttail) {
	assert(!dpp->dp_tready);
	pkt = dpp->dp_tpkts[dpp->dp_ttail];
	assert(pkt);
	dpp->dp_tpkts[dpp->dp_ttail] = 0;
	if (++dpp->dp_ttail == TQUEUESIZE) dpp->dp_ttail = 0;
	length = pkt->p_contents.pc_totsize;
	assert(length >= 60 && length <= 1514);
	(*dpi->dpi_putpkt)(&dpp->dp_conf, pkt, dpp->dp_conf.dpc_tpsr);
	assert((in_byte(dpreg + DP_CR) & CR_TXP) == 0);
	disable();
	out_byte(dpreg + DP_TBCR0, length & 0xFF);
	out_byte(dpreg + DP_TBCR1, (length >> 8) & 0xFF);
	out_byte(dpreg + DP_CR, CR_TXP|CR_DM_ABORT|CR_STA);
	enable();
	pkt_discard(pkt);
    } else
	dpp->dp_tready = TRUE;
}

/*
 * Stop ethernet hardware
 */
int
dp_stop(ifno)
    int ifno;
{
    register dpd_p dpp;
    register dpi_p dpi;

#ifndef NDEBUG
    if (dp_debug > 1)
	printf("dp #%d: stopped ethernet board\n", ifno);
#endif
    assert(ifno < ndp);
    dpi = &dp_info[ifno];
    if ((dpp = &dp_data[ifno])->dp_conf.dpc_irq != 0) {
	pic_disable(dpp->dp_conf.dpc_irq);
	out_byte(dpp->dp_conf.dpc_dpreg + DP_CR, CR_STP|CR_DM_ABORT);
	(*dpi->dpi_stop)(&dpp->dp_conf); /* board specific stop routine */
    }
}

#ifdef STATISTICS
/*
 * Flush dp8390 counters
 */
void
dp_count(dpp)
    register dpd_p dpp;
{
    register int dpreg = dpp->dp_conf.dpc_dpreg;

    dpp->dp_stat.dps_collisions += in_byte(dpreg + DP_NCR);
    dpp->dp_stat.dps_frame += in_byte(dpreg + DP_CNTR0);
    dpp->dp_stat.dps_crc += in_byte(dpreg + DP_CNTR1);
    dpp->dp_stat.dps_lost += in_byte(dpreg + DP_CNTR2);
}

/*
 * Dump dp8390 statistic information for every adapter
 */
int
dp_netdump(begin, end)
    char *begin, *end;
{
    register char *p;
    int i;

    p = bprintf(begin, end, "======== DP statistics =======\n");
    for (i = 0; i < ndp; i++) {
	register dpd_p dpp = &dp_data[i];
	register dpstat_p dpsp = &dpp->dp_stat;
#ifndef NDEBUG
	register int dpreg = dpp->dp_conf.dpc_dpreg;
#endif
	int n;

	if (dpp->dp_conf.dpc_irq == 0) continue;
	dp_count(dpp); /* flush counters */
	p = bprintf(p, end, "dp #%d:\n", i);
#ifndef NDEBUG
	if (dp_debug > 1) {
	    p = bprintf(p, end, "cr      %7lx ",  in_byte(dpreg + DP_CR));
	    p = bprintf(p, end, "bnry    %7lx ",  in_byte(dpreg + DP_BNRY));
	    p = bprintf(p, end, "tsr     %7lx ",  in_byte(dpreg + DP_TSR));
	    p = bprintf(p, end, "isr     %7lx ",  in_byte(dpreg + DP_ISR));
	    p = bprintf(p, end, "rsr     %7lx\n", in_byte(dpreg + DP_RSR));
	}
#endif
	p = bprintf(p, end, "read    %7ld ",   dpsp->dps_read);
	p = bprintf(p, end, "written %7ld ",   dpsp->dps_written);
	p = bprintf(p, end, "frame   %7ld ",   dpsp->dps_frame);
	p = bprintf(p, end, "crc     %7ld ",   dpsp->dps_crc);
	p = bprintf(p, end, "lost    %7ld\n",  dpsp->dps_lost);
	p = bprintf(p, end, "deff    %7ld ",   dpsp->dps_deferred);
	p = bprintf(p, end, "coll    %7ld ",   dpsp->dps_collisions);
	p = bprintf(p, end, "acoll   %7ld ",   dpsp->dps_xcollisions);
	p = bprintf(p, end, "carlost %7ld ",   dpsp->dps_carlost);
	p = bprintf(p, end, "fifo    %7ld\n",  dpsp->dps_fifo);
	p = bprintf(p, end, "heart   %7ld ",   dpsp->dps_heartbeat);
	p = bprintf(p, end, "lcoll   %7ld ",   dpsp->dps_lcol);
	p = bprintf(p, end, "ovw     %7ld ",   dpsp->dps_ovw);
	p = bprintf(p, end, "frozen  %7ld ",   dpsp->dps_frozen);
	p = bprintf(p, end, "align   %7ld\n",  dpsp->dps_align);
	p = bprintf(p, end, "curtovf %7ld ",   dpp->dp_cur_toverflow);
	p = bprintf(p, end, "tottovf %7ld ",   dpp->dp_tot_toverflow);
	n = dpp->dp_thead - dpp->dp_ttail;
	if (dpp->dp_thead < dpp->dp_ttail) n += TQUEUESIZE;
	p = bprintf(p, end, "tqueued %7ld\n\n", n);
    }
    return p - begin;
}
#endif /* STATISTICS */
