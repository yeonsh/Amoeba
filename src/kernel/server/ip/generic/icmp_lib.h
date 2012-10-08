/*	@(#)icmp_lib.h	1.3	96/02/27 14:16:39 */
/*
icmp_lib.h

Created Sept 30, 1991 by Philip Homburg

Copyright 1995 Philip Homburg
*/

#ifndef ICMP_LIB_H
#define ICMP_LIB_H

/* Prototypes */

void icmp_getnetmask ARGS(( int ip_port ));
void icmp_snd_parmproblem ARGS(( acc_t *pack ));
void icmp_snd_time_exceeded ARGS(( int port_nr, acc_t *pack, int code ));
void icmp_snd_unreachable ARGS(( int port_nr, acc_t *pack, int code ));
void icmp_snd_redirect ARGS(( int port_nr, acc_t *pack, int code,
							ipaddr_t gw ));

#endif /* ICMP_LIB_H */

/*
 * $PchId: icmp_lib.h,v 1.4 1995/11/21 06:45:27 philip Exp $
 */
