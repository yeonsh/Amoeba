/*	@(#)tcp_io.h	1.2	96/02/27 10:36:45 */
/*
tcp_io.h

Created June 25, 1991 by Philip Homburg
*/

#ifndef _TCP_IO_H_
#define _TCP_IO_H_

struct nwio_tcpconf;
struct nwio_tcpopt;
struct nwio_tcpcl;

errstat	tcp_ioc_setconf _ARGS(( capability *cap, 
						struct nwio_tcpconf *tcpconf ));
errstat	tcp_ioc_getconf _ARGS(( capability *cap, 
						struct nwio_tcpconf *tcpconf ));
errstat	tcp_ioc_setopt _ARGS(( capability *cap, 
						struct nwio_tcpopt *tcpopt ));
errstat	tcp_ioc_getopt _ARGS(( capability *cap, 
						struct nwio_tcpopt *tcpopt ));
errstat	tcp_ioc_listen _ARGS(( capability *cap, struct nwio_tcpcl *tcpcl ));
errstat	tcp_ioc_connect _ARGS(( capability *cap, struct nwio_tcpcl *tcpcl ));
errstat tcp_ioc_shutdown _ARGS(( capability *cap ));

#endif /* _TCP_IO_H_ */
