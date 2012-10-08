/*	@(#)udp.h	1.1	91/11/20 13:09:04 */
/*
udp.h
*/

#ifndef _UDP_H
#define _UDP_H

errstat udp_connect _ARGS(( capability *udp_cap, capability *chan_cap,
	Udpport_t srcport, Udpport_t dstport, ipaddr_t dstaddr, int flags ));

errstat udp_reconnect _ARGS(( capability *chan_cap, Udpport_t srcport, 
	Udpport_t dstport, ipaddr_t dstaddr, int flags ));

errstat udp_read_msg _ARGS(( capability *chan_cap, char *msg, int msglen,
	int *actlen, int flags ));

errstat udp_write_msg _ARGS(( capability *chan_cap, char *msg, int msglen,
	int flags ));

errstat udp_close _ARGS(( capability *chan_cap, int flags ));

errstat udp_destroy _ARGS(( capability *chan_cap, int flags ));

#endif /* _UDP_H */
