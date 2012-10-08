/*	@(#)tftp.c	1.2	92/09/08 11:20:03 */
/*
 * tftp.c
 *
 * Trivial File Transfer Protocol (see RFC 783).
 *
 * Copyright (c) 1992 by Leendert van Doorn
 */
#include "assert.h"
#include "param.h"
#include "types.h"
#include "packet.h"
#include "ether.h"
#include "inet.h"
#include "tftp.h"

/*
 * States which TFTP can be in
 */
#define	GOOD_PACKET	0
#define	BAD_PACKET	1
#define	SERV_ABORT	2
#define	CLIENT_ABORT	3
#define	FINISHED	4
#define	STARTING	5

static ipaddr_t tftpsvr;	/* IP address of TFTP server */
static int16 block;		/* current block */
static int ctid, stid;		/* UDP client and server TID (network order) */

extern void ip_send();
extern uint16 in_cksum();
extern uint32 timer();

int
tftp(server, gateway, file)
    ipaddr_t server, gateway;
    char *file;
{
    uint32 time, current, timeout;
    register packet_t *pkt;
    register ethhdr_t *ep;
    register udphdr_t *up;
    register tftphdr_t *tp;
    int state, retry, spin;

    tftpsvr = server;
    printf("Loading file %s", file);
    if (tftpsvr != IP_BCASTADDR) {
	printf(" from server ");
	ip_printaddr(tftpsvr);
    } else
	printf(" using IP broadcast");
    if (gateway) {
	printf(" using gateway ");
	ip_printaddr(gateway);
    }
    printf("\n");

    spin = 0;
    block = 0;
    state = STARTING;
    stid = htons(IP_PORT_TFTP);
    ctid = rand(), ctid = htons(ctid);

    timeout = 4; /* four seconds */
    for (retry = 0; ++retry < NRETRIES; ) {
	/*
	 * Send out a TFTP request. On the first block (actually
	 * zero) we send out a read request. Every other block we
	 * just acknowledge.
	 */
	pkt = pkt_alloc(sizeof(ethhdr_t) + sizeof(iphdr_t));
	up = (udphdr_t *) pkt->pkt_offset;
	tp = (tftphdr_t *) (pkt->pkt_offset + sizeof(udphdr_t));
	if (block == 0) { /* <RRQ> | <filename> | 0 | "octet" | 0 */
	    register char *cp, *p;

	    tp->th_op = htons(TFTP_RRQ);
	    cp = tp->th_stuff;
	    for (p = file; *p; )
		*cp++ = *p++;
	    *cp++ = '\0';
	    *cp++ = 'o';
	    *cp++ = 'c';
	    *cp++ = 't';
	    *cp++ = 'e';
	    *cp++ = 't';
	    *cp++ = '\0';
	    pkt->pkt_len = sizeof(udphdr_t) + (cp - (char *)tp);
	} else { /* else <ACK> | <block> */
	    tp->th_op = htons(TFTP_ACK);
	    tp->th_block = htons(block);
	    pkt->pkt_len = sizeof(udphdr_t) + sizeof(tftphdr_t);
	}
	up->uh_sport = ctid;
	up->uh_dport = stid;
	up->uh_sum = 0;
	up->uh_len = htons(pkt->pkt_len);
	ip_send(pkt, tftpsvr, gateway);
	printf("%c\b", "-\\|/"[spin++ % 4]);
	if (state == FINISHED)
	    return 1;

	/*
	 * Receive TFTP data or ARP packets
	 */
	time = timer(1) + timeout;
	do {
	    if (pkt = eth_receive()) {
		ep = (ethhdr_t *) pkt->pkt_offset;
		switch (ntohs(ep->eth_proto)) {
		case ETHTYPE_ARP:
		    arp_input(pkt);
		    break;
		case ETHTYPE_IP:
		    state = tftp_receive(pkt);
		    if (state != BAD_PACKET) {
			pkt_release(pkt);
			goto got_reply;
		    }
		    break;
		default:
		    break;
		}
		pkt_release(pkt);
	    }
	    current = timer(0);
	} while (current < time);

	eth_reset();

got_reply:
	if (state == SERV_ABORT) {
	    tftp_fail(tftpsvr, ip_myaddr, file, "Aborted by server");
	    return 0;
	} else if (state == CLIENT_ABORT) {
	    tftp_fail(tftpsvr, ip_myaddr, file, "Aborted locally by client");
	    return 0;
	}
	if (current < time)
	    retry = 0;
	else
	    timeout <<= 1;
    }
    tftp_fail(tftpsvr, ip_myaddr, file, "Timed Out");
    return 0;
}

