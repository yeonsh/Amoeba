/*	@(#)tcp.h	1.2	94/04/06 17:04:29 */
/*
server/ip/gen/tcp.h

Copyright 1994 Philip Homburg
*/

#ifndef __SERVER__IP__GEN__TCP_H__
#define __SERVER__IP__GEN__TCP_H__

#define TCP_MIN_HDR_SIZE	20
#define TCP_MAX_HDR_SIZE	60

#define TCPPORT_TELNET		23
#define TCPPORT_FINGER		79

#define TCPPORT_RESERVED	1024

typedef u16_t tcpport_t;
typedef U16_t Tcpport_t;	/* for use in prototypes */

#endif /* __SERVER__IP__GEN__TCP_H__ */
