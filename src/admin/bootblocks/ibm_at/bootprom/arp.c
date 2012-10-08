/*	@(#)arp.c	1.1	92/06/25 14:48:39 */
/*
 * arp.c
 *
 * Ethernet (Reverse) Address Resolution Protocol (see RFC 903, and 826).
 * No doubt this code is overkill, but I had it lying around.
 *
 * Copyright (c) 1992 by Leendert van Doorn
 */
#include "assert.h"
#include "param.h"
#include "types.h"
#include "packet.h"
#include "ether.h"
#include "inet.h"
#include "arp.h"

static uint8 bcastaddr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static arptab_t arptab[ARPTAB_SIZE];

static void rarp_whoami();
static ipaddr_t rarp_input();

static arptab_t *arptnew();
static void arptfree();

extern uint32 timer();

ipaddr_t ip_myaddr = IP_ANYADDR;
ipaddr_t ip_gateway = IP_ANYADDR;

/*
 * Using the RARP request/reply exchange we try to obtain our
 * internet address (see RFC 903).
 */
ipaddr_t
rarp_getipaddress(server)
    ipaddr_t *server;
{
    uint32 time, current, timeout;
    register int retry;
    ipaddr_t ipaddr;
    packet_t *pkt;
    int spin = 0;

    printf("Requesting IP address for ");
    eth_printaddr(eth_myaddr);
    printf("\n");

    timeout = 4; /* four seconds */
    for (retry = 0; retry < NRETRIES; retry++) {
	rarp_whoami();
	printf("%c\b", "-\\|/"[spin++ % 4]);

	time = timer() + timeout;
	do {
	    if (pkt = eth_receive()) {
		ip_myaddr = rarp_input(pkt, server);
		pkt_release(pkt);
		if (ip_myaddr) {
		    printf("Using IP address ");
		    ip_printaddr(ip_myaddr);
		    printf("\n");
		    return 1;
		}
	    }
	    current = timer();
	} while (current < time);
	eth_reset();
	timeout <<= 1;
    }
    printf("No response for RARP request\n");
    return IP_ANYADDR;
}

/*
 * Broadcast a RARP request (i.e. who knows who I am)
 */
static void
rarp_whoami()
{
    register arphdr_t *ap;
    packet_t *pkt;

    pkt = pkt_alloc(sizeof(ethhdr_t));
    pkt->pkt_len = sizeof(arphdr_t);
    ap = (arphdr_t *) pkt->pkt_offset;
    ap->arp_hrd = htons(ARPHRD_ETHER);
    ap->arp_pro = htons(ETHTYPE_IP);
    ap->arp_hln = ETH_ADDRSIZE;
    ap->arp_pln = sizeof(ipaddr_t);
    ap->arp_op = htons(REVARP_REQUEST);
    bcopy(eth_myaddr, ap->arp_sha, ETH_ADDRSIZE);
    bcopy(eth_myaddr, ap->arp_tha, ETH_ADDRSIZE);
    eth_send(pkt, ETHTYPE_RARP, bcastaddr);
    pkt_release(pkt);
}

/*
 * Called when packet containing RARP is received
 */
static ipaddr_t
rarp_input(pkt, server)
    packet_t *pkt;
    ipaddr_t *server;
{
    register ethhdr_t *ep;
    register arphdr_t *ap;
    ipaddr_t ipaddr;

    if (pkt->pkt_len < sizeof(arphdr_t))
	return 0;
    ep = (ethhdr_t *)pkt->pkt_offset;
    if (ntohs(ep->eth_proto) != ETHTYPE_RARP)
	return 0;
    ap = (arphdr_t *) (pkt->pkt_offset + sizeof(ethhdr_t));
    if (ntohs(ap->arp_op) != REVARP_REPLY ||
      ntohs(ap->arp_pro) != ETHTYPE_IP)
	return 0;
    if (bcmp(ap->arp_tha, eth_myaddr, ETH_ADDRSIZE))
	return 0;
    bcopy(ap->arp_tpa, (char *)&ipaddr, sizeof(ipaddr_t));
    if (server)
	bcopy(ap->arp_spa, server, sizeof(ipaddr_t));
    return ipaddr;
}

/*
 * Broadcast an ARP packet (i.e. ask who has address "addr")
 */
void
arp_whohas(addr)
    ipaddr_t addr;
{
    register arphdr_t *ap;
    packet_t *pkt;
	
    pkt = pkt_alloc(sizeof(ethhdr_t));
    pkt->pkt_len = sizeof(arphdr_t);
    ap = (arphdr_t *) pkt->pkt_offset;
    ap->arp_hrd = htons(ARPHRD_ETHER);
    ap->arp_pro = htons(ETHTYPE_IP);
    ap->arp_hln = ETH_ADDRSIZE;
    ap->arp_pln = sizeof(ipaddr_t);
    ap->arp_op = htons(ARPOP_REQUEST);
    bcopy(eth_myaddr, ap->arp_sha, ETH_ADDRSIZE);
    bcopy((char *)&ip_myaddr, ap->arp_spa, sizeof(ipaddr_t));
    bcopy((char *)&addr, ap->arp_tpa, sizeof(ipaddr_t));
    eth_send(pkt, ETHTYPE_ARP, bcastaddr);
    pkt_release(pkt);
}

