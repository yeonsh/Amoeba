/*	@(#)glance.c	1.13	96/02/27 13:48:25 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

typedef unsigned short unshort;

#include <bool.h>
#include <string.h>
#include "amoeba.h"
#include "assert.h"
INIT_ASSERT
#include "debug.h"
#include "machdep.h"
#include "byteorder.h"
#include "interrupt.h"
#if !defined(ISA) && !defined(MCA)
#include "memaccess.h"
#endif /* !defined(ISA) && !defined(MCA) */
#include "glance.h"
#include "glastat.h"
#include "glainfo.h"
#include "sys/flip/packet.h"
#include "sys/flip/ethpreamble.h"
#include "ethif.h"
#include "sys/proto.h"
#include "sys/flip/measure.h"

#include "map.h"

extern unsigned long EtherCRC();
extern void computemask();

/* Forward declarations */
static void la_reboot();
static la_stuff_in_rcv_ring();

/*
 * Ahum, I'm not proud of this either ...
 */
#ifdef CHIPDEBUG
#if defined(ISA) || defined(MCA)
static void
ladbio(output, addr, field, value, ioreg)
    lareg_p addr;
    char *field;
    unshort value;
    int ioreg;
{
    printf("%s: %X->%s (%x): %x\n", output? "OUT" : "IN ",
	addr, field, ioreg, output? value : in_word(ioreg));
}

#define	input(p, f)		(ladbio(0, p, #f, 0, p + f), in_word(p + f))
#define	output(p, f, v)		(ladbio(1, p, #f, v, p + f), out_word(p + f, v))

#else /* !defined(ISA) && !defined(MCA) */
static void
ladbio(output, addr, field, value)
    lareg_p addr;
    char *field;
    unshort value;
{
    printf("%s: %X->%s: %x\n", output? "OUT" : "IN ", addr, field, value);
}
#define input(p, f)		(ladbio(0, p,"f",(p)->f),((p)->f))
#define output(p, f, v)		(ladbio(1, p,"f",v), (p)->f = (v))
#endif /* !defined(ISA) && !defined(MCA) */
#else /* !defined(CHIPDEBUG) */
#if defined(ISA) || defined(MCA)
void dma_cascade();
void dma_done();
void pic_enable();
void pic_disable();

#ifndef __GNUC__
void out_word();
#endif

#define	input(p, f)		(in_word(p + f))
#define	output(p, f, v)		(out_word(p + f, v))
#ifdef __GNUC__
static __inline__ unsigned int
in_word(p)
short p;
{
    unsigned short r;
    __asm__ __volatile__("inw %1,%0": "=a" (r) : "d" (p));
    return r;
}
static __inline__ void
out_word(p, v)
short p, v;
{
   __asm__ __volatile__("outw %0,%1" : :"a" (v), "d" (p));
}
#endif /* __GNU__ */
#else /* !defined(ISA) && !defined(MCA) */
#define input(p, f)		mem_get_short(&(p)->f)
#define output(p, f, v)		mem_put_short(&(p)->f, (short) v)
#endif /* !defined(ISA) && !defined(MCA) */
#endif /* CHIPDEBUG */

#define SNDETHPKTSIZE	1514
#define RCVETHPKTSIZE	(1518+PKTBEGHDR)
#ifndef NOVERFLOW
#define NOVERFLOW	128
#endif

typedef struct latpkt {
	char 	l_data[SNDETHPKTSIZE];
} *latpkt_p, latpkt_t;

typedef
struct la_data {
	int	lad_hardifno;		/* Index in la_data array, saves divs */
	int	lad_softifno;		/* Identifier for upper levels */
	lareg_p lad_device;		/* Pointer to registers */
	IB_p	lad_ib;			/* Pointer to InitBlock */
	int	lad_csr3;		/* Value to stuff in csr3 */
/* Receive side management */
	rmde_p	lad_rcvring;		/* Pointer to receive ring */
	pkt_p	*lad_rpkts;		/* Array of receive packets */
	int	lad_rsize;		/* Size of receive ring */
	int	lad_rmask;		/* Mask of receive ring index */
	int	lad_rhead;		/* Index of next input packet */
	int	lad_rtail;		/* Index of first unfilled slot */
	int	lad_rinuse;		/* Number of packets in ring */
	pool_t	lad_rpool;		/* Packet pool */
/* Transmit side management */
	tmde_p	lad_tmtring;		/* Pointer to transmit ring */
	pkt_p	*lad_tpkts;		/* Array of transmit packets */
	latpkt_p  lad_latpkts;		/* lance array of transmit buffers */
	pool_t  lad_tpool;		/* transmit pool */
	int	lad_tsize;		/* Size of transmit ring */
	int	lad_tmask;		/* Mask of transmit ring index */
	int	lad_latsize;		/* Size of lance transmit ring */
	pkt_p	lad_first;		/* sending overflow queue */
	pkt_p	*lad_last;
	int	lad_noverflow;
	int	lad_thead;		/* First packet to be transmitted */
	int	lad_ttail;		/* First unfilled slot */
	int	lad_tinuse;		/* Number of packets in ring */
	int	lad_tdontchain;		/* Don't use chaining */
	int	lad_tchainmin;		/* Minimum size to consider chaining */
	int	lad_tchainhead;		/* Size of first chain element */
/* General stuff */
	int	lad_reinit;		/* re-init needed? */
	int	lad_up;			/* Are we up and running? */
	int	lad_intrarg;		/* Argument we will get at interrupt */
	uint32 (*lad_addrconv)();	/* Adress conversion */
#ifdef STATISTICS
	lastat	lad_stat;		/* Statistics structure */
#endif
	int lad_renq;
	int lad_senq;
} lad_t, *lad_p;


static int nlance;			/* number of interfaces */
static lad_p la_data;			/* per interface data */
static lad_p la_enddata;		/* end of la_data array */
extern lai_t la_info[];			/* per interface config info */

/*
 * Error print
 */
#define la_error(lap, errorstring) \
	DPRINTF(0, ("Lance %d error: %s\n", lap->lad_hardifno, errorstring))


/*
 * Interrupt routines.
 * It is possible for all interfaces to share a vector.
 * It is more efficient to have separate vectors.
 */
void la_i_rerror(lap, flags)
    register lad_p lap;
    register unshort flags;
{
    if (flags & RMD_FRAM) {
	STINC(ls_fram);
	la_error(lap, "Framing error");
    } else if (flags & RMD_CRC) {
	STINC(ls_crc);
	la_error(lap, "CRC error");
    }
    if (flags & RMD_BUFF) {
	STINC(ls_buff);
	la_error(lap, "Buffer error");
    }
    if (flags & RMD_OFLO) {
	STINC(ls_oflo);
	la_error(lap, "Receiver overflow error");
    }
}


