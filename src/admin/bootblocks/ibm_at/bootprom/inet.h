/*	@(#)inet.h	1.2	94/04/06 11:31:48 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Various UDP/IP definitions (see RFC 768, and 791)
 */

/* generic IP address */
#define	IP_ANYADDR	(ipaddr_t)0	/* IP any address */
#define	IP_BCASTADDR	(ipaddr_t)-1 	/* IP broadcast address */

/* some well defined protocols/ports */
#define	IP_PROTO_UDP	17		/* user datagram protocol */
#define	IP_PORT_TFTP	69		/* trivial FTP port */

/* internet address type */
typedef uint32 ipaddr_t;

/* internet address type (only used for printing address) */
typedef union {
    ipaddr_t a;
    struct { uint8 a0, a1, a2, a3; } s;
} inetaddr_t;

/*
 * Structure of an internet header (without options)
 */
typedef struct {
    uint8	ip_vhl;		/* version and header length */
#define	IP_VERSION	4	/* current version number */
    uint8	ip_tos;		/* type of service */
    int16	ip_len;		/* total length */
    uint16	ip_id;		/* identification */
    int16	ip_off;		/* fragment offset field */
    uint8	ip_ttl;		/* time to live */
#define	IP_FRAGTTL	60	/* time to live for frags */
    uint8	ip_p;		/* protocol */
    uint16	ip_sum;		/* checksum */
    ipaddr_t	ip_src;		/* source address */
    ipaddr_t	ip_dst;		/* destination address */
} iphdr_t;

/*
 * Structure of an UDP header
 */
typedef struct {
    uint16	uh_sport;	/* source port */
    uint16	uh_dport;	/* destination port */
    int16	uh_len;		/* udp length */
    uint16	uh_sum;		/* udp checksum */
} udphdr_t;

/* these are actually defined in arp.c */
extern ipaddr_t ip_myaddr;	/* my own IP address */
extern ipaddr_t ip_gateway;	/* gateway's IP address */

extern void ip_printaddr();

