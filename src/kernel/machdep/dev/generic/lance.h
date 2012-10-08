/*	@(#)lance.h	1.3	94/04/06 09:10:44 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef LITTLE_ENDIAN
#define BYTES_REVERSED
#endif LITTLE_ENDIAN

/*
 * defines for structures and bits for the Am7990 LANCE
 */
typedef unsigned char unchar;
typedef unsigned long unlong;

#define LANCEADDR	((volatile struct lance *) LANCEBASE)

struct lance {
	union {
		uint16 csr0;
		uint16 csr1;
		uint16 csr2;
		uint16 csr3;
	} 	la_rdp;
#ifdef VAX2000
	uint16 la_u1;			/* Unused */
#endif
	uint16 la_rap;
#ifdef VAX2000
	uint16	la_u2;			/* Unused */
#endif
};
#define la_csr la_rdp.csr0

/* la_rap functions as a select for la_rdp; */
#define RDP_CSR0	zero
#define RDP_CSR1	1
#define RDP_CSR2	2
#define RDP_CSR3	3

#define bit(b)		(1 << (b))

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

#ifdef BYTES_REVERSED
#define unchar2(char1,char2) unchar char2,char1;
#define PADR_BYTE0	1
#define PADR_BYTE1	0
#define PADR_BYTE2	3
#define PADR_BYTE3	2
#define PADR_BYTE4	5
#define PADR_BYTE5	4
#else
#define unchar2(char1,char2) unchar char1,char2;
#define PADR_BYTE0	0
#define PADR_BYTE1	1
#define PADR_BYTE2	2
#define PADR_BYTE3	3
#define PADR_BYTE4	4
#define PADR_BYTE5	5
#endif BYTES_REVERSED

/* The Initialization Block */

struct InitBlock {
	uint16	ib_mode;	/* modebits, see below                        */
	char    ib_padr[6];	/* physical 48bit Ether-address               */
	uint16 ib_ladrf[4];	/* 64bit hashtable for "logical" addresses    */
	uint16 ib_rdralow;	/* low 16 bits of Receiver Descr. Ring addr   */
	unchar2(ib_rdrahigh,	/* high 8 bits of Receiver Descr. Ring addr   */
		ib_rlen)	/* upper 3 bits are 2log Rec. Ring Length     */
	uint16 ib_tdralow;	/* low 16 bits of Transm. Descr. Ring addr    */
	unchar2(ib_tdrahigh,	/* high 8 bits of Transm. Descr. Ring addr    */
		ib_tlen)	/* upper 3 bits are 2log Transm. Ring Length  */
};

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

struct rmde {
	uint16 rmd_ladr;	/* low 16 bits of bufaddr                     */
	unchar2(rmd_hadr,	/* high 8 bits of bufaddr                     */
		rmd_flags)	/* see below                                  */
	short	rmd_bcnt;	/* two's complement of buffer byte count      */
	uint16	rmd_mcnt;	/* message byte count                         */
};

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

struct tmde {
	uint16 tmd_ladr;	/* low 16 bits of bufaddr                     */
	unchar2(tmd_hadr,	/* high 8 bits of bufaddr                     */
		tmd_flags)	/* see below                                  */
	short	tmd_bcnt;	/* two's complement of buffer byte count      */
	uint16	tmd_err;	/* more error bits + TDR                      */
};

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
