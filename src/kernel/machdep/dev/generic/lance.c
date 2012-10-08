/*	@(#)lance.c	1.5	94/08/19 12:50:40 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#define inline

#ifdef STATISTICS
#define LASTAT	/* keep statistics */
#endif

#include <string.h>
#include "amoeba.h"
#include "internet.h"
#include "assert.h"
INIT_ASSERT
#include "byteorder.h"
#include "etherformat.h"
#include "lance.h"
#include "global.h"
#include "machdep.h"
#include "map.h"
#include "interrupt.h"
#include "lainfo.h"
#include "lastat.h"
#include "randseed.h"
#include "sys/proto.h"

/*DBG*/long lance_bug;

#define FAKESITENO	0xFF		/* trans.c wants to have a number */

static port broadcastaddr = { { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF } };
static port gwaddr;
static port myaddr;

#ifdef VAX2000
/*
** Have to map the lance in on that machine...
*/
vir_bytes LANCEBASE;
#else
#define LANCEBASE lainfo.lai_devaddr
#endif VAX2000

/*
** Be careful with LOGNTRANS: Making it too big will result in a long
** delay before a hanging lance chip is detected, making it too small
** will cause packets to be dropped (and possibly the
** lance being rebooted) unnecessarily. Use small values for buggy
** lances, big values for good lances.
*/
#ifndef LOGNTRANS
#ifndef ROMKERNEL
#define LOGNTRANS 	4	/* 16 small transmit buffers */
#else
#define LOGNTRANS	2
#endif
#endif

#ifndef LOGNRECV
#ifndef ROMKERNEL
#define LOGNRECV	5	/* 32 full size receive buffers */
#else
#define LOGNRECV	2
#endif
#endif

#define LACHAIN

#ifdef LACHAIN
/*
 * User data will not be copied on output, except if the total
 * packet is less than 164 bytes.
 * Even if user data is chained the headers are padded to 100
 * bytes with user data.
 * Both of these magic numbers come from Lance requirements,
 * the first buffer in a chain must be >= 100 bytes, all other buffers
 * must be >= 64 bytes.
 * If the bus is larger than 24 address bits, the Lance can't cope
 * and we have to copy anyhow. Isn't nature wonderful?
 */
#define THEADMIN	100	/* Minimum for Lance chip */
#define TCHAINMIN	164	/* Transfers larger than this will be chained */
#ifndef BUS2BIG
#define TBUFSIZ		TCHAINMIN
#else
#define TBUFSIZ		1514
#endif

static int	curchain;	/* current output packet will be chained */

#else LACHAIN

#define TBUFSIZ		1514

#endif LACHAIN

#ifdef LADEBUG
#define DEB(x) x
#else
#define DEB(x) /* empty */
#endif

#define BUMP(n,size)	if (++n==size) n=0;else

#ifdef LASTAT
struct lastat lastat;
#endif

#include "memaccess.h"
#ifdef IOMAPPED		/* i/o mapped */
#define input(p,f)		(inword((uint16) (long) &(p)->f))
#define output(p,f,v)		outword((uint16) (long) &(p)->f, (v))
#else			/* memory mapped */
#define input(p,f)		mem_get_short((short *) &(p)->f)
#define output(p,f,v)		mem_put_short((short *) &(p)->f, v)
#endif


struct framehdr {
	char	f_dstaddr[6];	/* 48-bit Ether-addr */
	char	f_srcaddr[6];	/* 48-bit Ether-addr */
	uint16	f_proto;	/* protocol field */

	struct pktheader f_ah;	/* Amoeba stuff */
};

struct packet {
	struct framehdr p_fhdr;
	char		p_data[PACKETSIZE];
	long		p_crc;
};

static unsigned outsiz;
static char *inptr, *outptr;

#define NTRANSMRING	(1<<LOGNTRANS)
#define NRECVRING	(1<<LOGNRECV)

typedef union {
	struct {
		struct framehdr opk_fhdr;
		char opk_data[TBUFSIZ-sizeof(struct framehdr)];
	} p_opk;
	char p_chr[TBUFSIZ];
} tbuf_t;

static
volatile struct etherdata {		/* aligned on 8-byte boundary */
	struct InitBlock ed_ib;
	struct rmde	ed_rmde[NRECVRING];
	struct tmde	ed_tmde[NTRANSMRING];
	struct packet	ed_rbuf[NRECVRING];
	tbuf_t		ed_tbuf[NTRANSMRING];
} *ed;

