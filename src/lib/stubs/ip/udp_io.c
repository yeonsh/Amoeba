/*	@(#)udp_io.c	1.2	94/04/07 11:04:52 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
udp_io.c

Created June 28, 1991 by Philip Homburg
*/

#include <sys/types.h>

#include <assert.h>
#include <stdio.h>

#include <amtools.h>

#include <module/buffers.h>

#include <server/ip/tcpip.h>
#include <server/ip/udp_io.h>
#include <server/ip/types.h>
#include <server/ip/gen/in.h>
#include <server/ip/gen/udp.h>
#include <server/ip/gen/udp_io.h>


errstat udp_ioc_setopt(cap, udpopt)
capability *cap;
struct nwio_udpopt *udpopt;
{
	header trans_header;
	char param[256];
	char *p;
	bufsize repl_size;

	p= param;
	p= buf_put_uint32(p, param+sizeof(param), udpopt->nwuo_flags);
	p= buf_put_bytes(p, param+sizeof(param), (void *)&udpopt->nwuo_locport,
		sizeof(udpopt->nwuo_locport));
	p= buf_put_bytes(p, param+sizeof(param), (void *)&udpopt->nwuo_remport,
		sizeof(udpopt->nwuo_remport));
	p= buf_put_bytes(p, param+sizeof(param), (void *)&udpopt->nwuo_locaddr,
		sizeof(udpopt->nwuo_locaddr));
	p= buf_put_bytes(p, param+sizeof(param), (void *)&udpopt->nwuo_remaddr,
		sizeof(udpopt->nwuo_remaddr));
assert(p);
	trans_header.h_port= cap->cap_port;
	trans_header.h_priv= cap->cap_priv;
	trans_header.h_command= TCPIP_UDP_SETOPT;
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

errstat udp_ioc_getopt(cap, udpopt)
capability *cap;
struct nwio_udpopt *udpopt;
{
	header trans_header;
	char param[256];
	char *p, *end_p;
	bufsize repl_size;

	trans_header.h_port= cap->cap_port;
	trans_header.h_priv= cap->cap_priv;
	trans_header.h_command= TCPIP_UDP_GETOPT;
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
	p= buf_get_uint32(p, end_p, &udpopt->nwuo_flags);
	p= buf_get_bytes(p, end_p, &udpopt->nwuo_locport, 
		sizeof(udpopt->nwuo_locport));
	p= buf_get_bytes(p, end_p, &udpopt->nwuo_remport, 
		sizeof(udpopt->nwuo_remport));
	p= buf_get_bytes(p, end_p, &udpopt->nwuo_locaddr, 
		sizeof(udpopt->nwuo_locaddr));
	p= buf_get_bytes(p, end_p, &udpopt->nwuo_remaddr, 
		sizeof(udpopt->nwuo_remaddr));
	if (p != end_p)
		return STD_ARGBAD;
	return STD_OK;
}
