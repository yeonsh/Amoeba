/*	@(#)tcpip.h	1.4	96/02/27 10:36:50 */
/*
server/tcpip/tcpip.h
*/

#ifndef __SERVER__TCPIP__TCPIP_H__
#define __SERVER__TCPIP__TCPIP_H__


/* Commands ... */
#define TCPIP_OPEN	(TCPIP_FIRST_COM)
#define TCPIP_IOCTL	(TCPIP_FIRST_COM+1)
#define TCPIP_READ	(TCPIP_FIRST_COM+2)
#define TCPIP_WRITE	(TCPIP_FIRST_COM+3)
#define TCPIP_KEEPALIVE	(TCPIP_FIRST_COM+4)
#define TCPIP_MKCAP	(TCPIP_FIRST_COM+5)

#define TCPIP_ETH_SETOPT	(TCPIP_FIRST_COM+16)
#define TCPIP_ETH_GETOPT	(TCPIP_FIRST_COM+17)
#define TCPIP_ETH_GETSTAT	(TCPIP_FIRST_COM+18)

#define TCPIP_IP_SETCONF	(TCPIP_FIRST_COM+32)
#define TCPIP_IP_GETCONF	(TCPIP_FIRST_COM+33)
#define TCPIP_IP_SETOPT		(TCPIP_FIRST_COM+34)
#define TCPIP_IP_GETOPT		(TCPIP_FIRST_COM+35)

#define TCPIP_IP_GETROUTE	(TCPIP_FIRST_COM+40)
#define TCPIP_IP_SETROUTE	(TCPIP_FIRST_COM+41)
/* #define TCPIP_IP_DELROUTE	(TCPIP_FIRST_COM+42) */

#define TCPIP_TCP_SETCONF	(TCPIP_FIRST_COM+48)
#define TCPIP_TCP_GETCONF	(TCPIP_FIRST_COM+49)
#define TCPIP_TCP_CONNECT	(TCPIP_FIRST_COM+50)
#define TCPIP_TCP_LISTEN	(TCPIP_FIRST_COM+51)
/* skip 52 for hysterical raisins */
#define TCPIP_TCP_SHUTDOWN	(TCPIP_FIRST_COM+53)
#define TCPIP_TCP_SETOPT	(TCPIP_FIRST_COM+54)
#define TCPIP_TCP_GETOPT	(TCPIP_FIRST_COM+55)

#define TCPIP_UDP_SETOPT	(TCPIP_FIRST_COM+64)
#define TCPIP_UDP_GETOPT	(TCPIP_FIRST_COM+65)

/* Stubs and library functions ... */
#ifndef TCPIP_SERVER
errstat tcpip_open _ARGS(( capability *tcpip_cap, capability *chan_cap ));
bufsize tcpip_read _ARGS(( capability *chan_cap, char *buffer, size_t bytes ));
bufsize tcpip_write _ARGS(( capability *chan_cap, char *buffer, size_t bytes ));
errstat tcpip_ioctl _ARGS(( capability *can_cap, unsigned long cmd,
							void *params ));
errstat tcpip_mkcap _ARGS(( capability *tcpip_cap, objnum obj,
							capability *cap ));
errstat tcpip_keepalive _ARGS(( capability *chan_cap, int *respite ));
errstat tcpip_keepalive_cap _ARGS(( capability *cap ));
errstat tcpip_unkeepalive_cap _ARGS(( capability *cap ));
char *tcpip_why _ARGS(( errstat err ));

#define ECONNREFUSED TCPIP_CONNREFUSED
#endif

/* Errors... */

#define TCPIP_NOENT	(TCPIP_FIRST_ERR -2)	/* ? */

#define TCPIP_FAULT	(TCPIP_FIRST_ERR -14)	/* bad address */

#define TCPIP_PACKSIZE	(TCPIP_FIRST_ERR -50)	/* invalid packet size for
						   some protocol */
#define TCPIP_OUTOFBUFS	(TCPIP_FIRST_ERR -51)	/* not enough buffers left */
#define TCPIP_BADIOCTL	(TCPIP_FIRST_ERR -52)	/* illegal ioctl */
#define TCPIP_BADMODE	(TCPIP_FIRST_ERR -53)	/* bad mode in ioctl */

#define TCPIP_BADDEST	(TCPIP_FIRST_ERR -55)	/* not a valid destination 
						   address */
#define TCPIP_DSTNOTRCH	(TCPIP_FIRST_ERR -56)	/* destination not reachable */
#define TCPIP_ISCONN	(TCPIP_FIRST_ERR -57)	/* already connected */
#define TCPIP_ADDRINUSE	(TCPIP_FIRST_ERR -58)	/* address in use */
#define TCPIP_CONNREFUSED (TCPIP_FIRST_ERR -59)	/* connection refused */
#define TCPIP_CONNRESET	(TCPIP_FIRST_ERR -60)	/* connection reset */
#define TCPIP_TIMEDOUT	(TCPIP_FIRST_ERR -61)	/* connection timed out */
#define TCPIP_URG	(TCPIP_FIRST_ERR -62)	/* urgent data present */
#define TCPIP_NOURG	(TCPIP_FIRST_ERR -63)	/* no urgent data present */
#define TCPIP_NOTCONN	(TCPIP_FIRST_ERR -64)	/* no connection (yet or
						 * anymore) */
#define TCPIP_SHUTDOWN	(TCPIP_FIRST_ERR -65)	/* a write call to a shutdown
						 * connection */
#define TCPIP_NOCONN	(TCPIP_FIRST_ERR -66)	/* no such connection */

#define TCPIP_ERROR	(TCPIP_FIRST_ERR -99)	/* generic error */
/* Rights ... */
#define IP_RIGHTS_OPEN		0x80
#define IP_RIGHTS_RWIO		0x40
#define IP_RIGHTS_DESTROY	0x20
#define IP_RIGHTS_LINGER	0x10
#define IP_RIGHTS_SUPER		0x08

#endif /* __SERVER__TCPIP__TCPIP_H__ */