volatile static tbuf_t *tbaddr[NTRANSMRING];
static uint16 tbuslow[NTRANSMRING];
static char tbushigh[NTRANSMRING];
#ifdef EHOOK
extern int eh_active;
static mutex *tbmutex[NTRANSMRING];
#endif

static uint16	rnext;	/* Next ringentry to look at in laread() */
static uint16	rinext;	/* Next ringentry to look at in larecv() */
static uint16	renq;	/* Larecv enqueued */
static uint16	tnext;	/* Next ringentry to fill */
static uint16	tfull;	/* Next ringentry to inspect for completion */
static uint16	tinuse;	/* Number of entries in use */
static long	bootseqno;	/* Boot sequence number */
static long	boot2seqno;	/* Boot sequence number */

#define eacopy(f,t) (* (port *) (t) = * (port *) (f))	/* 48-bit hack */

#ifdef LADEBUG
int iambusy;		/* To check puthead consistency */
#endif

/* Forward static declarations to avoid warnings from standard C compilers: */
static laerror();
static larecv();
static latrans();
static latrerror();
static check();

/*
 * Mapping routine from 48-bit Ethernet addresses to 7-bit pseudo PRONET 
 * addresses. The trick is that a node does not know its own number
 * but the destination fills it in for him.
 */

#define MAPENTRIES	127	/* Cannot use 0xFF */

char eamap[MAPENTRIES+1][8];	/* 8 instead of 6 for speed */


#define eacmp(a, b)	 PORTCMP((port *) (a), (port *) (b))
#define eanull(a)	 NULLPORT((port *) (a))


inline
static
ealookup(eaddr)
register char *eaddr;
{
	register index;
	register i;
	register char *mep;

	index = eaddr[5]&0x7F;	/* hash */
	if (index >=MAPENTRIES)
		index = 0;
	i = index;
	do {
		mep = eamap[i];
		if (eacmp(eaddr,mep))
			return i|ETHERBITS;
		if (eanull(mep)) {
			eacopy(eaddr,mep);
			return i|ETHERBITS;
		}
		i++;
		if (i>=MAPENTRIES)
			i = 0;
	} while (i != index);
	return 0;
}

inline
static check(fhp)
register struct framehdr *fhp;
{

  dec_s_be(&fhp->f_proto);
  if (fhp->f_proto != AMOEBAPROTO
#ifdef ALTAMOEBAPROTO
	&& fhp->f_proto != ALTAMOEBAPROTO
#endif
					) {
	enc_s_be(&fhp->f_proto);
	return 0;
  }
  if (fhp->f_ah.ph_srcnode == 0) {	/* from normal Ethernet node */
	if ((fhp->f_ah.ph_srcnode = ealookup(fhp->f_srcaddr))==0) {
		printf("Ethernet mapping table overflow\n");
		return 0;
	}
  } else {
	/* packet from gateway */
	if (eanull(&gwaddr)) {
		if (eanull(fhp->f_srcaddr))
			return 0;	/* Illegal Ethernet address */
		eacopy(fhp->f_srcaddr,&gwaddr);
		DEB(printf("Gateway to PRONET at "));
		DEB(praddr((char *) &gwaddr));
		DEB(printf("\n"));
	}
#ifdef LADEBUG
	else if (!eacmp(&gwaddr,fhp->f_srcaddr)) {
		printf("Second gateway claims to be at ");
		praddr(fhp->f_srcaddr);
		printf("\n");
	}
#endif LADEBUG
  }
  return 1;
}

/*ARGSUSED*/
inline
static
ladrop(rend)
long rend;
{

	compare(rend, ==, rnext);
	ed->ed_rmde[rnext].rmd_flags = RMD_OWN;
	BUMP(rnext, NRECVRING);
}

inline
netenable() {
}

