/*	@(#)ethproto.h	1.2	94/04/06 17:19:48 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Ethernet Protocol Decoding stuff
 *
 * eps_arrived function returns 0 if packet can be thrown away
 */

typedef
struct ether_proto_switch {
	unsigned short eps_proto;
	char *	eps_name;
	int	(*eps_arrived)(/* ifno, srcaddr, pkt */);
} eps_t, *eps_p;


#define EP_AMOEBA3ALT	2222		/* Amoeba 3.0 old */
#define EP_AMOEBA3	0x8145		/* Amoeba 3.0 new */
#define EP_FLIP		0x8146		/* Flip protocol */

#define EP_XNS		0x0600
#define EP_IP		0x0800
#define EP_X25		0x0805
#define EP_ARP		0x0806
#define EP_RARP		0x8035
#define EP_ATALK	0x809B
