/*	@(#)barelance.c	1.4	94/04/06 09:08:04 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"

#define LADEBUG

#ifdef STATISTICS
#define LASTAT	/* keep statistics */
#endif

/*
 * Configuration for Amoeba 3.0 kernel
 */
#include "global.h"
#include "machdep.h"
#include "map.h"
#include "interrupt.h"

#include "lainfo.h"
#define LANCEBASE lainfo.lai_devaddr

#define LOGNTRANS 	3	/* 8 transmit buffers */
#define LOGNRECV	3	/* 8 receive buffers */

#ifdef LADEBUG
#define DEB(x) x
#else
#define DEB(x) /* empty */
#endif

#include "assert.h"
INIT_ASSERT

#include "lance.h"
#include "lastat.h"

#ifdef LASTAT
struct lastat lastat;
#endif

#include "memaccess.h"
#ifdef IOMAPPED		/* i/o mapped */
#define input(p,f)		(inword((uint16) (long) &(p)->f))
#define output(p,f,v)		outword((uint16) (long) &(p)->f, (v))
#else			/* memory mapped */
#define input(p,f)		mem_get_short(&(p)->f)
#define output(p,f,v)		mem_put_short(&(p)->f, v)
#endif

#define NTRANSMRING	(1<<LOGNTRANS)
#define NRECVRING	(1<<LOGNRECV)

static
volatile struct etherdata {		/* aligned on 8-byte boundary */
	struct InitBlock ed_ib;
	struct rmde	ed_rmde[NRECVRING];
	struct tmde	ed_tmde[NTRANSMRING];
	phys_bytes	ed_rphys[NRECVRING];
	phys_bytes	ed_tphys[NRECVRING];
} *ed;

static uint16	rnext;	/* Next ringentry to look at in laread() */
static uint16	rinext;	/* Next ringentry to look at in larecv() */
static uint16	rinuse;	/* Number of packets enqueued */
static uint16	tnext;	/* Next ringentry to fill */
static uint16	tfull;	/* Next ringentry to inspect for completion */
static uint16	tinuse;	/* Number of entries in use */

phys_bytes (*bufalloc)();
phys_bytes (*bufback)();
phys_bytes (*bufread)();

