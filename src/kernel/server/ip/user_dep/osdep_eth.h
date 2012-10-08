/*	@(#)osdep_eth.h	1.1	91/11/25 13:15:43 */
/*
osdep_eth.h
*/

#ifndef OSDEP_ETH_H
#define OSDEP_ETH_H

#define port	am_port_t
#define header	am_header_t

#include <amoeba.h>

#undef port
#undef header

typedef struct osdep_eth_port
{
	capability etp_cap;
	char *etp_ehook_name;
} osdep_eth_port_t;

#endif /* OSDEP_ETH_H */