void la_i_rint(lpar)
    long lpar;
{
    register rmde_p rdp;
    register pkt_p pkt;
    register lad_p lap;
    
    lap = (lad_p) lpar;
    lap->lad_renq= 0;
    while (lap->lad_rinuse>0) {
	rdp = lap->lad_rcvring+lap->lad_rhead;
	if (rdp->rmd_flags&RMD_OWN)	/* belongs to chip */
	{
	    if (lap->lad_renq)
	    {
		lap->lad_renq= 0;   /* one more check agains race conditions */
		continue;
	    }
	    break;
	}
	pkt = lap->lad_rpkts[lap->lad_rhead];
#ifdef ACTMSG_INTERRUPT
	if (rdp->rmd_flags == (RMD_STP|RMD_ENP) && 
	  am_interrupt(pkt_offset(pkt), rdp->rmd_mcnt)) {
	    assert(lap->lad_rhead == lap->lad_rtail);
	    BUMPMASK(lap->lad_rtail, lap->lad_rmask);
	    BUMPMASK(lap->lad_rhead, lap->lad_rmask);
	    rdp->rmd_flags = RMD_OWN;
	    rdp->rmd_bcnt = -rdp->rmd_mcnt;
	    rdp->rmd_mcnt = 0;
	    /* it belongs to the chip from now on */
	    continue;
	}
#endif
	lap->lad_rpkts[lap->lad_rhead] = 0;
	BUMPMASK(lap->lad_rhead, lap->lad_rmask);
	lap->lad_rinuse--;

	if (lap->lad_rinuse < 10)
		DPRINTF(2, ("lap->lad_rinuse= %d\n", lap->lad_rinuse));

	if (rdp->rmd_flags == (RMD_STP|RMD_ENP)) {
	    /* good packet */
	    pkt->p_contents.pc_totsize = pkt->p_contents.pc_dirsize = rdp->rmd_mcnt;
	    pkt_wanted(&lap->lad_rpool, 1); /* signal pool for a packet */
	    DPRINTF(3, ("Call eth_arrived(%d, %x) %x\n", lap->lad_softifno, pkt, pkt_offset(pkt)));
	    eth_arrived(lap->lad_softifno, pkt);
	    STINC(ls_read);
	} else {
	    /* something wrong */
	    la_i_rerror(lap, rdp->rmd_flags);
	    pkt_wanted(&lap->lad_rpool, 1); /* signal pool for a packet */
	    pkt_discard(pkt);
	    STINC(ls_dropped);
	}
    }
    lap->lad_renq= 0;
}


void la_i_terror(lap, err)
    register lad_p lap;
    register unshort err;
{
    STINC(ls_oerrors);
    if (err & TMDE_LCOL) {
	/*
	 * Late collision. In principle somebody elses fault,
	 * because if everybody kept to the specs it wouldn't
	 * happen.
	 */
	STINC(ls_lcol);
	la_error(lap, "Late collision");
    }
    if (err & TMDE_LCAR) {
	STINC(ls_lcar);
	la_error(lap, "Lost carrier");
    }
    if (err & TMDE_UFLO) {
	/*
	 * This one suggest memory being too slow for the chip.
	 * If it happens frequently replace your memory-speed
	 * crystal :-).
	 *
	 * Transmitter will stop. Fix that somewhere.
	 */
	STINC(ls_uflo);
	la_error(lap, "Transmit underflow");
    }
    if (err & TMDE_RTRY) {
	/*
	 * More than 16 collisions. Either an extremely busy
	 * network or an unterminated cable. In the last case
	 * the TDR value printed should be multiplied by a number
	 * depending on the type of cable to find distance to the
	 * fault.
	 */
	STINC(ls_rtry);
	STADD(ls_coll, 16);
	la_error(lap, "Transmit aborted due to excessive collisions");
	DPRINTF(0, ("TDR was %d\n", err & TMDE_TDR));
    }
}


void la_i_tint(lpar)
    long lpar;
{
    register tmde_p tdp;
    register unshort flags;
    register lad_p lap;
    pkt_p pkt, next;
    void la_send();
    
    lap = (lad_p) lpar;
    lap->lad_senq = 0;
    while (lap->lad_tinuse) {
	tdp = lap->lad_tmtring+lap->lad_thead;
	flags = tdp->tmd_flags;
	if (flags & TMD_OWN)
	    break;
	if (flags & TMD_ERR)
	    la_i_terror(lap, tdp->tmd_err);
	if (flags & TMD_ENP) {
	    /* handle collision statistics */
	    if (flags & TMD_DEF)
		STINC(ls_def);
	    if (flags & TMD_ONE) {
		STINC(ls_one);
		STINC(ls_coll);
	    }
	    if (flags & TMD_MORE) {
		STINC(ls_more);
		STADD(ls_coll, 2);	/* or more, who knows */
	    }
	    STINC(ls_written);
	    /* give packet back to higher layers */
#ifdef ACTMSG_INTERRUPT
	    if (lap->lad_tpkts[lap->lad_thead] != 0) {
#else
	    assert(lap->lad_tpkts[lap->lad_thead] != 0);
#endif
		pkt_discard(lap->lad_tpkts[lap->lad_thead]);
		lap->lad_tpkts[lap->lad_thead] = 0;
#ifdef ACTMSG_INTERRUPT
	    }
#endif

	}
	BUMPMASK(lap->lad_thead, lap->lad_tmask);
	lap->lad_tinuse--;
    }
    /* walk through transmit overflow queue */
    for(pkt = lap->lad_first; pkt != 0; pkt = next) {
	/* Only send a packet from the overflow queue if there are now at
	 * least two TMD entries available.  Otherwise la_send would queue
	 * the message again, since it doesn't know where it came from.
	 */
	if (lap->lad_tinuse > lap->lad_tsize - 2)
	    break;
	if((next = lap->lad_first = pkt->p_admin.pa_next) == 0)
	    lap->lad_last = &lap->lad_first;
	lap->lad_noverflow--;
	la_send(lap->lad_hardifno, pkt);
    }
}

static int la_reboot_pending;

static void
la_enqueue_reboot(lap)
register lad_p lap;
{
    /* only enqueue a lance reboot if we don't have one pending */
    if (!la_reboot_pending) {
	la_reboot_pending = 1;
	DPRINTF(0, ("la_enqueue: stop lance\n"));
	output(lap->lad_device,LA_CSR,CSR_STOP);
	enqueue(la_reboot, (long) lap);
    }
}

