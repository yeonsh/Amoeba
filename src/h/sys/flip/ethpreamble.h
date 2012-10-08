/*	@(#)ethpreamble.h	1.2	94/04/06 17:19:42 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

typedef
struct ether_header {
	char		eh_dstaddr[6];
	char		eh_srcaddr[6];
	unsigned short	eh_proto;
} eh_t, *eh_p;

void eth_arrived(), eth_done();
