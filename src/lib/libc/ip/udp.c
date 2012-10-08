/*	@(#)udp.c	1.4	96/02/27 11:09:37 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
libip/tcpip/udp.c
*/

#include <stddef.h>

#include <amoeba.h>
#include <stderr.h>

#include <module/stdcmd.h>

#include <server/ip/types.h>
#include <server/ip/udp_io.h>
#include <server/ip/hton.h>
#include <server/ip/gen/in.h>
#include <server/ip/gen/udp.h>
#include <server/ip/gen/udp_hdr.h>
#include <server/ip/gen/udp_io.h>

#include <server/ip/tcpip.h>
#include <server/ip/udp.h>

#if DEBUG 
#include <stdio.h>
#include <module/mutex.h>
#include <server/ip/debug.h>
#endif

errstat udp_connect(udp_cap, chan_cap, srcport, dstport, dstaddr, flags)
capability *udp_cap;
capability *chan_cap;
udpport_t srcport;
udpport_t dstport;
ipaddr_t dstaddr;
int flags;
{
	errstat result;

#if DEBUG & 2
 { where(); printf("udp_connect called\n"); endWhere(); }
#endif
	result= tcpip_open(udp_cap, chan_cap);
	if (result<0)
	{
#if DEBUG
 { where(); printf("tcpip_open failed\n"); endWhere(); }
#endif
		return result;
	}

	tcpip_keepalive_cap(chan_cap);
	result= udp_reconnect(chan_cap, srcport, dstport, dstaddr, flags);
	if (result<0)
		std_destroy(chan_cap);
	return result;
}

errstat udp_reconnect(chan_cap, srcport, dstport, dstaddr, flags)
capability *chan_cap;
udpport_t srcport;
udpport_t dstport;
ipaddr_t dstaddr;
int flags;
{
	errstat result;
	nwio_udpopt_t udp_opt;
	int udp_flags;

	udp_opt.nwuo_locport= srcport;
	udp_opt.nwuo_remport= dstport;
	udp_opt.nwuo_remaddr= dstaddr;

	udp_flags= 0;
	if (flags & NWUO_ACC_MASK)
		udp_flags= (flags & NWUO_ACC_MASK);
	else
		udp_flags |= NWUO_COPY;

	if (srcport)
		udp_flags |= NWUO_LP_SET;
	else
		udp_flags |= NWUO_LP_SEL;

	udp_flags |= NWUO_EN_LOC;
	udp_flags |= NWUO_EN_BROAD;

	if (dstport)
		udp_flags |= NWUO_RP_SET;
	else
		udp_flags |= NWUO_RP_ANY;

	if (dstaddr)
		udp_flags |= NWUO_RA_SET;
	else
		udp_flags |= NWUO_RA_ANY;

	udp_flags |= NWUO_RWDATALL;
	udp_flags |= NWUO_DI_IPOPT;

	if ((udp_flags & NWUO_LOCPORT_MASK) == NWUO_LP_SET &&
		!(flags & NWUO_ACC_MASK))
	{
		udp_flags= (udp_flags & ~NWUO_ACC_MASK) | NWUO_EXCL;
		udp_opt.nwuo_flags= udp_flags;
		result= udp_ioc_setopt(chan_cap, &udp_opt);
		if (result <0)
			return result;
		udp_flags= (udp_flags & ~NWUO_ACC_MASK) | NWUO_SHARED;
	}
		
	udp_opt.nwuo_flags= udp_flags;
	result= udp_ioc_setopt(chan_cap, &udp_opt);
#if DEBUG
 if (result<0)
 { where(); printf("udp_ioc_setopt failed\n"); endWhere(); }
#endif
	return result;
}

errstat udp_write_msg(chan_cap, msg, msglen, flags)
capability *chan_cap;
char *msg;
int msglen;
int flags;
{
	errstat result;
	udp_io_hdr_t *io_hdr;

	io_hdr= (udp_io_hdr_t *)msg;
	io_hdr->uih_ip_opt_len= HTONS(0);
	io_hdr->uih_data_len= HTONS(0);

	result= tcpip_write(chan_cap, msg, msglen);

	return result;
}

errstat udp_read_msg(chan_cap, msg, msglen, actlen, flags)
capability *chan_cap;
char *msg;
int msglen;
int *actlen;
int flags;
{
	errstat result;
	udp_io_hdr_t *io_hdr;

	result= tcpip_read(chan_cap, msg, msglen);
	if (ERR_STATUS(result))
		return result;

	io_hdr= (udp_io_hdr_t *)msg;

	/* decode the length fields */
	io_hdr->uih_data_len = NTOHS(io_hdr->uih_data_len);
	io_hdr->uih_ip_opt_len = NTOHS(io_hdr->uih_ip_opt_len);

	if (actlen)
		*actlen= io_hdr->uih_data_len + sizeof(*io_hdr);
	return result;
}

errstat udp_close(chan_cap, flags)
capability *chan_cap;
int flags;
{
	tcpip_unkeepalive_cap(chan_cap);
	return STD_OK;
}

errstat udp_destroy(chan_cap, flags)
capability *chan_cap;
int flags;
{
	udp_close(chan_cap, flags);
	std_destroy(chan_cap);
	return STD_OK;
}
