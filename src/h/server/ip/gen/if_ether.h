/*	@(#)if_ether.h	1.3	94/04/06 17:03:00 */
/*
server/ip/gen/if_ether.h

Copyright 1994 Philip Homburg
*/

#ifndef __SERVER__IP__GEN__IF_ETHER_H__
#define __SERVER__IP__GEN__IF_ETHER_H__

struct ether_addr;

#define _PATH_ETHERS "/etc/ethers"

char *ether_ntoa _ARGS(( struct ether_addr *e ));
struct ether_addr *ether_aton _ARGS(( char *s ));
int ether_ntohost _ARGS(( char *hostname, struct ether_addr *e ));
int ether_hostton _ARGS(( char *hostname, struct ether_addr *e ));
int ether_line _ARGS(( char *l, struct ether_addr *e, char *hostname ));

#endif /* __SERVER__IP__GEN__IF_ETHER_H__ */
