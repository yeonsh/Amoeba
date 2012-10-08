/*	@(#)udp_io.h	1.1	91/11/20 13:09:12 */
/*
udp_io.h

Created June 25, 1991 by Philip Homburg
*/

#ifndef _UDP_IO_H_
#define _UDP_IO_H_

struct nwio_udpopt;

errstat	udp_ioc_setopt _ARGS(( capability *cap, struct nwio_udpopt *udpopt ));
errstat	udp_ioc_getopt _ARGS(( capability *cap, struct nwio_udpopt *udpopt ));

#endif /* _UDP_IO_H_ */