static
lainit(etheraddr,crc) char *etheraddr; uint16 *crc; {
	register volatile struct lance *device;
	register i;
	register struct rmde *rp;
	register struct etherdata *E;
	register long baddr;
	static uint16 zero;/* Used to avoid read/modify/write cycles on CSR */

	device = LANCEADDR;
	E = ed;
	/*
	 * Init receive ring
	 */
	for (i=0,rp=E->ed_rmde;i<NRECVRING;i++,rp++) {
		E->ed_rphys[i] = (*bufalloc)();
		baddr = BUSADDR((vir_bytes) E->ed_rphys[i]);
		rp->rmd_ladr = baddr;
		rp->rmd_hadr = baddr>>16;
		rp->rmd_bcnt= -1518;
		rp->rmd_flags= RMD_OWN;
		/*
		 * From this point on we dare not touch this entry
		 * It is OWNed by the Lance.
		 */
	}
	rnext = 0;
	rinext= 0;
	rinuse= 0;

	/*
	 * Init transmit ring
	 */
	for (i=0;i<NTRANSMRING;i++) {
		E->ed_tmde[i].tmd_flags= 0;
	}
	tnext = 0;
	tfull = 0;
	tinuse= 0;

	/*
	 * Fill InitBlock
	 */

	if (etheraddr) {
		E->ed_ib.ib_padr[0] = etheraddr[1];
		E->ed_ib.ib_padr[1] = etheraddr[0];
		E->ed_ib.ib_padr[2] = etheraddr[3];
		E->ed_ib.ib_padr[3] = etheraddr[2];
		E->ed_ib.ib_padr[4] = etheraddr[5];
		E->ed_ib.ib_padr[5] = etheraddr[4];
	}

	if (crc) {
		E->ed_ib.ib_ladrf[0] = crc[0];
		E->ed_ib.ib_ladrf[1] = crc[1];
		E->ed_ib.ib_ladrf[2] = crc[2];
		E->ed_ib.ib_ladrf[3] = crc[3];
	}

	E->ed_ib.ib_rdralow = BUSADDR((vir_bytes) E->ed_rmde);
	E->ed_ib.ib_rdrahigh = BUSADDR((vir_bytes) E->ed_rmde) >> 16;
	E->ed_ib.ib_rlen = LOGNRECV<<5;

	E->ed_ib.ib_tdralow = BUSADDR((vir_bytes) E->ed_tmde);
	E->ed_ib.ib_tdrahigh = BUSADDR((vir_bytes) E->ed_tmde) >> 16;
	E->ed_ib.ib_tlen = LOGNTRANS<<5;

	/*
	 * Initialize Lance chip
	 */

	output(device,la_csr,CSR_STOP);	/* can't be too sure */
	output(device,la_rap,RDP_CSR1);
	output(device,la_rdp.csr1,BUSADDR((vir_bytes) &E->ed_ib));
	output(device,la_rap,RDP_CSR2);
	output(device,la_rdp.csr2,BUSADDR((vir_bytes) &E->ed_ib) >> 16);
	output(device,la_rap,RDP_CSR3);
	output(device,la_rdp.csr3,CSR3_BSWP);
	output(device,la_rap,RDP_CSR0);	/* and stays there */
	output(device,la_csr,CSR_INIT|CSR_STRT);
	while ((input(device,la_csr)&(CSR_IDON|CSR_ERR))==0)
		if (++i > 10000)
			panic("Hanging in Lance initialization")
		;
	if (input(device,la_csr)&CSR_ERR)
		panic("Lance initialization error\n");
	output(device,la_csr,CSR_IDON|CSR_INEA);	/* clear IDON bit ! */
}

static
la2reboot() {

	/* clean up buffers if necessary */
	lainit((char *) 0, (uint16 *) 0);
	DEB(printf("ok\n"));
}

static
net_stop() {

	output(LANCEADDR,la_csr,CSR_STOP);
}

static
lareboot() {

	DEB(printf("Rebooting Lance... "));
	net_stop();
	DEB(printf("stopped... "));
	la2reboot();
}

eth_write(buf,size)
char *buf;
{
	register struct tmde *rp= &ed->ed_tmde[tnext];
	register long baddr;
	
	if (size<60) size=60;
	ed->ed_tphys[tnext] = (phys_bytes) buf;
	baddr = BUSADDR((vir_bytes) ed->ed_tphys[tnext]);
	rp->tmd_ladr = baddr;
	rp->tmd_hadr = baddr>>16;
	rp->tmd_bcnt = -size;
	rp->tmd_flags = TMD_OWN|TMD_STP|TMD_ENP;
	output(LANCEADDR,la_csr,CSR_TDMD|CSR_INEA);
	tnext = circnext(tnext, NTRANSMRING);
	tinuse++;
	STINC(ls_written);
}

laintr() {
	register volatile struct lance *device;
	register uint16 csr;

	device = LANCEADDR;
	for(csr=input(device,la_csr); csr&CSR_INTR; csr=input(device,la_csr)) {
		if (csr&CSR_ERR)
			laerror(csr);
		if ((csr & (CSR_TXON|CSR_RXON)) != (CSR_TXON|CSR_RXON)) {
			printf("Lance stopped!!\n");
			lareboot();
			return;
		}
		if (csr&CSR_TINT) {
			output(device,la_csr,CSR_TINT);	/* clearing INEA */
			latrans();
		}
		if (csr&CSR_RINT) {
			output(device,la_csr,CSR_RINT);	/* clearing INEA */
			larecv();
		}
	}
	output(device,la_csr,CSR_INEA);
}

