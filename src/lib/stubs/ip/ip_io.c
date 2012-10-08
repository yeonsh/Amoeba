/*	@(#)ip_io.c	1.3	96/02/27 11:15:39 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
ip_io.c

Created july 8, 1991 by Philip Homburg
*/

#include <assert.h>

#include <amtools.h>

#include <module/buffers.h>

#include <server/ip/types.h>
#include <server/ip/ip_io.h>
#include <server/ip/tcpip.h>
#include <server/ip/gen/in.h>
#include <server/ip/gen/ip_io.h>
#include <server/ip/gen/route.h>

#if DEBUG
#include <stdio.h>
#include <module/mutex.h>
#include "debug.h"
#endif

errstat ip_ioc_getroute(cap, route)
capability *cap;
struct nwio_route *route;
{
	header trans_header;
	char param[256];
	char *p, *end_p;
	bufsize repl_size;

#if DEBUG
 { where(); printf("route->nwr_ent_no= %d\n", route->nwr_ent_no); endWhere(); }
#endif
	p= param;
	p= buf_put_uint32(p, param+sizeof(param), route->nwr_ent_no);
	p= buf_put_uint32(p, param+sizeof(param), route->nwr_ent_count);
	p= buf_put_bytes(p, param+sizeof(param), &route->nwr_dest,
						sizeof(route->nwr_dest));
	p= buf_put_bytes(p, param+sizeof(param), &route->nwr_netmask,
						sizeof(route->nwr_netmask));
	p= buf_put_bytes(p, param+sizeof(param), &route->nwr_gateway,
						sizeof(route->nwr_gateway));
	p= buf_put_uint32(p, param+sizeof(param), route->nwr_dist);
	p= buf_put_uint32(p, param+sizeof(param), route->nwr_flags);
	p= buf_put_uint32(p, param+sizeof(param), route->nwr_pref);
assert(p);
	trans_header.h_port= cap->cap_port;
	trans_header.h_priv= cap->cap_priv;
	trans_header.h_command= TCPIP_IP_GETROUTE;
	trans_header.h_size= p-param;
	repl_size= trans(&trans_header, param, trans_header.h_size,
		&trans_header, param, sizeof(param));
	if (ERR_STATUS(repl_size))
		return ERR_CONVERT(repl_size);
	if ((short)trans_header.h_status < 0)
		return (short)trans_header.h_status;
	if (repl_size != trans_header.h_size || repl_size > sizeof(param))
		return STD_ARGBAD;

	p= param;
	end_p= p+repl_size;
	p= buf_get_uint32(p, end_p, &route->nwr_ent_no);
	p= buf_get_uint32(p, end_p, &route->nwr_ent_count);
	p= buf_get_bytes(p, end_p, &route->nwr_dest, sizeof(route->nwr_dest));
	p= buf_get_bytes(p, end_p, &route->nwr_netmask,
		sizeof(route->nwr_netmask));
	p= buf_get_bytes(p, end_p, &route->nwr_gateway,
		sizeof(route->nwr_gateway));
	p= buf_get_uint32(p, end_p, &route->nwr_dist);
	p= buf_get_uint32(p, end_p, &route->nwr_flags);
	p= buf_get_uint32(p, end_p, &route->nwr_pref);
	if (p != end_p)
		return STD_ARGBAD;
	
	return STD_OK;
}

errstat ip_ioc_setroute(cap, route)
capability *cap;
struct nwio_route *route;
{
	header trans_header;
	char param[256];
	char *p, *end_p;
	bufsize repl_size;

#if DEBUG
 { where(); printf("route->nwr_ent_no= %d\n", route->nwr_ent_no); endWhere(); }
#endif
	p= param;
	end_p= param+sizeof(param);
	p= buf_put_uint32(p, end_p, route->nwr_ent_no);
	p= buf_put_uint32(p, end_p, route->nwr_ent_count);
	p= buf_put_bytes(p, end_p, &route->nwr_dest, sizeof(route->nwr_dest));
	p= buf_put_bytes(p, end_p, &route->nwr_netmask,
						sizeof(route->nwr_netmask));
	p= buf_put_bytes(p, end_p, &route->nwr_gateway,
						sizeof(route->nwr_gateway));
	p= buf_put_uint32(p, end_p, route->nwr_dist);
	p= buf_put_uint32(p, end_p, route->nwr_flags);
	p= buf_put_uint32(p, end_p, route->nwr_pref);
assert(p);
	trans_header.h_port= cap->cap_port;
	trans_header.h_priv= cap->cap_priv;
	trans_header.h_command= TCPIP_IP_SETROUTE;
	trans_header.h_size= p-param;
	repl_size= trans(&trans_header, param, trans_header.h_size,
		&trans_header, param, sizeof(param));
	if (ERR_STATUS(repl_size))
		return ERR_CONVERT(repl_size);
	if ((short)trans_header.h_status < 0)
		return (short)trans_header.h_status;
	if (repl_size != trans_header.h_size || repl_size)
		return STD_ARGBAD;
	return STD_OK;
}

errstat ip_ioc_setconf(cap, ipconf)
capability *cap;
struct nwio_ipconf *ipconf;
{
	header trans_header;
	char param[256];
	char *p;
	bufsize repl_size;