static
lainit(etheraddr)
char *etheraddr;
{
	register volatile struct lance *device;
	register i;
	volatile register struct rmde *rp;
	volatile register struct packet *rbp;
	register volatile struct etherdata *E;
	register long baddr;
	static uint16 zero;/* Used to avoid read/modify/write cycles on CSR */

	device = LANCEADDR;
	E = ed;
	/*
	 * Init receive ring
	 */
	for (i=0,rp=E->ed_rmde,rbp=E->ed_rbuf;i<NRECVRING;i++,rp++,rbp++) {

		baddr = BUSADDR((vir_bytes) rbp);
		rp->rmd_ladr = baddr;
		rp->rmd_hadr = baddr>>16;
		rp->rmd_bcnt= -sizeof(struct packet);
		rp->rmd_flags= RMD_OWN;
		/*
		 * From this point on we dare not touch this entry
		 * It is OWNed by the Lance.
		 */
	}
	rnext = 0;
	rinext= 0;
	renq  = 0;

	/*
	 * Init transmit ring
	 */
	for (i=0;i<NTRANSMRING;i++) {
		E->ed_tmde[i].tmd_flags= 0;
		tbaddr[i] = &E->ed_tbuf[i];
		baddr = BUSADDR((vir_bytes) &E->ed_tbuf[i]);
		tbuslow[i] = baddr;
		tbushigh[i] = baddr>>16;
	}
	tnext = 0;
	tfull = 0;
	tinuse= 0;

	/*
	 * Fill InitBlock
	 */

	E->ed_ib.ib_padr[0] = etheraddr[PADR_BYTE0];
	E->ed_ib.ib_padr[1] = etheraddr[PADR_BYTE1];
	E->ed_ib.ib_padr[2] = etheraddr[PADR_BYTE2];
	E->ed_ib.ib_padr[3] = etheraddr[PADR_BYTE3];
	E->ed_ib.ib_padr[4] = etheraddr[PADR_BYTE4];
	E->ed_ib.ib_padr[5] = etheraddr[PADR_BYTE5];

	baddr = BUSADDR((vir_bytes) E->ed_rmde);
	E->ed_ib.ib_rdralow = baddr;
	E->ed_ib.ib_rdrahigh = baddr>>16;
	E->ed_ib.ib_rlen = LOGNRECV<<5;

	baddr = BUSADDR((vir_bytes) E->ed_tmde);
	E->ed_ib.ib_tdralow = baddr;
	E->ed_ib.ib_tdrahigh = baddr>>16;
	E->ed_ib.ib_tlen = LOGNTRANS<<5;

	/*
	 * Initialize Lance chip
	 */

	output(device,la_csr,CSR_STOP);	/* can't be too sure */
	output(device,la_rap,RDP_CSR1);
	baddr = BUSADDR((vir_bytes) &E->ed_ib);
	output(device,la_rdp.csr1, baddr);
	output(device,la_rap,RDP_CSR2);
	output(device,la_rdp.csr2, baddr>>16);
	output(device,la_rap,RDP_CSR3);
	/* Some machines don't like csr3 initialization */
	if (lainfo.lai_csr3 != -1)
		output(device,la_rdp.csr3, lainfo.lai_csr3);
	output(device,la_rap,RDP_CSR0);	/* and stays there */
	output(device,la_csr,CSR_INIT|CSR_STRT);
	while ((input(device,la_csr)&(CSR_IDON|CSR_ERR))==0)
		if (++i > 10000) {
			printf("Hanging in Lance initialization");
		}
	;
	if ((input(device,la_csr)&(CSR_ERR|CSR_TXON|CSR_RXON)) !=
							(CSR_TXON|CSR_RXON))
		panic("Lance initialization error\n");
	output(device,la_csr,CSR_IDON|CSR_INEA);	/* clear IDON bit ! */
}

static
la2reboot(seqno)
long seqno;
{

	/* clean up buffers if necessary */
	if (seqno != boot2seqno) {
	    printf("la2reboot: out-of-sequence reboot(%ld, %ld)\n",
							seqno, boot2seqno);
	    return;
	}
	lainit((char *) &myaddr);
	boot2seqno++;
	DEB(printf("ok\n"));
	STINC(ls_reboots);
}

net_stop() {

	output(LANCEADDR,la_csr,CSR_STOP);
}

static
lareboot(seqno)
long seqno;
{

	/*
	** Note: lareboot *must* be called at lo-priority. We
	** set priority high (so we can do enqueue) and then back
	** to low.
	** Also, it is possible that multiple calls to lareboot
	** are enqueued before the first is handled. So, we keep
	** a bootsequencenumber, and return here if our sequencenumber
	** has already passed (because another incarnation of lareboot
	** has been here already)
	*/
	disable();
	if (seqno != bootseqno) {
	    DEB(printf("Spurious reboot(%ld) skipped\n", seqno));
	    enable();
	    return;
	}
	bootseqno++;
	DEB(printf("Rebooting Lance(%ld)... ", seqno));
	net_stop();
	DEB(printf("stopped... "));
	enqueue(la2reboot, seqno);
	enable();
}

#ifdef EHOOK
get_eadr(adr)
char adr[6];
{
    (void) memmove((_VOIDSTAR) adr, (_VOIDSTAR) &myaddr, 6);
}