static
laerror(csr) register uint16 csr; {
	register volatile struct lance *device;

	device = LANCEADDR;
	if (csr&CSR_BABL) {
		output(device,la_csr,CSR_BABL|CSR_INEA);	/* clear bit */
		DEB(printf("Lance chip: babbling error\n"));
		STINC(ls_babl);
	}
	if (csr&CSR_CERR) {
		static int ccnt;
		output(device,la_csr,CSR_CERR|CSR_INEA);	/* clear bit */
		if ((ccnt&0x3F)==0) { /* once every 64 times */
			printf("Lance chip: collission error, check cable\n");
			ccnt=1;
		} else
			ccnt++;
		STINC(ls_cerr);
	}
	if (csr&CSR_MISS) {
		output(device,la_csr,CSR_MISS|CSR_INEA);	/* clear bit */
		DEB(printf("Lance chip: missed packet\n"));
		STINC(ls_miss);
	}
	if (csr&CSR_MERR) {	/* pretty serious, should perhaps be panic */
		printf("Lance stopped because of memory error\n");
		/* reboot will come from netintr() */
	}
}

static
larcverror(flag)
unsigned flag;
{

	/* On Lance revision C some bits are not valid al the time
	   watch out for that problem */

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
	DEB(printf("\n"));
	STINC(ls_dropped);
}

static
laread(rend) long rend; {
	register address to,from;
	register uint16 total;

	STINC(ls_read);
	compare(rend, ==, rnext);
	ed->ed_rphys[rend] = (*bufread)(ed->ed_rphys[rend]);
	ladrop(rend);
}

/*ARGSUSED*/
static
ladrop(rend) long rend; {
	register long baddr;
	register struct rmde *rp;

	compare(rend, ==, rnext);
	baddr = BUSADDR((vir_bytes) ed->ed_rphys[rnext]);
	rp = &ed->ed_rmde[rnext];
	rp->rmd_ladr = baddr;
	rp->rmd_hadr = baddr>>16;
	rp->rmd_flags = RMD_OWN;
	rnext = circnext(rnext,NRECVRING);
	rinuse--;
}

static
larecv() {
	register struct rmde *rp;
	long par;

	while (rinuse < NRECVRING) {
		rp= &ed->ed_rmde[rinext];
		if (rp->rmd_flags&RMD_OWN)
			break;			/* Belongs to Lance */
		rinuse++;
		par = (long) rinext;	/* from laread reboot could be called */
		rinext=circnext(rinext,NRECVRING);
		if (rp->rmd_flags == (RMD_STP|RMD_ENP)) {
			enqueue(laread,par);
		} else {
			larcverror(rp->rmd_flags);
			enqueue(ladrop,par);
		}
	}
}

static
latrans() {
	uint16 fl;

	while (tinuse) {
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
		(*bufback)(ed->ed_tphys[tfull]);
		tfull = circnext(tfull,NTRANSMRING);
		tinuse--;
	}
}

