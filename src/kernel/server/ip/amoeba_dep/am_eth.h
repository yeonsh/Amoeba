/*	@(#)am_eth.h	1.1	91/11/25 12:57:15 */
/*
am_eth.h

Created Sept 30, 19991 by Philip Homburg
*/

#ifndef AM_ETH_H
#define AM_ETH_H

/* Prototypes */

void eth_init_ehook_names ARGS(( void ));
void eth_set_ehook_name ARGS(( int minor, char *ehook_name ));

#endif /* AM_ETH_H */