void la_intr(arg)
    int arg;
{
    register lad_p lap;
    register unshort csr;
    register lareg_p device;

    for (lap=la_data; lap<la_enddata; lap++) {
	if (lap->lad_intrarg == arg) {
	    assert(lap->lad_up);
	    device = lap->lad_device;
	    for (csr=input(device, LA_CSR); csr&CSR_INTR;
						csr=input(device, LA_CSR)) {
		
		/*
		 * First the more normal interrupts
		 */
		if (csr&CSR_TINT) {
		    output(device, LA_CSR, CSR_TINT);
		    if (lap->lad_senq == 0) {
			lap->lad_senq = 1;
			enqueue(la_i_tint, (long) lap);
		    }
		}
		if (csr&CSR_RINT) {
#ifdef ACTMSG_INTERRUPT
		    register rmde_p rdp, rdp2;
		    register pkt_t *pkt;
		    uint32 baddr;

		    rdp = lap->lad_rcvring + lap->lad_rhead;
		    for (; lap->lad_renq == 0;) {
			if (rdp->rmd_flags != (RMD_STP|RMD_ENP))
			    break;
			pkt = lap->lad_rpkts[lap->lad_rhead];
			if (!am_interrupt(pkt_offset(pkt), rdp->rmd_mcnt))
			    break;
			lap->lad_rpkts[lap->lad_rhead] = 0;
			BUMPMASK(lap->lad_rhead, lap->lad_rmask);
			rdp2 = lap->lad_rcvring+lap->lad_rtail;
			lap->lad_rpkts[lap->lad_rtail] = pkt;
			BUMPMASK(lap->lad_rtail, lap->lad_rmask);
			baddr = (*lap->lad_addrconv)
			    (pkt_offset(pkt), rdp->rmd_mcnt);
			rdp2->rmd_ladr = baddr;
			rdp2->rmd_hadr = baddr>>16;
			rdp2->rmd_flags = RMD_OWN;
			rdp2->rmd_bcnt = -rdp->rmd_mcnt;
			rdp2->rmd_mcnt = 0;
			/* it belongs to the chip from now on */
			rdp = lap->lad_rcvring + lap->lad_rhead;
		    }
		    output(device, LA_CSR, CSR_RINT);
		    if ((rdp->rmd_flags & RMD_OWN) == 0 && lap->lad_renq == 0) {
			lap->lad_renq = 1;
			BEGIN_MEASURE(ether_rcv);
			enqueue(la_i_rint, (long) lap);
		    }
#else
		    output(device, LA_CSR, CSR_RINT);
		    if (lap->lad_renq == 0) {
			lap->lad_renq = 1;
			BEGIN_MEASURE(ether_rcv);
			enqueue(la_i_rint, (long) lap);
		    }
#endif
		}
		
		/*
		 * The errors
		 */
		if ((csr & (CSR_ERR|CSR_TXON|CSR_RXON)) != (CSR_TXON|CSR_RXON)) {
		    /*
		     * O boy, errors. I wonder whats wrong?
		     */
		    if (csr&CSR_BABL) {
			output(device, LA_CSR, CSR_BABL|CSR_INEA);
			STINC(ls_babl);
			la_error(lap, "babbling error");
		    }
		    if (csr&CSR_CERR) {
			static int ccnt;
			output(device,LA_CSR,CSR_CERR|CSR_INEA);
			STINC(ls_cerr);
			if ((ccnt&0x3F)==0) { /* once every 64 times */
			    la_error(lap,"collision error, check cable");
			    ccnt=1;
			} else
			    ccnt++;
		    }
		    if (csr&CSR_MISS) {
			output(device,LA_CSR,CSR_MISS|CSR_INEA);
			STINC(ls_miss);
#if !TCPIP_DEBUG
			la_error(lap, "missed packet");
#endif
		    }
		    if (csr&CSR_MERR) {
			output(device,LA_CSR,CSR_MERR|CSR_INEA);
			printf("Lance stopped because of memory error\n");
		    }
		}
	    }
	    if ((csr&(CSR_TXON|CSR_RXON)) != (CSR_RXON|CSR_TXON)) {
		if (la_reboot_pending) {
		    printf("Lance reboot pending\n");
		} else {
		    printf("Lance stopped for unknown reason (la_i_intr)\n");
		}
		la_enqueue_reboot(lap);
	    }
	    if (lap->lad_reinit && lap->lad_tinuse == 0) {
		lap->lad_reinit = 0;
		la_enqueue_reboot(lap);
	    }
	    output(device, LA_CSR, CSR_INEA);
	}
    }
}


/*
 * There is a packet buffer available
 */
void la_bufavail(lparam, pkt)
    long lparam;
    pkt_p pkt;
{
    register lad_p lap;
    
    lap = (lad_p) lparam;
    assert (pkt != 0);
    proto_setup_input(pkt, eh_t);
    if (pkt->p_contents.pc_dirsize > 1518)	/* HACK */
	pkt->p_contents.pc_dirsize = 1518;
    la_stuff_in_rcv_ring(lap, pkt);
}

/*
 * Initialization routines. Called once for each interface by eth_init.
 */

#ifdef LANCE_DEBUG
static void check_ibp();
#endif

static int la_boot(lap)
    register lad_p lap;
{
    register lareg_p device;
    register loopcount;
    register zero=0;	/* To prevent 68000 clr instruction on rdp */
    register uint32 baddr;
    
    device = lap->lad_device;

    /*
     * Start configuring chip.
     * Stop the chip, and make sure it did.
     * Put mode in csr3, address of InitBlock in csr1 and csr2,
     * and fire it up through csr0.
     *
     * Actually all these registers are at the same address,
     * and we have to make them accessible through a "pointer" register.
     */
    
    output(device, LA_RAP, RDP_CSR0);	/* point to csr0, stays there */
    output(device, LA_CSR, ~(CSR_INIT|CSR_STRT|CSR_INTR));
    
#define maxloop 10000
    for (loopcount = 0; loopcount < maxloop; loopcount++)
	if (input(device, LA_CSR) & CSR_STOP)
	    break;
    if ((input(device, LA_CSR) & CSR_STOP) == 0) {
	la_error(lap,"failed to stop");
	return 0;
    }

    lap->lad_rhead = 0;
    lap->lad_rtail = 0;
    lap->lad_rinuse = 0;
    
    /*
     * fire up packet allocation
     */
    pkt_setwanted(&lap->lad_rpool, lap->lad_rsize);
    
    lap->lad_thead = 0;
    lap->lad_ttail = 0;
    lap->lad_tinuse = 0;
    
#ifdef LANCE_DEBUG
    /* make sure the initialisation block has not been tampered with */
    check_ibp(lap);
#ifdef STATISTICS
    /* Print network statistics to see if we're still getting unicasts
     * right after the Lance reboot.
     */
    flip_netstat((char *) 0, (char *) 0);
#endif
#endif

    baddr = (*lap->lad_addrconv)(lap->lad_ib, sizeof(*lap->lad_ib)); 
    compare(baddr, <, ADDRESSLIMIT);
    output(device, LA_RAP, RDP_CSR3);	/* point to csr3 */
    output(device, LA_CSR3, lap->lad_csr3);	/* See comment at paddr */
    output(device, LA_RAP, RDP_CSR1);	/* point to csr1 */
    output(device, LA_CSR1, (int) (baddr&0xFFFF));
    output(device, LA_RAP, RDP_CSR2);	/* point to csr2 */
    output(device, LA_CSR2, (int) ((baddr>>16)&0xFFFF));

    output(device, LA_RAP, RDP_CSR0);	/* point to csr0, stays there */
    output(device, LA_CSR, CSR_INIT|CSR_STRT);
    
    for (loopcount=0; loopcount<maxloop && 
	 ((input(device, LA_CSR)&(CSR_IDON|CSR_ERR))==0);
	 loopcount++)
	;
    if ((input(device, LA_CSR)&CSR_IDON)==0) {
	la_error(lap,"failed to fire");
	return 0;	/* We loose the memory though */
    }
    for (loopcount=0; loopcount<maxloop &&
	 ((input(device, LA_CSR)&(CSR_TXON|CSR_RXON)))!=(CSR_TXON|CSR_RXON);
	 loopcount++)
	;
    if ((input(device, LA_CSR)&(CSR_TXON|CSR_RXON))!=(CSR_TXON|CSR_RXON)) {
	la_error(lap, "improperly started");
	return 0;	/* We lose the memory though */
    }
#undef maxloop
    
    /*
     * Well, we did it. Final handshake with the chip and then we
     * get to work.
     */
    output(device, LA_CSR, CSR_BABL|CSR_CERR|CSR_MISS|CSR_IDON|CSR_INEA);

    return 1;
}