	p= param;
	p= buf_put_uint32(p, param+sizeof(param), ipconf->nwic_flags);
	p= buf_put_bytes(p, param+sizeof(param), (void *)&ipconf->nwic_ipaddr,
		sizeof(ipconf->nwic_ipaddr));
	p= buf_put_bytes(p, param+sizeof(param), (void *)&ipconf->nwic_netmask,
		sizeof(ipconf->nwic_netmask));
assert(p);
	trans_header.h_port= cap->cap_port;
	trans_header.h_priv= cap->cap_priv;
	trans_header.h_command= TCPIP_IP_SETCONF;
	trans_header.h_size= p-param;
	repl_size= trans(&trans_header, param, trans_header.h_size,
		&trans_header, param, sizeof(param));
	if (ERR_STATUS(repl_size))
		return ERR_CONVERT(repl_size);
	if ((short)trans_header.h_status < 0)
		return (short)trans_header.h_status;
	if (repl_size || trans_header.h_size)
		return STD_ARGBAD;
	
	return (short)trans_header.h_status;
}

errstat ip_ioc_getconf(cap, ipconf)
capability *cap;
struct nwio_ipconf *ipconf;
{
	header trans_header;
	char param[256];
	char *p, *end_p;
	bufsize repl_size;

	trans_header.h_port= cap->cap_port;
	trans_header.h_priv= cap->cap_priv;
	trans_header.h_command= TCPIP_IP_GETCONF;
	trans_header.h_size= 0;
	repl_size= trans(&trans_header, 0, trans_header.h_size,
		&trans_header, param, sizeof(param));
	if (ERR_STATUS(repl_size))
		return ERR_CONVERT(repl_size);
	if ((short)trans_header.h_status < 0)
		return (short)trans_header.h_status;
	if (repl_size != trans_header.h_size || repl_size > sizeof(param))
		return STD_ARGBAD;

	p= param;
	end_p= p+repl_size;
	p= buf_get_uint32(p, end_p, &ipconf->nwic_flags);
	p= buf_get_bytes(p, end_p, &ipconf->nwic_ipaddr,
						sizeof(ipconf->nwic_ipaddr));
	p= buf_get_bytes(p, end_p, &ipconf->nwic_netmask,
						sizeof(ipconf->nwic_netmask));
	if (p != end_p)
		return STD_ARGBAD;
	return STD_OK;
}

errstat ip_ioc_getopt(cap, ipopt)
capability *cap;
struct nwio_ipopt *ipopt;
{
	header trans_header;
	char param[256];
	char *p, *end_p;
	bufsize repl_size;

	trans_header.h_port= cap->cap_port;
	trans_header.h_priv= cap->cap_priv;
	trans_header.h_command= TCPIP_IP_GETOPT;
	trans_header.h_size= 0;
	repl_size= trans(&trans_header, 0, trans_header.h_size,
		&trans_header, param, sizeof(param));
	if (ERR_STATUS(repl_size))
		return ERR_CONVERT(repl_size);
	if ((short)trans_header.h_status < 0)
		return (short)trans_header.h_status;
	if (repl_size != trans_header.h_size || repl_size > sizeof(param))
		return STD_ARGBAD;
	
	p= param;
	end_p= p+repl_size;
	p= buf_get_uint32(p, param+sizeof(param), (void *)&ipopt->nwio_flags);
	p= buf_get_bytes(p, param+sizeof(param), (void *)&ipopt->nwio_rem,
		sizeof(ipopt->nwio_rem));
	p= buf_get_uint8(p, param+sizeof(param), 
				    (void *)&ipopt->nwio_hdropt.iho_opt_siz);
	p= buf_get_bytes(p, param+sizeof(param), 
		(void *)&ipopt->nwio_hdropt.iho_data[0],
		sizeof(ipopt->nwio_hdropt.iho_data));
	p= buf_get_uint8(p, param+sizeof(param), (void *)&ipopt->nwio_tos);
	p= buf_get_uint8(p, param+sizeof(param), (void *)&ipopt->nwio_ttl);
	p= buf_get_uint8(p, param+sizeof(param), (void *)&ipopt->nwio_df);
	p= buf_get_uint8(p, param+sizeof(param), (void *)&ipopt->nwio_proto);
	return (p != end_p) ? STD_ARGBAD : STD_OK;
}

errstat ip_ioc_setopt(cap, ipopt)
capability *cap;
struct nwio_ipopt *ipopt;
{
	header trans_header;
	char param[256];
	char *p;
	bufsize repl_size;

	p= param;
	p= buf_put_uint32(p, param+sizeof(param), ipopt->nwio_flags);
	p= buf_put_bytes(p, param+sizeof(param), (void *)&ipopt->nwio_rem,
		sizeof(ipopt->nwio_rem));
	p= buf_put_uint8(p, param+sizeof(param), 
						ipopt->nwio_hdropt.iho_opt_siz);
	p= buf_put_bytes(p, param+sizeof(param), 
		(void *)&ipopt->nwio_hdropt.iho_data[0],
		sizeof(ipopt->nwio_hdropt.iho_data));
	p= buf_put_uint8(p, param+sizeof(param), ipopt->nwio_tos);
	p= buf_put_uint8(p, param+sizeof(param), ipopt->nwio_ttl);
	p= buf_put_uint8(p, param+sizeof(param), ipopt->nwio_df);
	p= buf_put_uint8(p, param+sizeof(param), ipopt->nwio_proto);
assert(p);
	trans_header.h_port= cap->cap_port;
	trans_header.h_priv= cap->cap_priv;
	trans_header.h_command= TCPIP_IP_SETOPT;
	trans_header.h_size= p-param;
	repl_size= trans(&trans_header, param, trans_header.h_size,
		&trans_header, param, sizeof(param));
	if (ERR_STATUS(repl_size))
		return ERR_CONVERT(repl_size);
	if ((short)trans_header.h_status < 0)
		return (short)trans_header.h_status;
	if (repl_size || trans_header.h_size)
		return STD_ARGBAD;
	
	return (short)trans_header.h_status;
}

