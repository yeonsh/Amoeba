/*	@(#)ether.h	1.2	94/04/06 11:31:41 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Ethernet definitions
 */

#define	ETH_ADDRSIZE	6		/* address size */

/*
 * Structure of an ethernet header
 */
typedef struct {
    uint8 eth_dst[ETH_ADDRSIZE];	/* destination address */
    uint8 eth_src[ETH_ADDRSIZE];	/* source address */
    uint16 eth_proto;			/* protocol type */
} ethhdr_t;

/* protocol types */
#define	ETHTYPE_IP	0x0800		/* IP protocol */
#define	ETHTYPE_ARP	0x0806		/* ARP protocol */
#define	ETHTYPE_RARP	0x8035		/* Reverse ARP protocol */

extern uint8 eth_myaddr[];

extern int eth_init();
extern void eth_reset();
extern void eth_stop();
extern void eth_send();
extern packet_t *eth_receive();
extern void eth_printaddr();
