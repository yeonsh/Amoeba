/*	@(#)ne2100.c	1.3	96/03/07 14:08:46 */
/*
 * ne2100.c
 *
 * A very simple network driver for NE2100 boards that polls.
 *
 * Copyright (c) 1992 by Leendert van Doorn
 */
#include "assert.h"
#include "types.h"
#include "packet.h"
#include "ether.h"
#include "lance.h"

/* configurable parameters */
#define	NE_BASEREG	0x300		/* base register */
#define	NE_DMACHANNEL	5		/* DMA channel */

/* Lance register offsets */
#define LA_CSR          (NE_BASEREG+0x10)
#define LA_CSR1         (NE_BASEREG+0x10)
#define LA_CSR2         (NE_BASEREG+0x10)
#define LA_CSR3         (NE_BASEREG+0x10)
#define LA_RAP          (NE_BASEREG+0x12)

/*
 * Some driver specific constants.
 * Take care when tuning, this program only has 32 Kb
 */
#define	LANCEBUFSIZE	1518		/* plus 4 CRC bytes */
#define	MAXLOOP		1000000L	/* arbitrary retry limit */
#define	LOG2NRCVRING	2		/* log2(NRCVRING) */
#define	NRCVRING	(1 << LOG2NRCVRING)

uint8 eth_myaddr[ETH_ADDRSIZE];

static int next_rmd;			/* next receive element */
static initblock_t *initblock;		/* initialization block */
static tmde_t *tmd;			/* transmit ring */
static rmde_t *rmd;			/* receive ring */
static char rbuffer[NRCVRING][LANCEBUFSIZE]; /* receive buffers */

extern uint32 dsseg();
extern char *aalloc();

/*
 * Get ethernet address and compute checksum to be sure
 * that there is a board at this address.
 */
int
eth_init()
{
    uint16 checksum, sum;
    register int i;

    for (i = 0; i < 6; i++)
	eth_myaddr[i] = in_byte(NE_BASEREG + i);

    sum = 0;
    for (i = 0x00; i <= 0x0B; i++)
	sum += in_byte(NE_BASEREG + i);
    for (i = 0x0E; i <= 0xF; i++)
	sum += in_byte(NE_BASEREG + i);
    checksum = in_byte(NE_BASEREG + 0x0C) | (in_byte(NE_BASEREG + 0x0D) << 8);
    if (sum != checksum)
	return 0;

    /* initblock, tmd, and rmd should be 8 byte aligned ! */
    initblock = (initblock_t *) aalloc(sizeof(initblock_t), 8);
    tmd = (tmde_t *) aalloc(sizeof(tmde_t), 8);
    rmd = (rmde_t *) aalloc(NRCVRING * sizeof(rmde_t), 8);
    eth_reset();
    return 1;
}

/*
 * Reset ethernet board (i.e. after a timeout)
 */
void
eth_reset()
{
    register long l;
    uint32 addr;
    int i;

    /* program DMA chip */
    dma_cascade(NE_DMACHANNEL);

    /* stop the chip, and make sure it did */
    out_word(LA_RAP, RDP_CSR0);
    out_word(LA_CSR, CSR_STOP);
    for (l = 0; (in_word(LA_CSR) & CSR_STOP) == 0; l++) {
	if (l >= MAXLOOP) {
	    printf("Lance failed to stop\n");
	    return;
	}
    }

    /* fill lance initialization block */
    bzero(initblock, sizeof(initblock_t));

    /* set my ethernet address */
    initblock->ib_padr[0] = eth_myaddr[0];
    initblock->ib_padr[1] = eth_myaddr[1];
    initblock->ib_padr[2] = eth_myaddr[2];
    initblock->ib_padr[3] = eth_myaddr[3];
    initblock->ib_padr[4] = eth_myaddr[4];
    initblock->ib_padr[5] = eth_myaddr[5];

    /* receive ring pointer */
    addr = (dsseg() << 4) + (uint32)rmd;
    initblock->ib_rdralow = (uint16)addr;
    initblock->ib_rdrahigh = (uint8)(addr >> 16);
    initblock->ib_rlen = LOG2NRCVRING << 5;

    /* transmit ring with one element */
    addr = (dsseg() << 4) + (uint32)tmd;
    initblock->ib_tdralow = (uint16)addr;
    initblock->ib_tdrahigh = (uint8)(addr >> 16);
    initblock->ib_tlen = 0 << 5;

    /* setup the receive ring entries */
    for (next_rmd = 0, i = 0; i < NRCVRING; i++) {
	addr = (dsseg() << 4) + (uint32)&rbuffer[i];
	rmd[i].rmd_ladr = (uint16)addr;
	rmd[i].rmd_hadr = (uint8)(addr >> 16);
	rmd[i].rmd_mcnt = 0;
	rmd[i].rmd_bcnt = -LANCEBUFSIZE;
	rmd[i].rmd_flags = RMD_OWN;
    }

    /* zero transmit ring */
    bzero(tmd, sizeof(tmde_t));

    /* give lance the init block */
    addr = (dsseg() << 4) + (uint32)initblock;
    out_word(LA_RAP, RDP_CSR1);
    out_word(LA_CSR1, (uint16)addr);
    out_word(LA_RAP, RDP_CSR2);
    out_word(LA_CSR2, (int8)(addr >> 16));
    out_word(LA_RAP, RDP_CSR3);
    out_word(LA_CSR3, 0);

    /* and initialize it */
    out_word(LA_RAP, RDP_CSR0);
    out_word(LA_CSR, CSR_INIT|CSR_STRT);

    /* wait for the lance to complete initialization and fire it up */
    for (l = 0; (in_word(LA_CSR) & CSR_IDON) == 0; l++) {
	if (l >= MAXLOOP) {
	    printf("Lance failed to initialize\n");
	    break;
	}
    }
    for (l=0; (in_word(LA_CSR)&(CSR_TXON|CSR_RXON))!=(CSR_TXON|CSR_RXON); l++) {
	if (l >= MAXLOOP) {
	    printf("Lance not started\n");
	    break;
	}
    }
}

