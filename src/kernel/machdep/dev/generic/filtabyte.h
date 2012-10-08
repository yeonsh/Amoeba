/*	@(#)filtabyte.h	1.3	94/04/06 09:08:55 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

struct filtabyte {
	short fb_lap1;		/* Lance port 1 */
	short fb_lap2;		/* Lance port 2 */
	volatile short fb_snp;	/* serial number port */
	volatile short fb_crp;	/* controller reset port */
};

#define bit(n) (1<<(n))

/* bits in fb_snp */
#define SNP_RENA	bit(9)
#define SNP_SNB		bit(8)
#define SNP_ADDR	0xFF

/* bits in fb_crp */
#define CRP_RESET	bit(15)

#define PROMADDR	0x0E

#define FILTADDR	((struct filtabyte *) FILTBASE)