static void la_reboot(lparam)
    long lparam;
{
    register lad_p lap;
    register pkt_p pkt;
    register i;
    
    lap = (lad_p) lparam;
    STINC(ls_reboot);
    /*
     * clean up left over packets
     */
    DPRINTF(1, ("la_reboot: r %d t %d\n", lap->lad_rinuse, lap->lad_tinuse));
    pkt_setwanted(&lap->lad_rpool, 0); 	/* we don't want pkts anymore */
    for (i=0; i<lap->lad_rsize; i++) {
	lap->lad_rcvring[i].rmd_flags = 0;
	lap->lad_rcvring[i].rmd_mcnt = 0;/* not actually necessary */
	pkt = lap->lad_rpkts[i];
	if (pkt) {
	    pkt_discard(pkt);
	    lap->lad_rpkts[i] = 0;
	}
    }
    assert(lap->lad_rinuse >= 0 && lap->lad_rinuse <= lap->lad_rsize);
    
    for (i=0; i<lap->lad_tsize; i++) {
	lap->lad_tmtring[i].tmd_flags = 0;
	pkt = lap->lad_tpkts[i];
	if (pkt) {
	    /* just pretend we sent it */
	    pkt_discard(pkt);
	    lap->lad_tpkts[i] = 0;
	}
    }
    if (!la_boot(lap))
	panic("Lance failed to restart");
    la_error(lap, "rebooted");

    /* now allow a new lance reboot */
    la_reboot_pending = 0;
}


int la_setmc(ifno, list)
    int ifno;
    ethmcast_p list;
{
#ifndef NOMULTICAST
    register lad_p lap;
    register IB_p ibp;
    ethmcast_p p;
    int i, lance, equal = 1; 
    unsigned long crc;
    unsigned short mask[4], newmask[4];
    
    assert(ifno<nlance);
    lap = la_data+ifno;
    assert(lap->lad_up);
#ifdef MULTICAST_DONT_FILTER
    /* get all multicast packets by setting the address filter to all-ones */
    for(i =0; i < 4; i++) 
	newmask[i] = 0xffff;
#else
    for(i =0; i < 4; i++) 
	newmask[i] = 0;
    if (list == NULL) {
	DPRINTF(1, ("la_setmc: no multicast addresses\n"));
    } else {
	DPRINTF(1, ("la_setmc: set multicast addresses\n"));
    }
    for(p = list; p != 0; p = p->em_next) {
	DPRINTF(2, ("la_setmc: add mc addr %x:%x:%x:%x:%x:%x\n",
			(* ((char *) p->em_addr + 0)) & 0xFF,
			(* ((char *) p->em_addr + 1)) & 0xFF,
			(* ((char *) p->em_addr + 2)) & 0xFF,
			(* ((char *) p->em_addr + 3)) & 0xFF,
			(* ((char *) p->em_addr + 4)) & 0xFF,
			(* ((char *) p->em_addr + 5)) & 0xFF));
	for(i = 0; i < 4; i++) mask[i] = 0;
	crc = EtherCRC((unsigned char *) p->em_addr, 6 );
	lance = crc >> 26;
	computemask(lance, mask);
	for(i=0; i < 4; i++) {
	    newmask[i] |= mask[i];
	}
    }
#endif
    ibp = lap->lad_ib;
    for(i=3; i >= 0; i--) {
	if(newmask[i] != ibp->ib_ladrf[i]) {
	    ibp->ib_ladrf[i] = newmask[i];
	    equal = 0;
	}
    }
    if(!equal) {
	if(lap->lad_tinuse == 0) {
	    DPRINTF(1, ("reboot now: %d\n", lap->lad_rinuse));
	    la_enqueue_reboot(lap);
	} else lap->lad_reinit = 1;
    }
#endif
}


#ifdef LANCE_DEBUG

/* Protection against overwrites of the Lance init block */
static IB_p shadow_ibp;

static void
print_ibp(ibp)
IB_p ibp;
{
    printf("ib_mode: 0x%x\n", ibp->ib_mode);
    printf("ib_paddr: %x:%x:%x:%x:%x:%x\n",
	   ibp->ib_padr[0] & 0xFF, ibp->ib_padr[1] & 0xFF,
	   ibp->ib_padr[2] & 0xFF, ibp->ib_padr[3] & 0xFF,
	   ibp->ib_padr[4] & 0xFF, ibp->ib_padr[5] & 0xFF);
    printf("ib_ladrf: [%x,%x,%x,%x]\n",
	   ibp->ib_ladrf[0] & 0xFFFF, ibp->ib_ladrf[1] & 0xFFFF,
	   ibp->ib_ladrf[2] & 0xFFFF, ibp->ib_ladrf[3] & 0xFFFF);

    printf("ib_rdralow: 0x%x\n", ibp->ib_rdralow & 0xFFFF);
    printf("ib_rlen: 0x%x\n", ibp->ib_rlen & 0xFF);
    printf("ib_rdrahigh: 0x%x\n", ibp->ib_rdrahigh & 0xFF);

    printf("ib_tdralow: 0x%x\n", ibp->ib_tdralow & 0xFFFF);
    printf("ib_tlen: 0x%x\n", ibp->ib_tlen & 0xFF);
    printf("ib_tdrahigh: 0x%x\n", ibp->ib_tdrahigh & 0xFF);
}

static void
check_ibp(lap)
register lad_p lap;
{
    /* Check the specified Lance init block for inconsistencies. */
    register IB_p test_ibp;

    test_ibp = shadow_ibp + lap->lad_hardifno;

    /* Only check the first 8 bytes, containing mode and etheraddr */
    if (memcmp(lap->lad_ib, test_ibp, 8) != 0) {
	printf("Inconsistent Lance Init block for ifno %d!\n",
	       lap->lad_hardifno);
	printf("Original init block:\n");
	print_ibp(test_ibp);
	printf("Current init block at 0x%lx:\n", lap->lad_ib);
	print_ibp(lap->lad_ib);
	panic("Lance init data overwritten");
    }
}

int
lance_reboot(begin, end)
char *begin;
char *end;
{
    /* allow explicit Lance reboot by means of a kstat */
    register lad_p lap;
    char *p;
   
    p = begin;
    for (lap = la_data; lap < la_data + nlance; lap++) {
	if (lap->lad_up) {
	    p = bprintf(p, end, "Rebooting Lance %d\n", lap->lad_hardifno);
	    output(lap->lad_device, LA_CSR, CSR_STOP);
	    la_reboot((long) lap);
	}
    }
    return p - begin;
}

#endif /* LANCE_DEBUG */

la_alloc(nif)
    int nif;
{
    la_data = (lad_p) aalloc((vir_bytes) nif*sizeof(lad_t), 0);
    nlance = nif;
    la_enddata = la_data + nlance;
#ifdef LANCE_DEBUG
    shadow_ibp = (IB_p) aalloc((vir_bytes) nif*sizeof(IB_t), 0);
#endif
}

