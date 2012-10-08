/*	@(#)rom_eadr.c	1.2	94/04/06 09:06:42 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#define ROM_UPPER_HALF	0xFF020000

etheraddr(eaddr)
register char *eaddr;
{
	register i;
	register char *p;

	p = (char *) ROM_UPPER_HALF;
	for (i=0; i<6; i++) {
		*eaddr = *p;
		eaddr++;
		p += 4;		/* Use only EPROM 0 */
	}
}
