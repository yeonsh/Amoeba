/*	@(#)ip_io.h	1.2	96/02/27 10:36:40 */
/*
ip_io.h

Created July 8, 1991 by Philip Homburg
*/

#ifndef _IP_IO_H
#define _IP_IO_H

/* Are forward struct declarations legal in older C compilers? */
struct nwio_ipconf;
struct nwio_ipopt;
struct nwio_route;

errstat ip_ioc_setconf _ARGS(( capability *cap, struct nwio_ipconf *ipconf ));
errstat ip_ioc_getconf _ARGS(( capability *cap, struct nwio_ipconf *ipconf ));
errstat ip_ioc_setopt _ARGS(( capability *cap, struct nwio_ipopt *ipopt ));
errstat ip_ioc_getopt _ARGS(( capability *cap, struct nwio_ipopt *ipopt ));
errstat ip_ioc_getroute _ARGS(( capability *cap, struct nwio_route *route ));
errstat ip_ioc_setroute _ARGS(( capability *cap, struct nwio_route *route ));

#endif /* _IP_IO_H */