static int lasend();

ethersend(addr, len, mu)
phys_bytes addr;
phys_bytes len;
mutex *mu;
{
    int head = (len < THEADMIN) ? len : THEADMIN;
    char *out;
    
    out = tbaddr[tnext]->p_chr;
    tbmutex[tnext] = mu;
#ifdef LACHAIN
    phys_copy(addr, VTOP(out), head);
    lasend(head, addr+head, len - head);
#else
    phys_copy(addr, VTOP(out), len);
    lasend(len, addr, 0);
#endif
}
#endif


static lasend(length, data, size)
uint16 length;
phys_bytes data;
uint16 size;
{
	register struct tmde *rp= (struct tmde *)&ed->ed_tmde[tnext];
	
	if (length<60) length=60;
	rp->tmd_ladr = tbuslow[tnext];
	rp->tmd_hadr = tbushigh[tnext];
	rp->tmd_bcnt = -length;
#ifdef LACHAIN
	if (size) {	/* chaining */
		register struct tmde *rp2;
		register long baddr;

		baddr = BUSADDR(PTOV(data));
#ifdef vax
		
		/*DBG*/if( baddr < 0 || baddr > 0x1000000 ) { printf("data %x %x\n", data, baddr); assert(0); }
#endif
#ifdef BUS2BIG
		if (baddr>=0x1000000) {
			/*
			 * Lance can't get here, so we have to copy
			 */
			phys_copy(data,
			    (phys_bytes) (ed->ed_tbuf[tnext].p_chr+length),
			    size);
			rp->tmd_bcnt = -(length+size);
			rp->tmd_flags = TMD_OWN|TMD_STP|TMD_ENP;
		} else
#endif
		{
			compare(tinuse, <, NTRANSMRING);
			BUMP(tnext, NTRANSMRING);
			tinuse++;
			rp2= (struct tmde *)&ed->ed_tmde[tnext];
			rp2->tmd_ladr = baddr;
			rp2->tmd_hadr = baddr>>16;
			rp2->tmd_bcnt = -size;
			rp2->tmd_flags = TMD_OWN|TMD_ENP;  /* order of these  */
			rp->tmd_flags = TMD_OWN|TMD_STP;   /* lines important */
		}
	} else
#endif
		rp->tmd_flags = TMD_OWN|TMD_STP|TMD_ENP;
	output(LANCEADDR,la_csr,CSR_TDMD|CSR_INEA);
	BUMP(tnext, NTRANSMRING);
	tinuse++;
	STINC(ls_written);
#ifdef LADEBUG
	iambusy = 0;
#endif
}

/*ARGSUSED*/
static
laread(rend)
long rend;
{
	register uint16 total;
	register struct rmde *rp;
	register struct packet *packet;

	STINC(ls_read);
	compare(rend, ==, rnext);
	rp = (struct rmde *)&ed->ed_rmde[rnext];
	packet = (struct packet *)&ed->ed_rbuf[rnext];
	dec_s_le(&packet->p_fhdr.f_ah.ph_size);
	total = packet->p_fhdr.f_ah.ph_size;
	if (rp->rmd_mcnt>=total && check(&packet->p_fhdr)) {
		packet->p_fhdr.f_ah.ph_dstnode = FAKESITENO;
		packet->p_fhdr.f_ah.ph_size = total-sizeof(struct framehdr);
		inptr = &packet->p_data[HEADERSIZE];
		if (!pkthandle(&packet->p_fhdr.f_ah, packet->p_data))
			netenable();
	} else
#ifdef EHOOK
	{
		if (eh_active) {
		    enc_s_le(&packet->p_fhdr.f_ah.ph_size);
		    /* subtract 4 to get rid of CRC bytes */
		    ethergot((char *)packet, rp->rmd_mcnt - 4);
		}
		netenable();
	}
#else
		netenable();
#endif EHOOK
	rp->rmd_flags = RMD_OWN;
	BUMP(rnext, NRECVRING);
}

inline
static
latrans() {
	uint16 fl;

	while (tinuse) {
#ifdef EHOOK
		if (tbmutex[tfull]) {
		    extern void mu_unlock();

		    enqueue(mu_unlock, (long)tbmutex[tfull]);
		    tbmutex[tfull] = 0;
		}
#endif EHOOK
		fl= ed->ed_tmde[tfull].tmd_flags;

		if (fl & TMD_OWN)
			break;
		if (fl & TMD_ERR)
			latrerror(ed->ed_tmde[tfull].tmd_err);
		if (fl & TMD_ENP) {
			if (fl & TMD_ONE) {
				STINC(ls_one);
				STINC(ls_coll);
			}
			if (fl & TMD_MORE) {
				STINC(ls_more);
				STADD(ls_coll,2);	/* Guestimate */
			}
			if (fl & TMD_DEF)
				STINC(ls_def);
		}
		BUMP(tfull, NTRANSMRING);
		tinuse--;
	}
}