/*
 * Stop ethernet board
 */
void
eth_stop()
{
    register long l;

    /* stop chip and disable DMA access */
    out_word(LA_RAP, RDP_CSR0);
    out_word(LA_CSR, CSR_STOP);
    for (l = 0; (in_word(LA_CSR) & CSR_STOP) == 0; l++) {
	if (l >= MAXLOOP) {
	    printf("Lance failed to stop\n");
	    break;
	}
    }
    dma_done(NE_DMACHANNEL);
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
    register long l;
    uint32 addr;
    uint16 csr;

    /* add ethernet header and fill in source & destination */
    pkt->pkt_len += sizeof(ethhdr_t);
    pkt->pkt_offset -= sizeof(ethhdr_t);
    ep = (ethhdr_t *) pkt->pkt_offset;
    ep->eth_proto = htons(proto);
    bcopy(dest, ep->eth_dst, ETH_ADDRSIZE);
    bcopy(eth_myaddr, ep->eth_src, ETH_ADDRSIZE);
    if (pkt->pkt_len < 60)
	pkt->pkt_len = 60;
    assert(pkt->pkt_len <= 1514);

    /* set up transmit ring element */
    assert((tmd->tmd_flags & TMD_OWN) == 0);
    addr = (dsseg() << 4) + (uint32)pkt->pkt_offset;
    assert((addr & 1) == 0);
    tmd->tmd_ladr = (uint16)addr;
    tmd->tmd_hadr = (uint8)(addr >> 16);
    tmd->tmd_bcnt = -pkt->pkt_len;
    tmd->tmd_err = 0;
    tmd->tmd_flags = TMD_OWN|TMD_STP|TMD_ENP;

    /* start transmission */
    out_word(LA_CSR, CSR_TDMD);

    /* wait for interrupt and acknowledge it */
    for (l = 0; l < MAXLOOP; l++) {
	if ((csr = in_word(LA_CSR)) & CSR_TINT) {
	    out_word(LA_CSR, CSR_TINT);
	    break;
	}
    }
}

/*
 * Poll the LANCE just see if there's an Ethernet packet
 * available. If there is, its contents is returned in a
 * pkt structure, otherwise a nil pointer is returned.
 */
packet_t *
eth_receive()
{
    register packet_t *pkt;
    register rmde_t *rp;
    uint32 addr;
    uint16 csr;

    pkt = (packet_t *)0;
    if ((csr = in_word(LA_CSR)) & CSR_RINT) {
	out_word(LA_CSR, CSR_RINT);
	assert(next_rmd >= 0);
	assert(next_rmd <= NRCVRING);
	rp = &rmd[next_rmd];
	if ((rp->rmd_flags & ~RMD_OFLO) == (RMD_STP|RMD_ENP)) {
	    pkt = pkt_alloc(0);
	    pkt->pkt_len = rp->rmd_mcnt - 4;
	    assert(pkt->pkt_len >= 0);
	    assert(pkt->pkt_len < PKT_DATASIZE);
	    bcopy(rbuffer[next_rmd], pkt->pkt_offset, pkt->pkt_len);
	    /* give packet back to the lance */
	    rp->rmd_bcnt = -LANCEBUFSIZE;
	    rp->rmd_mcnt = 0;
	    rp->rmd_flags = RMD_OWN;
	}
	next_rmd = (next_rmd + 1) & (NRCVRING - 1);
    }
    return pkt;
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

/*
 * Program DMA channel 'chan' for cascade mode
 */
static void
dma_cascade(chan)
    int chan;
{
    assert(chan >= 0);
    assert(chan <= 7);
    if (chan >= 0 && chan <= 3) {
	out_byte(0x0B, 0xC0 | (chan & 03));
	out_byte(0x0A, chan & 03);
    } else {
	out_byte(0xD6, 0xC0 | ((chan - 4) & 03));
	out_byte(0xD4, (chan - 4) & 03);
    }
}

/*
 * Disable DMA for channel 'chan'
 */
static void
dma_done(chan)
    int chan;
{
    assert(chan >= 0);
    assert(chan <= 7);
    if (chan >= 0 && chan <= 3)
	out_byte(0x0A, 0x04 | (chan & 03));
    else
	out_byte(0xD4, 0x04 | ((chan - 4 ) & 03));
}
