/*	@(#)eth_io.c	1.3	96/02/27 11:15:32 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
eth_io.c

Created July 17, 1991 by Philip Homburg
*/

#include <assert.h>

#include <amtools.h>

#include <module/buffers.h>

#include <server/ip/types.h>
#include <server/ip/tcpip.h>
#include <server/ip/eth_io.h>
#include <server/ip/gen/ether.h>
#include <server/ip/gen/eth_io.h>

errstat eth_ioc_setopt(cap, ethopt)
capability *cap;
struct nwio_ethopt *ethopt;
{
	header trans_header;
	char param[256];
	char *p;
	bufsize repl_size;

	p= param;
	p= buf_put_uint32(p, param+sizeof(param), ethopt->nweo_flags);
	p= buf_put_bytes(p, param+sizeof(param), (void *)&ethopt->nweo_multi,
		sizeof(ethopt->nweo_multi));
	p= buf_put_bytes(p, param+sizeof(param), (void *)&ethopt->nweo_rem,
		sizeof(ethopt->nweo_rem));
	p= buf_put_uint16(p, param+sizeof(param), ethopt->nweo_type);
assert(p);
	trans_header.h_port= cap->cap_port;
	trans_header.h_priv= cap->cap_priv;
	trans_header.h_command= TCPIP_ETH_SETOPT;
	trans_header.h_size= p-param;
	repl_size= trans(&trans_header, param, trans_header.h_size,
		&trans_header, param, sizeof(param));
	if (ERR_STATUS(repl_size))
	{
#if DEBUG
 { where(); printf("repl_size is error: %d\n", repl_size); endWhere();
#endif
		return ERR_CONVERT(repl_size);
	}
	if ((short)trans_header.h_status < 0)
	{
#if DEBUG
 { where(); printf("h_status is error: %d\n", (short)trans_header.h_status);
	endWhere(); }
#endif
		return (short)trans_header.h_status;
	}
	if (repl_size || trans_header.h_size)
	{
#if DEBUG
 { whre(); printf("repl_size || trans_header.h_size\n"); endWhere(); }
#endif
		return STD_ARGBAD;
	}
	return (short)trans_header.h_status;
}

errstat eth_ioc_getopt(cap, ethopt)
capability *cap;
struct nwio_ethopt *ethopt;
{
	header trans_header;
	char param[256];
	char *p, *end_p;
	bufsize repl_size;

	trans_header.h_port= cap->cap_port;
	trans_header.h_priv= cap->cap_priv;
	trans_header.h_command= TCPIP_ETH_GETOPT;
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
	p= buf_get_uint32(p, param+sizeof(param), (void *)&ethopt->nweo_flags);
	p= buf_get_bytes(p, param+sizeof(param), (void *)&ethopt->nweo_multi,
		sizeof(ethopt->nweo_multi));
	p= buf_get_bytes(p, param+sizeof(param), (void *)&ethopt->nweo_rem,
		sizeof(ethopt->nweo_rem));
	p= buf_get_uint16(p, param+sizeof(param), (void *)&ethopt->nweo_type);
	if (p != end_p)
		return STD_ARGBAD;
	return STD_OK;
}

errstat eth_ioc_getstat(cap, ethstat)
capability *cap;
struct nwio_ethstat *ethstat;
{
	header trans_header;
	char param[256];
	char *p, *end_p;
	bufsize repl_size;

	trans_header.h_port= cap->cap_port;
	trans_header.h_priv= cap->cap_priv;
	trans_header.h_command= TCPIP_ETH_GETSTAT;
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
	p= buf_get_bytes(p, end_p, &ethstat->nwes_addr,
						sizeof(ethstat->nwes_addr));
	p= buf_get_uint32(p, end_p, &ethstat->nwes_stat.ets_recvErr);
	p= buf_get_uint32(p, end_p, &ethstat->nwes_stat.ets_sendErr);
	p= buf_get_uint32(p, end_p, &ethstat->nwes_stat.ets_OVW);
	p= buf_get_uint32(p, end_p, &ethstat->nwes_stat.ets_CRCerr);
	p= buf_get_uint32(p, end_p, &ethstat->nwes_stat.ets_frameAll);
	p= buf_get_uint32(p, end_p, &ethstat->nwes_stat.ets_missedP);
	p= buf_get_uint32(p, end_p, &ethstat->nwes_stat.ets_packetR);
	p= buf_get_uint32(p, end_p, &ethstat->nwes_stat.ets_packetT);
	p= buf_get_uint32(p, end_p, &ethstat->nwes_stat.ets_transDef);
	p= buf_get_uint32(p, end_p, &ethstat->nwes_stat.ets_collision);
	p= buf_get_uint32(p, end_p, &ethstat->nwes_stat.ets_transAb);
	p= buf_get_uint32(p, end_p, &ethstat->nwes_stat.ets_carrSense);
	p= buf_get_uint32(p, end_p, &ethstat->nwes_stat.ets_fifoUnder);
	p= buf_get_uint32(p, end_p, &ethstat->nwes_stat.ets_fifoOver);
	p= buf_get_uint32(p, end_p, &ethstat->nwes_stat.ets_CDheartbeat);
	p= buf_get_uint32(p, end_p, &ethstat->nwes_stat.ets_OWC);
	if (p != end_p)
		return STD_ARGBAD;
	return STD_OK;
}