static
larcverror(flag)
unsigned flag;
{

	/* On Lance revision C some bits are not valid al the time
	   watch out for that problem */

	if (flag&RMD_CRC) {
	    /*
	    ** Stop all those horrible messages.
	    */
	    STINC(ls_crc);
	    if (flag&RMD_FRAM)
		    STINC(ls_fram);
	} else {
	    DEB(printf("Lance rcv error:"));
	    if (flag&RMD_FRAM) {
		    DEB(printf(" FRAM"));
		    STINC(ls_fram);
	    }
	    if (flag&RMD_BUFF) {
		    DEB(printf(" BUFF"));
		    STINC(ls_buff);
	    }
	    if (flag&RMD_CRC) {
		    DEB(printf(" CRC"));
		    STINC(ls_crc);
	    }
	    if (flag&RMD_OFLO) {
		    DEB(printf(" OFLO"));
		    STINC(ls_oflo);
	    }
	    if (! (flag & (RMD_FRAM|RMD_CRC|RMD_BUFF|RMD_OFLO)))
		DEB(printf("%x", flag));
	    DEB(printf("\n"));
	}
	STINC(ls_dropped);
}

static
larecv() {
	register struct rmde *rp;
	register long par;

	/*
	** Clear renq, we're going to handle the received packets.
	** If we do this at the last packet race condition may occur.
	**
	*/
	renq = 0;

	/*DBG*/lance_bug = 0;
	while (1) {
		rp= (struct rmde *)&ed->ed_rmde[rinext];
		if (rp->rmd_flags&RMD_OWN)
			break;			/* Belongs to Lance */
		par = (long) rinext;	/* from laread reboot could be called */
		BUMP(rinext, NRECVRING);
		if (rp->rmd_flags == (RMD_STP|RMD_ENP)) {
			laread(par);
		} else {
			if (rp->rmd_flags & RMD_ERR)
			    larcverror(rp->rmd_flags);
			else {
			    printf("lance rcv: %x %x next %x %x\n", rp->rmd_flags,
			     rp->rmd_mcnt, ed->ed_rmde[rinext].rmd_flags,
			     ed->ed_rmde[rinext].rmd_mcnt);
			}
			ladrop(par);
		}
	}
}

netintr() {
	register volatile struct lance *device;
	register uint16 csr;

	device = LANCEADDR;
	for(csr=input(device,la_csr); csr&CSR_INTR; csr=input(device,la_csr)) {
		/* next if is for speedup in nonerror case */
		if ((csr & (CSR_ERR|CSR_TXON|CSR_RXON)) != (CSR_TXON|CSR_RXON)) {
			if (csr&CSR_ERR)
				laerror(csr);
			if ((csr & (CSR_TXON|CSR_RXON)) != (CSR_TXON|CSR_RXON)) {
				DEB(printf("Lance stopped!!\n"));
				STINC(ls_stopped);
				enqueue(lareboot, bootseqno);
				return;
			}
		}
		if (csr&CSR_TINT) {
			output(device,la_csr,CSR_TINT);	/* clearing INEA */
			latrans();
		}
		if (csr&CSR_RINT) {
			output(device,la_csr,CSR_RINT);	/* clearing INEA */

			/*
			** Check if there is already an enqueued larecv,
			** if so, don't enqueue larecv a second time.
			** Actually sater thinks this is unnecessary XXX
			**
			*/
			if (renq == 0) {
				renq = 1;
				enqueue(larecv, 0L);
			}
		}
	}
	output(device,la_csr,CSR_INEA);
}

