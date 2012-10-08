/*	@(#)tcp_io.c	1.3	96/02/27 11:15:46 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
tcp_io.c

Created June 25, 1991 by Philip Homburg
*/

#include <sys/types.h>

#include <assert.h>
#include <stdio.h>

#include <amoeba.h>
#include <cmdreg.h>
#include <stderr.h>
#include <module/buffers.h>

#include <server/ip/tcpip.h>
#include <server/ip/tcp_io.h>
#include <server/ip/types.h>
#include <server/ip/gen/in.h>
#include <server/ip/gen/tcp.h>
#include <server/ip/gen/tcp_io.h>


#if DEBUG
#include <module/mutex.h>
#include "debug.h"
#endif

errstat tcp_ioc_setconf(cap, conf)
capability *cap;
struct nwio_tcpconf *conf;
{
	header trans_header;
	char param[256];
	char *p;
	bufsize repl_size;

	p= param;
	p= buf_put_uint32(p, param+sizeof(param), conf->nwtc_flags);
	p= buf_put_bytes(p, param+sizeof(param), (void *)&conf->nwtc_locaddr,
		sizeof(conf->nwtc_locaddr));
	p= buf_put_bytes(p, param+sizeof(param), (void *)&conf->nwtc_remaddr,
		sizeof(conf->nwtc_remaddr));
	p= buf_put_bytes(p, param+sizeof(param), (void *)&conf->nwtc_locport,
		sizeof(conf->nwtc_locport));
	p= buf_put_bytes(p, param+sizeof(param), (void *)&conf->nwtc_remport,
		sizeof(conf->nwtc_remport));
	assert(p);
	trans_header.h_port= cap->cap_port;
	trans_header.h_priv= cap->cap_priv;
	trans_header.h_command= TCPIP_TCP_SETCONF;
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

errstat tcp_ioc_getconf(cap, tcpconf)
capability *cap;
struct nwio_tcpconf *tcpconf;
{
	header trans_header;
	char param[256];
	char *p, *end_p;
	bufsize repl_size;

	trans_header.h_port= cap->cap_port;
	trans_header.h_priv= cap->cap_priv;
	trans_header.h_command= TCPIP_TCP_GETCONF;
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
	p= buf_get_uint32(p, end_p, &tcpconf->nwtc_flags);
	p= buf_get_bytes(p, end_p, &tcpconf->nwtc_locaddr,
						sizeof(tcpconf->nwtc_locaddr));
	p= buf_get_bytes(p, end_p, &tcpconf->nwtc_remaddr,
						sizeof(tcpconf->nwtc_remaddr));
	p= buf_get_bytes(p, end_p, &tcpconf->nwtc_locport,
						sizeof(tcpconf->nwtc_locport));
	p= buf_get_bytes(p, end_p, &tcpconf->nwtc_remport,
						sizeof(tcpconf->nwtc_remport));
	if (p != end_p)
		return STD_ARGBAD;
	return STD_OK;
}

errstat tcp_ioc_setopt(cap, tcpopt)
capability *cap;
struct nwio_tcpopt *tcpopt;
{
	header trans_header;
	char param[256];
	char *p;
	bufsize repl_size;

	p= param;
	p= buf_put_uint32(p, param+sizeof(param), tcpopt->nwto_flags);
	assert(p);
	trans_header.h_port= cap->cap_port;
	trans_header.h_priv= cap->cap_priv;
	trans_header.h_command= TCPIP_TCP_SETOPT;
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

errstat tcp_ioc_getopt(cap, tcpopt)
capability *cap;
struct nwio_tcpopt *tcpopt;
{
	header trans_header;
	char param[256];
	char *p, *end_p;
	bufsize repl_size;

	trans_header.h_port= cap->cap_port;
	trans_header.h_priv= cap->cap_priv;
	trans_header.h_command= TCPIP_TCP_GETOPT;
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
	p= buf_get_uint32(p, end_p, &tcpopt->nwto_flags);
	if (p != end_p)
		return STD_ARGBAD;
	return STD_OK;
}

errstat tcp_ioc_connect(cap, clopt)
capability *cap;
struct nwio_tcpcl *clopt;
{
	header trans_header;
	char param[256];
	char *p;
	bufsize repl_size;

	p= param;
	p= buf_put_int32(p, param+sizeof(param), clopt->nwtcl_flags);
	p= buf_put_int32(p, param+sizeof(param), clopt->nwtcl_ttl);
assert(p);
	trans_header.h_port= cap->cap_port;
	trans_header.h_priv= cap->cap_priv;
	trans_header.h_command= TCPIP_TCP_CONNECT;
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

errstat tcp_ioc_listen(cap, clopt)
capability *cap;
struct nwio_tcpcl *clopt;
{
	header trans_header;
	char param[256];
	char *p;
	bufsize repl_size;

	p= param;
	p= buf_put_int32(p, param+sizeof(param), clopt->nwtcl_flags);
	p= buf_put_int32(p, param+sizeof(param), clopt->nwtcl_ttl);
assert(p);
	trans_header.h_port= cap->cap_port;
	trans_header.h_priv= cap->cap_priv;
	trans_header.h_command= TCPIP_TCP_LISTEN;
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

errstat tcp_ioc_shutdown(cap)
capability *cap;
{
	header trans_header;
	bufsize repl_size;

	trans_header.h_port= cap->cap_port;
	trans_header.h_priv= cap->cap_priv;
	trans_header.h_command= TCPIP_TCP_SHUTDOWN;
	trans_header.h_size= 0;
	repl_size= trans(&trans_header, (void *)0, trans_header.h_size,
		&trans_header, (void *)0, 0);
	if (ERR_STATUS(repl_size))
		return ERR_CONVERT(repl_size);
	if ((short)trans_header.h_status < 0)
		return (short)trans_header.h_status;
	if (repl_size || trans_header.h_size)
		return STD_ARGBAD;
	
	return (short)trans_header.h_status;
}