la_init(hardifno, softifno, ifaddr, nrcvpkt, nsndpkt)
    int hardifno, softifno;
    char *ifaddr;
    int *nrcvpkt, *nsndpkt;
{
    register lad_p lap;
    register lai_p info;
    register lareg_p device;
    register IB_p ibp;
    register uint32 baddr;
    register pkt_p bufs;
    register char *bufdata;
    
    DPRINTF(2, ("la_init: initing lance interface %d\n", hardifno));
    assert(hardifno<nlance);
    info = la_info+hardifno;
    lap = la_data+hardifno;
    device = (lareg_p) info->lai_devaddr;
#if !defined(ISA) && !defined(MCA)
    if (!wprobe((vir_bytes) device, 2)) { /* 16 bit wide probe */
	DPRINTF(2, ("la_init: wprobe failed for lance interface %d\n",
			hardifno));
	return 0;
    }
#endif

    /*
     * Well, it seems to be there.
     */
    DPRINTF(2, ("la_init: lance interface %d seems to be there\n", hardifno));
    assert(ifaddr);
    assert(nrcvpkt);
    assert(nsndpkt);
    if ((*info->lai_getaddr)(info->lai_getarg, ifaddr)==0) {
	printf("la_init: can't get ethernet address of lance interface %d\n",
		hardifno);
	return 0;
    }
    DPRINTF(2, ("la_init: lance interface %d gave us its address\n", hardifno));

    /*
     * It even gave us its address, so start allocating resources
     */
    lap->lad_hardifno = hardifno;
    lap->lad_softifno = softifno;
    lap->lad_device = device;
    lap->lad_csr3 = info->lai_csr3;
    lap->lad_addrconv = info->lai_addrconv;
    lap->lad_tdontchain = info->lai_dont_chain;
    lap->lad_tchainhead = info->lai_min_chain_head_size;
    lap->lad_tchainmin = lap->lad_tchainhead + 64;
    
    /*
     * Allocate initblock
     */
    lap->lad_ib = (IB_p) (*info->lai_memalloc)(sizeof *lap->lad_ib, 8);
    
    /*
     * Allocate receive ring and parallel packet array
     */
    lap->lad_rsize = 1<<info->lai_logrcv;
    lap->lad_rmask = lap->lad_rsize-1;
    *nrcvpkt = lap->lad_rsize;
    lap->lad_rcvring = (rmde_p) (*info->lai_memalloc)(lap->lad_rsize*sizeof(rmde_t), 8);
    lap->lad_rpkts = (pkt_p *) aalloc((vir_bytes) lap->lad_rsize*sizeof(pkt_p), 0);
    
#ifndef EXTRA_RECEIVE_BUFFERS
    DPRINTF(2, ("la_init: allocate %d packets (%d)\n", lap->lad_rsize, 
	   RCVETHPKTSIZE + sizeof(pkt_t)));
    bufs = (pkt_p) aalloc((vir_bytes) lap->lad_rsize * sizeof(pkt_t), 0);
    bufdata = (*info->lai_memalloc)(lap->lad_rsize * RCVETHPKTSIZE, 0);
    pkt_init(&lap->lad_rpool, RCVETHPKTSIZE, bufs, lap->lad_rsize, bufdata,
	     la_bufavail, (long) lap);
#else
    /* Experiment: allocate twice as much packets as fit in the receive ring.
     * We need to resort to this because the receive ring already has the
     * maximum size of 128 in some configurations.
     */
    DPRINTF(2, ("la_init: allocate %d packets (%d)\n", 2 * lap->lad_rsize, 
	   RCVETHPKTSIZE + sizeof(pkt_t)));
    bufs = (pkt_p) aalloc((vir_bytes) 2 * lap->lad_rsize * sizeof(pkt_t), 0);
    bufdata = (*info->lai_memalloc)(2 * lap->lad_rsize * RCVETHPKTSIZE, 0);
    pkt_init(&lap->lad_rpool, RCVETHPKTSIZE, bufs, 2 * lap->lad_rsize, bufdata,
	     la_bufavail, (long) lap);
#endif
    
    lap->lad_tsize = 1<<info->lai_logtmt;
    lap->lad_tmask = lap->lad_tsize-1;
    lap->lad_latsize = info->lai_lalogtmt == 0 ? 0 : 1<<info->lai_lalogtmt;
    assert(lap->lad_tsize == lap->lad_latsize || lap->lad_latsize == 0);
    lap->lad_tmtring = (tmde_p) (*info->lai_memalloc)(lap->lad_tsize*sizeof(tmde_t), 8);
    lap->lad_tpkts = (pkt_p *) aalloc((vir_bytes) lap->lad_tsize*sizeof(pkt_p), 0);
    
    if(lap->lad_latsize > 0) {
	DPRINTF(2, ("la_init: allocate %d packets (%d)\n", lap->lad_latsize, 
	       sizeof(latpkt_t)));
	lap->lad_latpkts = (latpkt_p) (*info->lai_memalloc)(
				    lap->lad_latsize * sizeof(latpkt_t), 0);
    } else {
	assert(!lap->lad_tdontchain);
    }

    lap->lad_first = 0;
    lap->lad_last = &lap->lad_first;
    lap->lad_noverflow = 0;
    *nsndpkt = NOVERFLOW;
    DPRINTF(2, ("la_init: rsize %d tsize %d latsize %d\n", lap->lad_rsize,
	   lap->lad_tsize, lap->lad_latsize));
    
#if defined(ISA) || defined(MCA)
    /*
     * Set up the DMA chip and interrupt vector
     */
    DPRINTF(1, ("Lance %d: dma %d, irq %d\n", lap->lad_hardifno,
	info->lai_intrinfo.ii_dma, info->lai_intrinfo.ii_irq));
    dma_cascade(info->lai_intrinfo.ii_dma);
    lap->lad_intrarg = info->lai_intrinfo.ii_irq;
    setirq(info->lai_intrinfo.ii_irq, la_intr);
    pic_enable(info->lai_intrinfo.ii_irq);
#else
    /*
     * set the interrupt vector
     */
    lap->lad_intrarg = setup_interrupt(&info->lai_intrinfo, la_intr);
#endif /* defined(ISA) || defined(MCA) */
    
    /*
     * Fill in initblock
     */
    ibp = lap->lad_ib;
    ibp->ib_mode = 0;			/* plain sailing */
    
    /*
     * This convoluted initialization coupled with the csr3 stuff
     * below actually depend on byte order, board layout and such,
     * and should be configurable. Some day.
     */
#ifdef __LITTLE_ENDIAN_H__
    ibp->ib_padr[0] = ifaddr[0];
    ibp->ib_padr[1] = ifaddr[1];
    ibp->ib_padr[2] = ifaddr[2];
    ibp->ib_padr[3] = ifaddr[3];
    ibp->ib_padr[4] = ifaddr[4];
    ibp->ib_padr[5] = ifaddr[5];
#else
    ibp->ib_padr[0] = ifaddr[1];
    ibp->ib_padr[1] = ifaddr[0];
    ibp->ib_padr[2] = ifaddr[3];
    ibp->ib_padr[3] = ifaddr[2];
    ibp->ib_padr[4] = ifaddr[5];
    ibp->ib_padr[5] = ifaddr[4];
#endif
    
    /* Ignore multicast packets until the higher layers call la_setmc()
     * to accept specific ones.
     */
    ibp->ib_ladrf[0] = 0;
    ibp->ib_ladrf[1] = 0;
    ibp->ib_ladrf[2] = 0;
    ibp->ib_ladrf[3] = 0;
    
    baddr = (*lap->lad_addrconv) (lap->lad_rcvring,
	lap->lad_rsize * sizeof(rmde_t));
    compare(baddr, <, ADDRESSLIMIT);
    ibp->ib_rdralow = baddr;
    ibp->ib_rdrahigh = baddr>>16;
    ibp->ib_rlen = info->lai_logrcv<<5;
    
    baddr = (*lap->lad_addrconv) (lap->lad_tmtring,
	lap->lad_tsize * sizeof(tmde_t));
    compare(baddr, <, ADDRESSLIMIT);
    ibp->ib_tdralow = baddr;
    ibp->ib_tdrahigh = baddr>>16;
    ibp->ib_tlen = info->lai_logtmt<<5;

#ifdef LANCE_DEBUG
  {
    IB_p test_ibp;

    assert(shadow_ibp != 0);
    test_ibp = shadow_ibp + hardifno;
    memcpy((_VOIDSTAR) test_ibp, (_VOIDSTAR) ibp, sizeof(IB_t));
    printf("Lance control block at 0x%lx:\n", lap->lad_ib);
    print_ibp(test_ibp);
    check_ibp(lap);
  }
#endif
    
    lap->lad_reinit = 0;
    if (la_boot(lap)==0) {
	printf("la_init: couldn't boot lance chip at interface %d\n", hardifno);
	return 0;
    }
    lap->lad_up = 1;
    lap->lad_renq= 0;
    lap->lad_senq = 0;
    return 1;
}


