/*	@(#)filt_eaddr.c	1.3	94/04/06 09:08:47 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "memaccess.h"
#include "filtabyte.h"
#include "randseed.h"

#define input(p,f)		mem_get_short(&(p)->f)
#define output(p,f,v)		mem_put_short(&(p)->f, v)

filtaddr(device ,eaddr)
struct filtabyte *device;
register char *eaddr;
{
	register bite,shift,result,i,paddr;

	/*
	 * Routine to figure out Ethernet address from LRT Filtabyte
	 * board. Probably the only code here dealing with the board
	 * and not the Lance chip itself.
	 * To understand this code look in the manual.
	 */
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
		if (i>2) /* First bytes always the same */
			randseed((unsigned long) bite, 8, RANDSEED_HOST);
		*eaddr++ = bite;
	}
	output(device,fb_crp,0);
	return 1;
}