static
laerror(csr)
register uint16 csr;
{
	register volatile struct lance *device;

	device = LANCEADDR;
	if (csr&CSR_BABL) {
		output(device,la_csr,CSR_BABL|CSR_INEA);	/* clear bit */
		DEB(printf("Lance chip: babbling error\n"));
		STINC(ls_babl);
	}
	if (csr&CSR_CERR) {
		output(device,la_csr,CSR_CERR|CSR_INEA);	/* clear bit */
/* 		DEB(printf("Lance chip: collission error\n")); */
		STINC(ls_cerr);
	}
	if (csr&CSR_MISS) {
		output(device,la_csr,CSR_MISS|CSR_INEA);	/* clear bit */
		/*
		** If there is an enqueued larecv, the missed packet 
		** occured because larecv wasn't called in time; ignore
		** missed packet. When there is no enqueued larecv it
		** is a "normal" missed packet.
		**
		*/
		if (renq == 0) {
			DEB(printf("Lance chip: missed packet\n"));
			/*DBG*/if (lance_bug++ > 10) {
				panic("Lance bug detected");
			}
		}
		STINC(ls_miss);
	}
	if (csr&CSR_MERR) {	/* pretty serious, should perhaps be panic */
		printf("Lance stopped because of memory error\n");
		/* reboot will come from netintr() */
	}
}


static
latrerror(err)
uint16 err;
{
	STINC(ls_oerrors);
	DEB(printf("Lance xmt error:"));
	if ( err & TMDE_LCOL) {
		DEB(printf(" LCOL"));
		STINC(ls_lcol);
	}
	if (err & TMDE_LCAR) {
		DEB(printf(" LCAR"));
		STINC(ls_lcar);
	}
	if (err & TMDE_UFLO) {
		DEB(printf(" UFLO"));
		STINC(ls_uflo);
	}
	if (err & TMDE_RTRY) {
		DEB(printf(" RTRY"));
		STINC(ls_rtry);
		STADD(ls_coll,16);
	}
	DEB(printf("\n"));
}