/*
 * Resolve an IP address into a hardware address.  If success, 
 * destha is filled in and 1 is returned.  If there is no entry
 * in arptab, set one up and broadcast a request 
 * for the IP address;  return 0.  Hold onto this packet and 
 * resend it once the address is finally resolved.
 */
int
arp_resolve(pkt, destip, destha)
    packet_t *pkt;
    register ipaddr_t destip;
    register uint8 *destha;
{
    register arptab_t *at;
    int lna = ntohl(destip) & 0xFF;

    if (lna == 0xFF || lna == 0x0) { /* broadcast address */
	bcopy(bcastaddr, destha, ETH_ADDRSIZE);
	return 1;
    }

    ARPTAB_LOOK(at, destip);
    if (at == 0) {
	at = arptnew(destip);
	at->at_hold = pkt;
	arp_whohas(destip);
	return 0;
    }

    at->at_timer = timer(); /* restart the timer */
    if (at->at_flags & ATF_COM) { /* entry is complete */
	bcopy(at->at_eaddr, destha, ETH_ADDRSIZE);
	return 1;
    }

    /*
     * There is an arptab entry, but no hardware address
     * response yet.  Replace the held packet with this
     * latest one.
     */
    if (at->at_hold)
	pkt_release(at->at_hold);
    at->at_hold = pkt;
    arp_whohas(destip);
    return 0;
}


/*
 * Called when packet containing ARP is received.
 * Algorithm is that given in RFC 826.
 */
void
arp_input(pkt)
    packet_t *pkt;
{
    register arphdr_t *ap;
    register arptab_t *at;
    packet_t *phold;
    ipaddr_t isaddr, itaddr;

    if (pkt->pkt_len < sizeof(arphdr_t))
	return;

    ap = (arphdr_t *) (pkt->pkt_offset + sizeof(ethhdr_t));
    if (ntohs(ap->arp_pro) != ETHTYPE_IP)
	return;

    bcopy(ap->arp_spa, (char *)&isaddr, sizeof(ipaddr_t));
    bcopy(ap->arp_tpa, (char *)&itaddr, sizeof(ipaddr_t));
    if (!bcmp(ap->arp_sha, eth_myaddr, ETH_ADDRSIZE))
	return;

    at = (arptab_t *)0;
    ARPTAB_LOOK(at, isaddr);
    if (at) {
	bcopy(ap->arp_sha, at->at_eaddr, ETH_ADDRSIZE);
	at->at_flags |= ATF_COM;
	if (at->at_hold) {
	    phold = at->at_hold;
	    at->at_hold = (packet_t *)0;
	    eth_send(phold, ETHTYPE_IP, at->at_eaddr);
	    pkt_release(phold);
	}
    }

    /*
     * Only answer ARP request which are for me
     */
    if (itaddr != ip_myaddr)
	return;

    if (at == 0) {		/* ensure we have a table entry */
	at = arptnew(isaddr);
	bcopy(ap->arp_sha, at->at_eaddr, ETH_ADDRSIZE);
	at->at_flags |= ATF_COM;
    }
    if (ntohs(ap->arp_op) != ARPOP_REQUEST)
	return;
    bcopy(ap->arp_sha, ap->arp_tha, ETH_ADDRSIZE);
    bcopy(ap->arp_spa, ap->arp_tpa, sizeof(ipaddr_t));
    bcopy(eth_myaddr, ap->arp_sha, ETH_ADDRSIZE);
    bcopy((char *)&itaddr, ap->arp_spa, sizeof(ipaddr_t));
    ap->arp_op = htons(ARPOP_REPLY);
    eth_send(pkt, ETHTYPE_ARP, ap->arp_tha);
}

/*
 * Enter a new address in arptab, pushing out the oldest entry 
 * from the bucket if there is no room.
 */
static arptab_t *
arptnew(addr)
    ipaddr_t addr;
{
    register int n;
    uint32 oldest;
    register arptab_t *at, *ato;

    oldest = ~0;
    ato = at = &arptab[ARPTAB_HASH(addr) * ARPTAB_BSIZ];
    for (n = 0 ; n < ARPTAB_BSIZ ; n++,at++) {
	if (at->at_flags == 0)
	    goto out;	 /* found an empty entry */
	if (at->at_timer < oldest) {
	    oldest = at->at_timer;
	    ato = at;
	}
    }
    at = ato;
    arptfree(at);
out:
    at->at_ipaddr = addr;
    at->at_flags = ATF_INUSE;
    return at;
}

/*
 * Free an arptab entry
 */
static void
arptfree(at)
    register arptab_t *at;
{
    if (at->at_hold)
	pkt_release(at->at_hold);
    at->at_hold = (packet_t *)0;
    at->at_timer = at->at_flags = 0;
    at->at_ipaddr = 0;
}
