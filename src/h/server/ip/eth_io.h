/*	@(#)eth_io.h	1.2	96/02/27 10:36:34 */
/*
server/ip/eth_io.h

Created July 17, 1991 by Philip Homburg
*/

#ifndef __SERVER__IP__ETH_IO_H__
#define __SERVER__IP__ETH_IO_H__

struct nwio_ethopt;
struct nwio_ethstat;

errstat eth_ioc_setopt _ARGS(( capability *cap, struct nwio_ethopt *ethopt ));
errstat eth_ioc_getopt _ARGS(( capability *cap, struct nwio_ethopt *ethopt ));
errstat eth_ioc_getstat _ARGS(( capability *cap,
						struct nwio_ethstat *ethstat ));

#endif /* __SERVER__IP__ETH_IO_H__ */
