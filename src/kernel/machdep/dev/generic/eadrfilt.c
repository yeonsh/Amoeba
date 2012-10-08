/*	@(#)eadrfilt.c	1.3	94/04/06 09:08:26 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "memaccess.h"
#include "filtabyte.h"

#ifdef IOMAPPED		/* i/o mapped */
#define input(p,f)		(inword((short) (long) &(p)->f))
#define output(p,f,v)		outword((short) (long) &(p)->f, (v))
#else			/* memory mapped */
#define input(p,f)		mem_get_short(&(p)->f)
#define output(p,f,v)		mem_put_short(&(p)->f, v)
#endif

#ifdef DRIVER
#include "config.h"
#ifndef FILTBASE
FILTBASE must be defined
#endif
#else DRIVER
#include "amoeba.h"
#include "machdep.h"
#include "interrupt.h"
#include "lainfo.h"
#define FILTBASE lainfo.lai_devaddr
#endif DRIVER

etheraddr(eaddr)
register char *eaddr;
{
	register volatile struct filtabyte *device;
	register bite,shift,result,i,paddr;

	/*
	 * Routine to figure out Ethernet address from LRT Filtabyte
	 * board. Probably the only code here dealing with the board
	 * and not the Lance chip itself.
	 * To understand this code look in the manual.
	 */
	device = FILTADDR;
	output(device,fb_crp,CRP_RESET);
	paddr = PROMADDR;
	for (i=0;i<6;i++) {
		bite = 0;
		for (shift=8;shift>0;shift--) {
			do {
				output(device,fb_snp,paddr);
				result = input(device,fb_snp);
			} while((result&(SNP_RENA|SNP_ADDR)) != paddr);
			bite |= (result&SNP_SNB)>>shift;
			paddr++;
		}
		*eaddr++ = bite;
	}
	output(device,fb_crp,0);
}