static
latrerror(err) uint16 err; {

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

static
circnext(n,size)
uint16 n;
{

	if (++n == size)
		return 0;
	return n;
}

#ifdef LADUMP
ladump(dostat,dorecv,dotransm) {
	register i,j;

    if (dostat ) {
	printf("Lance: csr=0x%x\n", input(LANCEADDR,la_csr));
	printf("ib_ladrf=<0x%x,0x%x,0x%x,0x%x>, ib_rlen=0x%x, ib_tlen=0x%x\n",
		InitBlock.ib_ladrf[0], InitBlock.ib_ladrf[1],
		InitBlock.ib_ladrf[2], InitBlock.ib_ladrf[3],
		InitBlock.ib_rlen,InitBlock.ib_tlen);
	printf("ib_rdralow=0x%x,ib_rdrahigh=0x%x,ib_tdralow=0x%x,ib_tdrahigh=0x%x,\n",
		InitBlock.ib_rdralow,InitBlock.ib_rdrahigh,
		InitBlock.ib_tdralow,InitBlock.ib_tdrahigh);
    }
    if (dorecv) {
	printf(" # FL MCNT WORDS       Receive ring: next=%d,rinext=%d,inuse=%d\n",rnext,rinext,rinuse);
	for(i=0;i<NRECVRING;i++) {
		register struct rmde *rp= &ed->ed_rmde[i];

		printf("%d %x %d",i,rp->rmd_flags,rp->rmd_mcnt);
		for(j=0;j<32;j++)
			printf("%s%x%x", j%8==0 ? " " : "",
			(((char *) &ed->ed_rbuf[i])[j]&0xF0)>>4,
			((char *) &ed->ed_rbuf[i])[j]&0xF);
		printf("\n");
	}
    }
    if (dotransm) {
	printf(" # FL BCNT  ERR WORDS      Transmit ring: next=%d,full=%d,inuse=%d\n",tnext,tfull,tinuse);
	for(i=0;i<NTRANSMRING;i++) {
		register struct tmde *rp= &ed->ed_tmde[i];

		printf("%d %x %d %x",i,rp->tmd_flags,-rp->tmd_bcnt,rp->tmd_err);
		for(j=0;j<30;j++)
			printf("%s%x%x", j%8==0 ? " " : "",
			(ed->ed_tbuf[i].p_chr[j]&0xF0)>>4,
			ed->ed_tbuf[i].p_chr[j]&0xF);
		printf("\n");
	}
    }
}
#endif LADUMP

#ifndef SMALL_KERNEL
#ifdef LASTAT
lastatistics() {

	printf("====== Lance statistics ==============\n");
	printf("babl    %7ld ",lastat.ls_babl);
	printf("cerr    %7ld ",lastat.ls_cerr);
	printf("miss    %7ld ",lastat.ls_miss);
	printf("dropped %7ld ",lastat.ls_dropped);
	printf("read    %7ld\n",lastat.ls_read);
	printf("fram    %7ld ",lastat.ls_fram);
	printf("buff    %7ld ",lastat.ls_buff);
	printf("crc     %7ld ",lastat.ls_crc);
	printf("oflo    %7ld ",lastat.ls_oflo);
	printf("coll    %7ld\n",lastat.ls_coll);
	printf("written %7ld ",lastat.ls_written);
	printf("oerrors %7ld ",lastat.ls_oerrors);
	printf("one     %7ld ",lastat.ls_one);
	printf("more    %7ld ",lastat.ls_more);
	printf("def     %7ld\n",lastat.ls_def);
	printf("uflo    %7ld ",lastat.ls_uflo);
	printf("lcar    %7ld ",lastat.ls_lcar);
	printf("lcol    %7ld ",lastat.ls_lcol);
	printf("rtry    %7ld\n",lastat.ls_rtry);
}
#endif

netdump() {	/* called by typing something on the console */

#ifdef LADUMP
	ladump(1,1,1);
#endif
#ifdef LASTAT
	lastatistics();
#endif
}
#endif /* SMALL_KERNEL */

uint16 crc[4];

eth_setmcaddr(addr)
char *addr;
{

	crc[0] = crc[1] = crc[2] = crc[3] = 0xFFFF;
}

eth_init(myaddr, p_bufback, p_bufalloc, p_bufread)
char *myaddr;
phys_bytes (*p_bufback)(), (*p_bufalloc)(), (*p_bufread)();
{
  ed = (struct etherdata *) (*lainfo.lai_alloc)(sizeof(struct etherdata), 8);
  setup_interrupt(&lainfo.lai_intrinfo, laintr);
  /* (void) setvec(lainfo.lai_vector, laintr);
  */
  bufback = p_bufback;
  bufalloc = p_bufalloc;
  bufread = p_bufread;
  lainit(myaddr, crc);	/* Initialize Lance chip */
}