/*
 * Generic TFTP error routine
 */
tftp_fail(fromaddr, toaddr, filename, reason)
    ipaddr_t fromaddr, toaddr;
    char *filename, *reason;
{
    printf("TFTP from ");
    ip_printaddr(fromaddr);
    printf(" to ");
    ip_printaddr(toaddr);
    printf(" file: %s", filename);
    printf(" failed: %s\n", reason);
}

/*
 * Setup the standard IP header fields for a destination,
 * and send packet (possibly using the gateway).
 */
void
ip_send(pkt, dst, gateway)
    packet_t *pkt;
    ipaddr_t dst, gateway;
{
    register iphdr_t *ip;
    uint8 edst[ETH_ADDRSIZE];
    static int ipid = 0;

    pkt->pkt_offset -= sizeof(iphdr_t);
    pkt->pkt_len += sizeof(iphdr_t);
    ip = (iphdr_t *) pkt->pkt_offset;
    ip->ip_vhl = (IP_VERSION << 4) | (sizeof(*ip) >> 2);
    ip->ip_tos = 0;
    ip->ip_len = htons(pkt->pkt_len);
    ip->ip_id = ipid++;
    ip->ip_off = 0;
    ip->ip_ttl = IP_FRAGTTL;
    ip->ip_p = IP_PROTO_UDP;
    ip->ip_src = ip_myaddr ? ip_myaddr : IP_ANYADDR;
    ip->ip_dst = dst;
    ip->ip_sum = 0;
    ip->ip_sum = in_cksum((char *)ip, sizeof(*ip));
    if (arp_resolve(pkt, gateway ? gateway : dst, edst)) {
	eth_send(pkt, ETHTYPE_IP, edst);
	pkt_release(pkt);
    }
}

/*
 * Pseudo header to compute UDP checksum
 */
struct pseudoheader {
    ipaddr_t	ph_src;
    ipaddr_t	ph_dst;
    uint8	ph_zero;
    uint8	ph_prot;
    uint16	ph_length;
};

/*
 * Determine whether this IP packet is the TFTP data packet
 * we were expecting. When a broadcast TFTP request was made
 * we'll set the TFTP server address as well.
 */
