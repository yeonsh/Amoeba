/*	@(#)ip.h	1.4	96/02/27 14:17:01 */
/*
ip.h

Copyright 1995 Philip Homburg
*/

#ifndef INET_IP_H
#define INET_IP_H

#define IP0	0
#define IP1	1
#define IP2	2
#define IP3	3

/* Prototypes */

struct acc;

void ip_init ARGS(( void ));
int  ip_open ARGS(( int port, int srfd,
	get_userdata_t get_userdata, put_userdata_t put_userdata,
	put_pkt_t put_pkt ));
int ip_ioctl ARGS(( int fd, unsigned long req ));
int ip_read ARGS(( int fd, size_t count ));
int ip_write ARGS(( int fd, size_t count ));
int ip_send ARGS(( int fd, struct acc *data, size_t data_len ));

#endif /* INET_IP_H */

/*
 * $PchId: ip.h,v 1.5 1995/11/21 06:45:27 philip Exp $
 */
