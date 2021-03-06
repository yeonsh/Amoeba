/*	@(#)ether.h	1.2	94/04/06 17:02:36 */
/*
server/ip/gen/ether.h

Copyright 1994 Philip Homburg
*/

#ifndef __SERVER__IP__GEN__ETHER_H__
#define __SERVER__IP__GEN__ETHER_H__

#define ETH_MIN_PACK_SIZE	60
#define ETH_MAX_PACK_SIZE	1514
#define ETH_HDR_SIZE		14

typedef struct ether_addr
{
	u8_t ea_addr[6];
} ether_addr_t;

typedef u16_t ether_type_t;
typedef U16_t Ether_type_t;

#define ETH_RARP_PROTO	0x8035
#define ETH_ARP_PROTO	0x806
#define ETH_IP_PROTO	0x800

#endif /* __SERVER__IP__GEN__ETHER_H__ */
