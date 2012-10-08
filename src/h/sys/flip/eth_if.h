/*	@(#)eth_if.h	1.2	94/04/06 17:30:28 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
A few prototypes...
*/

struct packet;

void eth_addr _ARGS(( int ifno, char *ifaddr ));
void eth_info _ARGS(( int ifno, char *ifaddr, int *nrcvpkt, int *nsndpkt ));
void eth_send _ARGS(( int ifno, struct packet *pkt, char *dstaddr,
			int /* ushort_t */ proto));
void eth_register _ARGS(( int proto, char *name,
	int (*arrived) _ARGS(( int ifno, struct packet *pkt )) ));
