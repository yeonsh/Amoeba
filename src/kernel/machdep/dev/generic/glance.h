/*	@(#)glance.h	1.6	94/04/06 09:09:17 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * defines for structures and bits for the Am7990 LANCE
 */
typedef unsigned char unchar;
typedef unsigned long unlong;

/* Lance cannot access beyond 16Mb */
#define	ADDRESSLIMIT	0x1000000

/* useful macro */
#define bit(n)		(1<<(n))

#ifdef lint
#define volatile /**/
#endif


/*
 * Only the ISA/EISA/MCA architectures do not use memory
 * mapped I/O all other supported architectures do.
 */
#if !defined(ISA) && !defined(MCA)

/* layout of lance's registers */
typedef
struct lance {
	union {
		volatile unshort csr0;
		volatile unshort csr1;
		volatile unshort csr2;
		volatile unshort csr3;
	} 	la_rdp;
	volatile unshort la_rap;
} *lareg_p;

#define	LA_CSR		la_rdp.csr0
#define	LA_CSR1		la_rdp.csr1
#define	LA_CSR2		la_rdp.csr2
#define	LA_CSR3		la_rdp.csr3
#define	LA_RAP		la_rap 
#else /* defined(ISA) || defined(MCA) */
/* variables of type lareg_p hold base I/O address */
typedef int lareg_p;

/*
 * Don't bet on this being some kind of standard for ISA machines.
 * BICC boards seem to do it entirely different.
 */
#define	LA_CSR		0x0
#define	LA_CSR1		0x0
#define	LA_CSR2		0x0
#define	LA_CSR3		0x0
#define	LA_RAP		0x2
#endif /* defined(ISA) || defined(MCA) */

/* la_rap functions as a select for la_rdp; */
#define RDP_CSR0	zero
#define RDP_CSR1	1
#define RDP_CSR2	2
#define RDP_CSR3	3

/* contents of csr0 */
#define CSR_ERR		bit(15)
#define CSR_BABL	bit(14)
#define CSR_CERR	bit(13)
#define CSR_MISS	bit(12)
#define CSR_MERR	bit(11)
#define CSR_RINT	bit(10)
#define CSR_TINT	bit( 9)
#define CSR_IDON	bit( 8)
#define CSR_INTR	bit( 7)
#define CSR_INEA	bit( 6)
#define CSR_RXON	bit( 5)
#define CSR_TXON	bit( 4)
#define CSR_TDMD	bit( 3)
#define CSR_STOP	bit( 2)
#define CSR_STRT	bit( 1)
#define CSR_INIT	bit( 0)

/* csr1 contains low 16 bits of address of Initialization Block */

/* csr2 contains in low byte high 8 bits of address of InitBlock */
/*      upper byte reserved */

/* contents of csr3 */
#define CSR3_BSWP	bit( 2)
#define CSR3_ACON	bit( 1)
#define CSR3_BCON	bit( 0)

/* Now the main-memory structures */
/* The Initialization Block */

typedef
struct InitBlock {
	unshort	ib_mode;	/* modebits, see below                        */
	char    ib_padr[6];	/* physical 48bit Ether-address               */
	unshort ib_ladrf[4];	/* 64bit hashtable for "logical" addresses    */
	unshort ib_rdralow;	/* low 16 bits of Receiver Descr. Ring addr   */
#ifdef __LITTLE_ENDIAN_H__
	unchar	ib_rdrahigh;	/* high 8 bits of Receiver Descr. Ring addr   */
	unchar	ib_rlen;	/* upper 3 bits are 2log Rec. Ring Length     */
#else
	unchar	ib_rlen;	/* upper 3 bits are 2log Rec. Ring Length     */
	unchar	ib_rdrahigh;	/* high 8 bits of Receiver Descr. Ring addr   */
#endif
	unshort ib_tdralow;	/* low 16 bits of Transm. Descr. Ring addr    */
#ifdef __LITTLE_ENDIAN_H__
	unchar	ib_tdrahigh;	/* high 8 bits of Transm. Descr. Ring addr    */
	unchar	ib_tlen;	/* upper 3 bits are 2log Transm. Ring Length  */
#else
	unchar	ib_tlen;	/* upper 3 bits are 2log Transm. Ring Length  */
	unchar	ib_tdrahigh;	/* high 8 bits of Transm. Descr. Ring addr    */
#endif
} IB_t, *IB_p;

/* bits in mode */

#define IB_PROM		bit(15)
#define IB_INTL		bit( 6)
#define IB_DRTY		bit( 5)
#define IB_COLL		bit( 4)
#define IB_DTCR		bit( 3)
#define IB_LOOP		bit( 2)
#define IB_DTX		bit( 1)
#define IB_DRX		bit( 0)

/* A receive message descriptor entry */
typedef
struct rmde {
	unshort rmd_ladr;	/* low 16 bits of bufaddr                     */
#ifdef __LITTLE_ENDIAN_H__
	unchar	rmd_hadr;	/* high 8 bits of bufaddr                     */
	volatile unchar	rmd_flags; 	/* see above                          */
#else
	volatile unchar	rmd_flags; /* see below                               */
	unchar	rmd_hadr;	/* high 8 bits of bufaddr                     */
#endif
	short	rmd_bcnt;	/* two's complement of buffer byte count      */
	volatile unshort	rmd_mcnt; /* message byte count               */
} rmde_t, *rmde_p;

/* bits in flags */

#define RMD_OWN		bit(7)
#define RMD_ERR		bit(6)
#define RMD_FRAM	bit(5)
#define RMD_OFLO	bit(4)
#define RMD_CRC		bit(3)
#define RMD_BUFF	bit(2)
#define RMD_STP		bit(1)
#define RMD_ENP		bit(0)

/* A transmit message descriptor entry */

typedef
struct tmde {
	unshort tmd_ladr;	/* low 16 bits of bufaddr                     */
#ifdef __LITTLE_ENDIAN_H__
	unchar	tmd_hadr;	/* high 8 bits of bufaddr                     */
	volatile unchar	tmd_flags;	/* see above                          */
#else
	volatile unchar	tmd_flags;	/* see below                          */
	unchar	tmd_hadr;	/* high 8 bits of bufaddr                     */
#endif
	short	tmd_bcnt;	/* two's complement of buffer byte count      */
	volatile unshort	tmd_err;	/* more error bits + TDR      */
} tmde_t, *tmde_p;

/* bits in flags */

#define TMD_OWN		bit(7)
#define TMD_ERR		bit(6)
#define TMD_RES		bit(5)	/* unused                                     */
#define TMD_MORE	bit(4)
#define TMD_ONE		bit(3)
#define TMD_DEF		bit(2)
#define TMD_STP		bit(1)
#define TMD_ENP		bit(0)

/* bits in tmd_err */

#define TMDE_BUFF	bit(15)
#define TMDE_UFLO	bit(14)
#define TMDE_RES	bit(13)	/* unused                                     */
#define TMDE_LCOL	bit(12)
#define TMDE_LCAR	bit(11)
#define TMDE_RTRY	bit(10)
#define TMDE_TDR	0x3FF	/* mask for TDR                               */

#define MIN_CHAIN_HEAD_SIZE 100	/* minimum # of bytes in first chain elmt */

/* Circular list increment */
#define BUMPMASK(index, mask)	index = (index+1)&(mask)