la_stop(ifno)
    int ifno;
{
    register lad_p lap;
#if defined(ISA) || defined(MCA)
    register lai_p info;
#endif

    assert(ifno<nlance);
    lap = la_data+ifno;
    assert(lap->lad_up);
    DPRINTF(0, ("la_stop: stop lance\n"));
    output(lap->lad_device, LA_CSR, CSR_STOP);
#if defined(ISA) || defined(MCA)
    info = la_info+ifno;
    /* If the dma and lance irq are not initialised, don't stop them */
    if (lap->lad_intrarg != 0) {
	pic_disable(info->lai_intrinfo.ii_irq);
	dma_done(info->lai_intrinfo.ii_dma);
    }
#endif /* defined(ISA) || defined(MCA) */
}


static la_stuff_in_rcv_ring(lap, pkt)
    register lad_p lap;
    register pkt_p pkt;
{
    register uint32 baddr;
    register rmde_p rdp;
    
    rdp = lap->lad_rcvring+lap->lad_rtail;
    DPRINTF(3,("Fill rcv slot %d, flags %x\n", lap->lad_rtail, rdp->rmd_flags));
    assert(!(rdp->rmd_flags&RMD_OWN));
    lap->lad_rpkts[lap->lad_rtail] = pkt;
    BUMPMASK(lap->lad_rtail, lap->lad_rmask);
    lap->lad_rinuse++;
    compare(lap->lad_rinuse, <=, lap->lad_rsize);
    baddr = (*lap->lad_addrconv) (pkt_offset(pkt), pkt->p_contents.pc_dirsize);
    rdp->rmd_ladr = baddr;
    rdp->rmd_hadr = baddr>>16;
    rdp->rmd_flags = RMD_OWN;
    rdp->rmd_bcnt = -pkt->p_contents.pc_dirsize;
    rdp->rmd_mcnt = 0;
    /*
     * It belongs to the chip from now on
     */
}


static void la_overflow(lap, pkt)
    register lad_p lap;
    register pkt_p pkt;
{
    /* Too bad, ring is full. Pretend we sent it.
     * Maybe we should inform higher layers here.
     * Think about it.
     */    
    register unshort csr;
    register lareg_p device;

    device = lap->lad_device;
    csr=input(device, LA_CSR); 
    if ((csr&(CSR_TXON|CSR_RXON)) != (CSR_RXON|CSR_TXON)) {
	pkt_discard(pkt);
	printf("Lance stopped for unknown reason (la_overflow)\n");
	output(device,LA_CSR,CSR_STOP);
	la_reboot((long) lap);
	return;
    }
    if(lap->lad_noverflow > NOVERFLOW) {
	la_error(lap, "transmit ring full");
        STINC(ls_toflo);
	pkt_discard(pkt);
    } else {
        STINC(ls_tqtot);
	*lap->lad_last = pkt;
	lap->lad_last = &pkt->p_admin.pa_next;
	pkt->p_admin.pa_next = 0;
	lap->lad_noverflow++;
    }
}

/* To avoid unchained packets getting in front of the ones that do
 * need chaining (which may cause confusion at higher layers) we only
 * transmit a packet when at least two TMD entries are available or
 * when there is one available and the overflow queue is empty.
 */
#define can_send_packet(lap)	\
	(((lap)->lad_tinuse <= (lap)->lad_tsize - 2) ||	\
	 (((lap)->lad_tinuse != (lap)->lad_tsize) && ((lap)->lad_first == 0)))

/*
 * Work. Send away this packet.
 */