int
tftp_receive(pkt)
    packet_t *pkt;
{
    register iphdr_t *ip;
    register udphdr_t *up;
    register tftphdr_t *tp;
    struct pseudoheader ph;
    uint16 oldsum, sum;
    uint16 udplength;
    int length;

    /* check for minimum size tftp packet */
    if (pkt->pkt_len < (sizeof(ethhdr_t) + sizeof(iphdr_t) +
      sizeof(udphdr_t) + sizeof(tftphdr_t)))
	return BAD_PACKET;

    /* IP related checks */
    ip = (iphdr_t *) (pkt->pkt_offset + sizeof(ethhdr_t));
    if (tftpsvr != IP_BCASTADDR && ip->ip_src != tftpsvr)
	return BAD_PACKET;
    if (ntohs(ip->ip_len) < sizeof(iphdr_t) + sizeof(udphdr_t) +
      sizeof(tftphdr_t))
	return BAD_PACKET;
    if (ip->ip_p != IP_PROTO_UDP)
	return BAD_PACKET;
    if (ip_myaddr && ip->ip_dst != ip_myaddr)
	return BAD_PACKET;
    
    /* UDP related checks */
    up = (udphdr_t *) ((char *)ip + sizeof(iphdr_t));
    if (block && up->uh_sport != stid)
	return BAD_PACKET;
    length = ntohs(up->uh_len) - sizeof(udphdr_t) - sizeof(tftphdr_t);
    if (up->uh_dport != ctid || length < 0)
	return BAD_PACKET;

    /* compute UDP checksum if any */
    if (oldsum = up->uh_sum) {
	udplength = ntohs(up->uh_len);
	/*
	 * zero the byte past the last data byte because the
	 * checksum will be over an even number of bytes.
	 */
	if (udplength & 01)	
	    ((char *)up)[udplength] = '\0';
	
	/* set up the pseudo-header */
	ph.ph_src = ip->ip_src;
	ph.ph_dst = ip->ip_dst;
	ph.ph_zero = 0;
	ph.ph_prot = ip->ip_p;
	ph.ph_length = htons(udplength);

	up->uh_sum = ~in_cksum((char *)&ph, sizeof(ph));
	sum = in_cksum((char *)up, (udplength + 1) & ~1);
	up->uh_sum = oldsum; /* put original back */
	if (oldsum == (uint16) -1)
	    oldsum = 0;
	if (sum != oldsum) {
	    printf("Bad checksum %x != %x, length %d from ",
		sum, oldsum, udplength);
	    ip_printaddr(ip->ip_src);
	    printf("\n");
	    return BAD_PACKET;
	}
    }

    /* TFTP related checks */
    tp = (tftphdr_t *) ((char *)up + sizeof(udphdr_t));
    switch (ntohs(tp->th_op)) {
    case TFTP_ERROR:
	printf("Error: %d -> %s\n", ntohs(tp->th_code), &tp->th_msg);
	return SERV_ABORT;
    case TFTP_DATA:
	break;
    default:
	return BAD_PACKET;
    }

    /* reject old packets */
    if (ntohs(tp->th_block) != block + 1)
	return BAD_PACKET;

    /* some TFTP related check */
    if (block == 0) {
	stid = up->uh_sport;
	/* in case of a broadcast, remember server address */
	if (tftpsvr == IP_BCASTADDR) {
	    tftpsvr = ip->ip_src;
	    printf("Found TFTP server at ");
	    ip_printaddr(tftpsvr);
	    printf("\n");
	}
    }
    if (stid != up->uh_sport)
	return BAD_PACKET;

    /* check whether it fits in memory (i.e. does not overwrite us) */
    if ((long)LOADADDR + ((uint32)block * SEGSIZE) >= RAMADDR) {
	printf("Program does not fit in memory\n");
	return CLIENT_ABORT;
    }
    phys_copy(((uint32)dsseg() << 4) + (uint32)&tp->th_data,
	(long)LOADADDR + ((uint32)block * SEGSIZE), (uint32)length);

    /* advance to next block */
    block++;

    return length < SEGSIZE ? FINISHED : GOOD_PACKET;
}

/*
 * One complement check sum
 */
uint16
in_cksum(cp, count)
    char *cp;
    register int count;
{
    register uint16 *sp;
    uint32 sum, oneword = 0x00010000;

    for (sum = 0, sp = (uint16 *)cp, count >>= 1; count--; ) {
	sum += *sp++;
	if (sum >= oneword) {
	    /* wrap carry into low bit */
	    sum -= oneword;
	    sum++;
	}
    }
    if (sum >= oneword) {
	/* wrap carry into low bit */
	sum -= oneword;
	sum++;
    }
    return ~sum;
}

/*
 * Print IP address in a readable form
 */
void
ip_printaddr(addr)
    ipaddr_t addr;
{
    inetaddr_t ip;

    ip.a = addr;
    printf("%d.%d.%d.%d", ip.s.a0, ip.s.a1, ip.s.a2, ip.s.a3);
}
