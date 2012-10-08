/*	@(#)route.h	1.3	96/02/27 10:37:46 */
/*
server/ip/gen/route.h
*/

#ifndef __SERVER__IP__GEN__ROUTE_H__
#define __SERVER__IP__GEN__ROUTE_H__

typedef struct nwio_route
{
	u32_t nwr_ent_no;
	u32_t nwr_ent_count;
	ipaddr_t nwr_dest;
	ipaddr_t nwr_netmask;
	ipaddr_t nwr_gateway;
	u32_t nwr_dist;
	u32_t nwr_flags;
	u32_t nwr_pref;
	ipaddr_t nwr_ifaddr;
} nwio_route_t;

#define NWRF_EMPTY		0
#define NWRF_INUSE		1
#define NWRF_STATIC		2
#define NWRF_UNREACHABLE	4

#endif /* __SERVER__IP__GEN__ROUTE_H__ */

/*
 * $PchHeader: /mount/hd2/minix/include/net/gen/RCS/route.h,v 1.2 1994/04/07 21:04:40 philip Exp $
 */