void la_send(ifno, pkt)
    register pkt_p pkt;
{
    register lad_p lap;
    register tmde_p tdp, tdp2;
    register uint32 baddr, baddr2;
    register latpkt_p hpkt;
    register lareg_p device;

    assert(ifno<nlance);
    lap = la_data+ifno;
    device = lap->lad_device;
    assert(lap->lad_up);

    /*
     * First handle Lance bogosity. The thing can chain, which is nice,
     * but it has its requirements. In a chain the first element must be
     * at least 100 bytes(MIN_CHAIN_HEAD_SIZE) and all other elements
     * at least 64 bytes. We only do chains of length 2, but still...
     *
     * Some interfaces require more than 100 bytes. Now a configurable
     * parameter.
     */
    if (pkt->p_contents.pc_totsize<=lap->lad_tchainmin) {
	/* 
	 * We can't chain anyhow, so copy extra data if there is some.
	 */
	if (pkt->p_contents.pc_totsize>pkt->p_contents.pc_dirsize) {
	    pkt_pullup(pkt, pkt->p_contents.pc_totsize);
	}
    } else {
	/*
	 * Chaining is possible, but first chainelement must still
	 * be of size MIN_CHAIN_HEAD_SIZE.
	 */
	if (pkt->p_contents.pc_dirsize<lap->lad_tchainhead) {
	    pkt_pullup(pkt, lap->lad_tchainhead);
	}
    }
    /*
     * End of Lance bogosity. Back to useful work.
     */

    /*
     * Transmit a packet. There are three possible cases: the packet
     * contains only direct data, or direct and virtual data, or its
     * the later case but the hardware cannot chain. In the first
     * case only one TMD is allocated, and in the second case two
     * TMD's are allocated and the packet is chained. In the third
     * case the packet is folded to an unchained one. In case the
     * transmit queue is full we place the packet on an overflow
     * queue.
     */
    if (pkt->p_contents.pc_totsize == pkt->p_contents.pc_dirsize) {
	/*
	 * Packet without chaining. Easy.
	 */
	if (!can_send_packet(lap)) {
	    /* just pretend we sent it */
	    la_overflow(lap, pkt);
	    return;
	}
	DPRINTF(2, ("sending %d bytes from slot %d",
	    pkt->p_contents.pc_totsize, lap->lad_ttail));

	/* set up transmit ring entry for direct data */
	lap->lad_tpkts[lap->lad_ttail] = pkt;
	baddr = (*lap->lad_addrconv) (pkt_offset(pkt),
	    pkt->p_contents.pc_totsize);
	if (baddr >= ADDRESSLIMIT) {
	    /* the Lance cannot access it directly, copy it */
	    assert(lap->lad_latsize > 0);
	    DPRINTF(2, (" [private buffer]"));
	    STINC(ls_memcpy);
	    hpkt = &(lap->lad_latpkts[lap->lad_ttail]);
	    (void) memmove((_VOIDSTAR) hpkt->l_data, (_VOIDSTAR)pkt_offset(pkt),
			   (size_t) pkt->p_contents.pc_totsize);
	    baddr = (*lap->lad_addrconv) (hpkt->l_data,
		pkt->p_contents.pc_totsize);
	}
	compare(baddr, <, ADDRESSLIMIT);
	tdp = lap->lad_tmtring + lap->lad_ttail;
	tdp->tmd_ladr = baddr;
	tdp->tmd_hadr = baddr >> 16;
	tdp->tmd_bcnt = -pkt->p_contents.pc_totsize;
	BUMPMASK(lap->lad_ttail, lap->lad_tmask);
	lap->lad_tinuse++;
	DPRINTF(2, ("\n"));

	/* and go ... */
	tdp->tmd_flags = TMD_OWN|TMD_STP|TMD_ENP;
    } else if (lap->lad_tdontchain && lap->lad_latsize > 0) {
	/*
	 * This packet is chained, but for some reason we cannot
	 * use the hardware chaining feature, but we have to copy
	 * it. So turn it into an unchained packet.
	 */
	if (!can_send_packet(lap)) {
	    /* just pretend we sent it */
	    la_overflow(lap, pkt);
	    return;
	}

	DPRINTF(2, ("sending %d bytes from slot %d\n",
	    pkt->p_contents.pc_totsize, lap->lad_ttail));

	lap->lad_tpkts[lap->lad_ttail] = pkt;
	hpkt = &(lap->lad_latpkts[lap->lad_ttail]);
	STINC(ls_memcpy);
	(void) memmove((_VOIDSTAR) hpkt->l_data, (_VOIDSTAR) pkt_offset(pkt),
		       (size_t) pkt->p_contents.pc_dirsize);
	if (pkt->p_contents.pc_totsize > pkt->p_contents.pc_dirsize) {
	    (void) memmove(
			(_VOIDSTAR) (hpkt->l_data + pkt->p_contents.pc_dirsize),
			(_VOIDSTAR) pkt->p_contents.pc_virtual,
			(size_t) (pkt->p_contents.pc_totsize -
				  pkt->p_contents.pc_dirsize));
	}
	baddr = (*lap->lad_addrconv) (hpkt->l_data, pkt->p_contents.pc_totsize);
	compare(baddr, <, ADDRESSLIMIT);

	tdp = lap->lad_tmtring + lap->lad_ttail;
	tdp->tmd_ladr = baddr;
	tdp->tmd_hadr = baddr >> 16;
	tdp->tmd_bcnt = -pkt->p_contents.pc_totsize;
	BUMPMASK(lap->lad_ttail, lap->lad_tmask);
	lap->lad_tinuse++;

	/* and go ... */
	tdp->tmd_flags = TMD_OWN|TMD_STP|TMD_ENP;
    } else {
	/*
	 * Chained packet. A bit more hassle.
	 */
	if (lap->lad_tinuse > lap->lad_tsize-2) {
	    /* just pretend we sent it */
	    la_overflow(lap, pkt);
	    return;
	}

	DPRINTF(2, ("chained sending %d bytes from slot %d",
	    pkt->p_contents.pc_dirsize, lap->lad_ttail));

	/* set up transmit ring entry for direct data */
	baddr = (*lap->lad_addrconv) (pkt_offset(pkt),
	    pkt->p_contents.pc_dirsize);
	if (baddr >= ADDRESSLIMIT) {
	    /* the Lance cannot access it directly, copy it */
	    assert(lap->lad_latsize > 0);
	    DPRINTF(2, (" [private buffer]"));
	    STINC(ls_memcpy);
	    hpkt = &(lap->lad_latpkts[lap->lad_ttail]);
	    (void) memmove((_VOIDSTAR) hpkt->l_data,
			   (_VOIDSTAR) pkt_offset(pkt),
			   (size_t) pkt->p_contents.pc_dirsize);
	    baddr = (*lap->lad_addrconv) (hpkt->l_data,
		pkt->p_contents.pc_dirsize);
	}
	compare(baddr, <, ADDRESSLIMIT);
	tdp = lap->lad_tmtring + lap->lad_ttail;
	tdp->tmd_ladr = baddr;
	tdp->tmd_hadr = baddr >> 16;
	tdp->tmd_bcnt = -pkt->p_contents.pc_dirsize;
	assert(pkt->p_contents.pc_dstype == 0); /* for the moment */
	BUMPMASK(lap->lad_ttail, lap->lad_tmask);
	lap->lad_tinuse++;

	/*
	 * Put packet pointer in tail slot. Do not discard until
	 * packet is really and fully gone
	 */
	lap->lad_tpkts[lap->lad_ttail] = pkt;

	DPRINTF(2, (" and %d bytes from slot %d",
	    pkt->p_contents.pc_totsize-pkt->p_contents.pc_dirsize,
	    lap->lad_ttail));

	/* set up transmit ring entry for virtual data */
	baddr2 = (*lap->lad_addrconv) ((char *) pkt->p_contents.pc_virtual,
	    pkt->p_contents.pc_totsize - pkt->p_contents.pc_dirsize);
	if (baddr2 >= ADDRESSLIMIT) {
	    /* the Lance cannot access it directly, copy it */
	    assert(lap->lad_latsize > 0);
	    DPRINTF(2, (" [private buffer]"));
	    hpkt = &(lap->lad_latpkts[lap->lad_ttail]);
	    STINC(ls_memcpy);
	    (void) memmove((_VOIDSTAR) hpkt->l_data,
			   (_VOIDSTAR) pkt->p_contents.pc_virtual,
			   (size_t) (pkt->p_contents.pc_totsize -
				     pkt->p_contents.pc_dirsize));
	    baddr2 = (*lap->lad_addrconv) (hpkt->l_data,
		pkt->p_contents.pc_totsize - pkt->p_contents.pc_dirsize);
	}
	compare(baddr2, <, ADDRESSLIMIT);
	tdp2 = lap->lad_tmtring + lap->lad_ttail;
	tdp2->tmd_ladr = baddr2;
	tdp2->tmd_hadr = baddr2 >> 16;
	tdp2->tmd_bcnt =
	    -(pkt->p_contents.pc_totsize - pkt->p_contents.pc_dirsize);
	BUMPMASK(lap->lad_ttail, lap->lad_tmask);
	lap->lad_tinuse++;

	DPRINTF(2, ("\n"));

	/* and go ... */
	tdp2->tmd_flags = TMD_OWN|TMD_ENP;	/* DON'T SWITCH */
	tdp->tmd_flags = TMD_OWN|TMD_STP;	/* THESE TWO */
    }
    output(device, LA_CSR, CSR_TDMD|CSR_INEA);
    END_MEASURE(ether_snd);
}

