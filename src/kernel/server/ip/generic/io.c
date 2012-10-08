/*	@(#)io.c	1.3	96/02/27 14:16:44 */
/*
io.c

Copyright 1995 Philip Homburg
*/

#include <stdlib.h>

#include "inet.h"
#include "io.h"

PUBLIC void writeIpAddr(addr)
ipaddr_t addr;
{
	u8_t *addrInBytes;

	addrInBytes= (u8_t *)&addr;
	printf("%d.%d.%d.%d", addrInBytes[0], addrInBytes[1],
		addrInBytes[2], addrInBytes[3]);
}

PUBLIC void writeEtherAddr(addr)
ether_addr_t *addr;
{
	u8_t *addrInBytes;

	addrInBytes= (u8_t *)(addr->ea_addr);
	printf("%x:%x:%x:%x:%x:%x", addrInBytes[0], addrInBytes[1],
		addrInBytes[2], addrInBytes[3], addrInBytes[4], addrInBytes[5]);
}

/*
 * $PchId: io.c,v 1.5 1995/11/21 06:45:27 philip Exp $
 */