puthead(dest, source, ident, seq, type, size)
address dest, source;
char ident, seq, type;
uint16 size;
{
  register uint16 totalsize;
  register tbuf_t *tbp;
  register struct framehdr *fhp;
  register dstnode;

#ifdef LADEBUG
  if (iambusy) {
      panic("Puthead after puthead!\n");
      return;
  }
  iambusy++;
#endif
  tbp = (tbuf_t *)tbaddr[tnext];
  fhp= &tbp->p_opk.opk_fhdr;
  totalsize = size + sizeof(struct framehdr);
  compare(totalsize, <=, 1514);
#ifdef LACHAIN
  curchain = totalsize > TCHAINMIN;
  if (tinuse >= NTRANSMRING-1) {
#else LACHAIN
  if (tinuse >= NTRANSMRING) {
#endif LACHAIN
	DEB(printf("output ring overflow, packet dropped\n"));
#ifdef LAREBOOT
	/*
	** Lance reboot code. There are older revisions of the lance that
	** can hang occasionally.  The symptoms are that nothing is transmitted
	** and no interrupts are generated. The LAREBOOT code tries to detect
	** and correct this situation by stopping and re-initializing the 
	** lance if three consecutive transmits find no free output buffers.
	*/
	if (outptr == 0)	/* twice in a row, do something drastic */
		lareboot(bootseqno);
#endif
	outptr = 0;
#ifdef LADEBUG
	iambusy = 0;
#endif
	return;
  }
  fhp->f_ah.ph_dstnode = dstnode = lobyte(dest);

  if ((dstnode & ETHERBITS) == 0) {
	assert(!eanull(&gwaddr));
	eacopy(&gwaddr, fhp->f_dstaddr);
  } else	/* including broadcast */
	eacopy(eamap[dstnode&0x7F], fhp->f_dstaddr);
  eacopy(&myaddr, fhp->f_srcaddr);
  fhp->f_proto = AMOEBAPROTO;
  enc_s_be(&fhp->f_proto);

  /* Why was this commented out? */
  fhp->f_ah.ph_srcnode = 0;
  fhp->f_ah.ph_dstthread = hibyte(dest);
  fhp->f_ah.ph_srcthread = hibyte(source);
  fhp->f_ah.ph_ident = ident;
  fhp->f_ah.ph_seq = seq;
  fhp->f_ah.ph_type = type;
  fhp->f_ah.ph_flags = 0;
  fhp->f_ah.ph_size = totalsize;
  enc_s_le(&fhp->f_ah.ph_size);
  outsiz = sizeof(struct framehdr);
  if (size == 0) {
	lasend(outsiz,(phys_bytes) 0, 0);
	return;
  }
  outptr = tbp->p_opk.opk_data;
}

append(data, size, send)
phys_bytes data;
register unsigned size;
{
  register rembytes;

  compare(size, !=, 0);
  if (outptr == 0)
	return;
#ifdef LADEBUG
  if (!iambusy) {
      panic("append without puthead");
      return;
  }
#endif
#ifdef LACHAIN
  if (curchain && (rembytes=THEADMIN-outsiz)<size) {
	phys_copy(data, VTOP(outptr), (phys_bytes) rembytes);
	data += rembytes;
	size -= rembytes;
	assert(send);
	lasend(THEADMIN, data, size);
	return;
  }
#endif
  phys_copy(data, VTOP(outptr), (phys_bytes) size);
  outptr += size;
  outsiz += size;
  if (send)
	lasend(outsiz, (phys_bytes) 0, 0);
}

pickoff(data, size)
phys_bytes data;
unsigned size;
{
  phys_copy(VTOP(inptr), data, (phys_bytes) size);
  inptr += size;
}

net_port(p)
port *p;
{

	/* Use our own 48-bit Ether address as a port */

	*p = myaddr;
}

getall(){

	/* We already have everything */
}

void
initnetdev(){
#ifdef VAX2000
  extern vir_bytes mmumap();

  LANCEBASE = (vir_bytes)mmumap(lainfo.lai_devaddr, sizeof(struct lance));
#endif VAX2000
  eacopy(&broadcastaddr,eamap[127]);
  etheraddr((char *) &myaddr);	/* Find out etheraddress */
  printf("Etheraddr: ");
  praddr((char *) &myaddr);	/* print it */
  printf("\n");
  ed = (struct etherdata *) (*lainfo.lai_alloc)(sizeof(struct etherdata), 8);
  compare(BUSADDR((vir_bytes) ed), <, 0x1000000);
  (void) setup_interrupt(&lainfo.lai_intrinfo, netintr);
#ifndef NORANDOM
  randseed((unsigned long) myaddr._portbytes[0], 8, RANDSEED_HOST);
  randseed((unsigned long) myaddr._portbytes[1], 8, RANDSEED_HOST);
  randseed((unsigned long) myaddr._portbytes[2], 8, RANDSEED_HOST);
  randseed((unsigned long) myaddr._portbytes[3], 8, RANDSEED_HOST);
  randseed((unsigned long) myaddr._portbytes[4], 8, RANDSEED_HOST);
  randseed((unsigned long) myaddr._portbytes[5], 8, RANDSEED_HOST);
#endif
  lainit((char *) &myaddr);	/* Initialize Lance chip */
}

static char *
bpraddr(begin, end, eaddr)
char *begin;
char *end;
char eaddr[6];
{
  
  return bprintf(begin, end, "%x:%x:%x:%x:%x:%x",
	eaddr[0]&0xFF,
	eaddr[1]&0xFF,
	eaddr[2]&0xFF,
	eaddr[3]&0xFF,
	eaddr[4]&0xFF,
	eaddr[5]&0xFF);
}

praddr(eaddr)
char eaddr[6];
{

	(void) bpraddr((char *) 0, (char *) 0, eaddr);
}

address interinit(){
  return FAKESITENO;
}

#ifndef SMALL_KERNEL

#ifdef LADUMP
static
ladump(begin, end, dostat, dorecv, dotransm)
char *	begin;
char *	end;
int	dostat;
int	dorecv;
int	dotransm;
{

    register int	i;
    register int	j;
    register char *	p;

    if (dostat) {
	p = bprintf(begin, end, "Lance: csr=0x%x\n", input(LANCEADDR,la_csr));
	p = bprintf(p, end,
		"ib_ladrf=<0x%x,0x%x,0x%x,0x%x>, ib_rlen=0x%x, ib_tlen=0x%x\n",
		ed->ed_ib.ib_ladrf[0], ed->ed_ib.ib_ladrf[1],
		ed->ed_ib.ib_ladrf[2], ed->ed_ib.ib_ladrf[3],
		ed->ed_ib.ib_rlen,ed->ed_ib.ib_tlen);

	p = bprintf(p, end,
		"ib_rdralow=0x%x,ib_rdrahigh=0x%x,ib_tdralow=0x%x,ib_tdrahigh=0x%x,\n",
		ed->ed_ib.ib_rdralow,ed->ed_ib.ib_rdrahigh,
		ed->ed_ib.ib_tdralow,ed->ed_ib.ib_tdrahigh);
    }
    if (dorecv) {
	p = bprintf(p, end,
	    " # FL MCNT WORDS       Receive ring: next=%d,rinext=%d\n",
	    rnext,rinext);

	for (i = 0; i < NRECVRING; i++) {
	    register struct rmde *rp= (struct rmde *) &ed->ed_rmde[i];

	    p = bprintf(p, end, "%d %x %d",i,rp->rmd_flags,rp->rmd_mcnt);
	    for(j = 0; j < 32; j++)
		p = bprintf(p, end, "%s%x%x", j % 8 == 0 ? " " : "",
			(((char *) &ed->ed_rbuf[i])[j]&0xF0)>>4,
			((char *) &ed->ed_rbuf[i])[j]&0xF);
	    p = bprintf(p, end, "\n");
	}
    }
    if (dotransm) {
	p = bprintf(p, end,
		" # FL BCNT  ERR WORDS      Transmit ring: next=%d,full=%d,inuse=%d\n",
		tnext,tfull,tinuse);
	for(i = 0; i < NTRANSMRING; i++) {
	    register struct tmde *rp = (struct tmde *) &ed->ed_tmde[i];

	    p = bprintf(p, end, "%d %x %d %x",
				i, rp->tmd_flags, -rp->tmd_bcnt, rp->tmd_err);
	    for(j = 0;j < 30; j++)
		p = bprintf(p, end, "%s%x%x", j % 8 == 0 ? " " : "",
			(ed->ed_tbuf[i].p_chr[j]&0xF0)>>4,
			ed->ed_tbuf[i].p_chr[j]&0xF);
	    p = bprintf(p, end, "\n");
	}
    }
    return p - begin;
}
#endif LADUMP

#ifdef LASTAT
int
lastatistics(begin, end)
char *	begin;
char *	end;
{
    char *	p;

    p = bprintf(begin, end, "====== Lance statistics ==============\n");
    p = bprintf(p, end, "babl    %7ld ", lastat.ls_babl);
    p = bprintf(p, end, "cerr    %7ld ", lastat.ls_cerr);
    p = bprintf(p, end, "miss    %7ld ", lastat.ls_miss);
    p = bprintf(p, end, "dropped %7ld ", lastat.ls_dropped);
    p = bprintf(p, end, "read    %7ld\n",lastat.ls_read);
    p = bprintf(p, end, "fram    %7ld ", lastat.ls_fram);
    p = bprintf(p, end, "buff    %7ld ", lastat.ls_buff);
    p = bprintf(p, end, "crc     %7ld ", lastat.ls_crc);
    p = bprintf(p, end, "oflo    %7ld ", lastat.ls_oflo);
    p = bprintf(p, end, "coll    %7ld\n",lastat.ls_coll);
    p = bprintf(p, end, "written %7ld ", lastat.ls_written);
    p = bprintf(p, end, "oerrors %7ld ", lastat.ls_oerrors);
    p = bprintf(p, end, "one     %7ld ", lastat.ls_one);
    p = bprintf(p, end, "more    %7ld ", lastat.ls_more);
    p = bprintf(p, end, "def     %7ld\n",lastat.ls_def);
    p = bprintf(p, end, "uflo    %7ld ", lastat.ls_uflo);
    p = bprintf(p, end, "lcar    %7ld ", lastat.ls_lcar);
    p = bprintf(p, end, "lcol    %7ld ", lastat.ls_lcol);
    p = bprintf(p, end, "rtry    %7ld\n",lastat.ls_rtry);
    p = bprintf(p, end, "stopped %7ld ", lastat.ls_stopped);
    p = bprintf(p, end, "reboots %7ld\n",lastat.ls_reboots);
    return p - begin;
}
#endif

/* called by typing something on the console or amstat */
/*ARGSUSED*/
netdump(begin, end)
char *	begin;	/* start ofbuffer in which to dump output */
char *	end;
{
	int sum = 0;

#ifdef LADUMP
	sum += ladump(begin + sum, end, 1, 1, 1);
#endif
#ifdef LASTAT
	sum += lastatistics(begin + sum, end);
#endif
	return sum;
}

eadrdump(begin, end)
char *begin;
char *end;
{
	char *p;
	int i;
	int nonzero;

	p = bprintf(begin, end, "====== Ethernet Address map ============\n");
	nonzero = 0;
	for (i=0; i<MAPENTRIES; i++) {
		if (!eanull(eamap[i])) {
			p = bprintf(p, end, "%x\t", i+0x80);
			p = bpraddr(p, end, eamap[i]);
			p = bprintf(p, end, "\n");
			nonzero++;
		}
	}
	p = bprintf(p, end, "Used %d out of %d\n", nonzero, MAPENTRIES);
	return p - begin;
}
#endif /* SMALL_KERNEL */