#ifdef ACTMSG_INTERRUPT
void
la_polled_send(ifno, data, size)
    int ifno;
    void *data;
    int size;
{
    register lad_p lap;
    register tmde_p tdp, tdp2;
    register uint32 baddr, baddr2;
    register latpkt_p hpkt;
    register lareg_p device;

    assert(ifno<nlance);
    lap = la_data+ifno;
    device = lap->lad_device;
    assert(lap->lad_up);

    if (lap->lad_tinuse == lap->lad_tsize)
	return;
    /* set up transmit ring entry for direct data */
    baddr = (*lap->lad_addrconv) (data, size);
    if (baddr >= ADDRESSLIMIT) {
	/* the Lance cannot access it directly, copy it */
	assert(lap->lad_latsize > 0);
	hpkt = &(lap->lad_latpkts[lap->lad_ttail]);
	STINC(ls_memcpy);
	(void) memmove((_VOIDSTAR) hpkt->l_data, data, size);
	baddr = (*lap->lad_addrconv) (hpkt->l_data, size);
    }
    compare(baddr, <, ADDRESSLIMIT);
    tdp = lap->lad_tmtring + lap->lad_ttail;
    tdp->tmd_ladr = baddr;
    tdp->tmd_hadr = baddr >> 16;
    tdp->tmd_bcnt = -size;
    BUMPMASK(lap->lad_ttail, lap->lad_tmask);
    lap->lad_tinuse++;
    output(device, LA_CSR, CSR_TDMD|CSR_INEA);
}
#endif /* ACTMSG_INTERRUPT */

#ifndef SMALL_KERNEL

#ifdef STATISTICS

#ifdef LADUMP

int la_dump(begin, end, lap)
    char *begin;
    char *end;
    register lad_p lap;
{
    register rmde_p rdp;
    register tmde_p tdp;
    char * p;
    
    assert(lap->lad_up);
    p = bprintf(begin, end, "la_csr=%x\n", input(lap->lad_device, LA_CSR));
    p = bprintf(p, end, "input: head=%d, tail=%d, inuse=%d\n",lap->lad_rhead,
					   lap->lad_rtail, lap->lad_rinuse);
    for(rdp=lap->lad_rcvring; rdp<lap->lad_rcvring+lap->lad_rsize; rdp++) {
	p = bprintf(p, end, "%2d: %x %x %d %d %x\n",
		rdp-lap->lad_rcvring, rdp->rmd_hadr, rdp->rmd_ladr,
		rdp->rmd_bcnt, rdp->rmd_mcnt, rdp->rmd_flags);
    }
    p = bprintf(p, end, "output: head=%d, tail=%d, inuse=%d\n",lap->lad_thead,
				       lap->lad_ttail, lap->lad_tinuse);
    for(tdp=lap->lad_tmtring; tdp<lap->lad_tmtring+lap->lad_tsize; tdp++) {
	p = bprintf(p, end, "%2d: %x %x %d %x %x\n",
			tdp-lap->lad_tmtring, tdp->tmd_hadr, tdp->tmd_ladr,
			tdp->tmd_bcnt, tdp->tmd_flags, tdp->tmd_err);
    }
    return p - begin;
}

#endif

int lastatistics(begin, end)
    char *begin, *end;
{
    register lad_p lap;
    char *	p;
    
    p = begin;
    for(lap = la_data; lap < la_data + nlance; lap++) {
    if(lap->lad_up) {
    p = bprintf(p, end, "====== Lance statistics %d(%d) ==============\n",
	lap->lad_hardifno, lap->lad_softifno);
    p = bprintf(p, end, "babl    %7ld ", lap->lad_stat.ls_babl);
    p = bprintf(p, end, "cerr    %7ld ", lap->lad_stat.ls_cerr);
    p = bprintf(p, end, "miss    %7ld ", lap->lad_stat.ls_miss);
    p = bprintf(p, end, "dropped %7ld ", lap->lad_stat.ls_dropped);
    p = bprintf(p, end, "read    %7ld\n",lap->lad_stat.ls_read);
    p = bprintf(p, end, "fram    %7ld ", lap->lad_stat.ls_fram);
    p = bprintf(p, end, "buff    %7ld ", lap->lad_stat.ls_buff);
    p = bprintf(p, end, "crc     %7ld ", lap->lad_stat.ls_crc);
    p = bprintf(p, end, "oflo    %7ld ", lap->lad_stat.ls_oflo);
    p = bprintf(p, end, "coll    %7ld\n",lap->lad_stat.ls_coll);
    p = bprintf(p, end, "written %7ld ", lap->lad_stat.ls_written);
    p = bprintf(p, end, "oerrors %7ld ", lap->lad_stat.ls_oerrors);
    p = bprintf(p, end, "one     %7ld ", lap->lad_stat.ls_one);
    p = bprintf(p, end, "more    %7ld ", lap->lad_stat.ls_more);
    p = bprintf(p, end, "def     %7ld\n",lap->lad_stat.ls_def);
    p = bprintf(p, end, "uflo    %7ld ", lap->lad_stat.ls_uflo);
    p = bprintf(p, end, "lcar    %7ld ", lap->lad_stat.ls_lcar);
    p = bprintf(p, end, "lcol    %7ld ", lap->lad_stat.ls_lcol);
    p = bprintf(p, end, "rtry    %7ld ", lap->lad_stat.ls_rtry);
    p = bprintf(p, end, "reboot  %7ld\n",lap->lad_stat.ls_reboot);
    p = bprintf(p, end, "tqoflo  %7ld ", lap->lad_stat.ls_toflo);
    p = bprintf(p, end, "tqtot   %7ld ", lap->lad_stat.ls_tqtot);
    p = bprintf(p, end, "tqnow   %7ld ", lap->lad_noverflow);
    p = bprintf(p, end, "memcpy  %7ld\n",lap->lad_stat.ls_memcpy);
    }
    }
    return p - begin;
}

/* called by typing something on the console or amstat */
netdump(begin, end)
    char *	begin;	/* start of buffer in which to dump output */
    char *	end;
{
    int sum = 0;
    
#ifdef LADUMP
    register lad_p lap;

    for (lap = la_data; lap < la_data + nlance; lap++) {
	sum += la_dump(begin + sum, end, lap);
    }
#endif
    sum += lastatistics(begin + sum, end);
    return sum;
}

#endif /* STATISTICS */

#endif /* SMALL_KERNEL */
