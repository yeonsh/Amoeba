/*	@(#)etherformat.h	1.2	94/04/06 16:43:40 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __ETHERFORMAT_H__
#define __ETHERFORMAT_H__

#include "etherproto.h"

/* Format of packets on the Ethernet */

#define ETHERBITS	0x80		/* These addresses on Ethernet */

struct etherpacket {
	char	ep_dst[6];
	char	ep_src[6];
	uint16	ep_proto;
	struct pktheader ep_ah;
	char	ep_data[1490];
};

#define ETHSIZE	(14+sizeof(struct pktheader))

#endif /* __ETHERFORMAT_H__ */
