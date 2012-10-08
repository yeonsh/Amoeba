/*	@(#)arp.h	1.2	94/04/06 11:31:22 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Ethernet Address Resolution Protocol (see RFC 826)
 */

/*
 * ARP packets are variable in size; the arphdr_t type defines the
 * 10Mb Ethernet variant.  Field names used correspond to RFC 826.
 */
typedef struct {
    uint16	arp_hrd;		/* format of hardware address */
#define	ARPHRD_ETHER	1       	/* ethernet hardware address */
    uint16	arp_pro;		/* format of proto. address  */
    uint8	arp_hln;		/* length of hardware address  */
    uint8	arp_pln;		/* length of protocol address  */
    uint16 	arp_op;
#define	ARPOP_REQUEST	1		/* request to resolve address */
#define	ARPOP_REPLY	2		/* response to previous request */
#define	REVARP_REQUEST	3		/* reverse ARP request */
#define	REVARP_REPLY	4		/* reverse ARP reply */
    uint8	arp_sha[ETH_ADDRSIZE];	/* sender hardware address */
    uint8	arp_spa[4];		/* sender protocol address */
    uint8	arp_tha[ETH_ADDRSIZE];	/* target hardware address */
    uint8	arp_tpa[4];		/* target protocol address */
} arphdr_t;

/*
 * Internet to hardware address resolution table
 */
typedef struct {
    ipaddr_t	at_ipaddr;		/* internet address */
    uint8	at_eaddr[ETH_ADDRSIZE];	/* ethernet address */
    uint32	at_timer;		/* time when referenced */
    uint8	at_flags;		/* flags */
    packet_t	*at_hold;		/* ast packet until resolved/timeout */
} arptab_t;

/* at_flags field values */
#define	ATF_INUSE	1		/* entry in use */
#define ATF_COM		2		/* completed entry (eaddr valid) */

#define	ARPTAB_BSIZ	3		/* bucket size */
#define	ARPTAB_NB	2		/* number of buckets */
#define	ARPTAB_SIZE	(ARPTAB_BSIZ * ARPTAB_NB)


#define	ARPTAB_HASH(a) \
	((short)((((a) >> 16) ^ (a)) & 0x7fff) % ARPTAB_NB)

#define	ARPTAB_LOOK(at, addr) { \
	register n; \
	at = &arptab[ARPTAB_HASH(addr) * ARPTAB_BSIZ]; \
	for (n = 0; n < ARPTAB_BSIZ; n++, at++) \
		if (at->at_ipaddr == addr) \
			break; \
	if (n >= ARPTAB_BSIZ) \
		at = 0; }


extern ipaddr_t rarp_getipaddress();
extern void arp_whohas();
extern int arp_resolve();
extern void arp_input();
