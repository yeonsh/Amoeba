/*	@(#)inet_config.c	1.1	96/02/27 14:14:41 */
/*
inet/inet_config.c

Created:	Nov 11, 1992 by Philip Homburg

Give values for structures defined in inet_config.h
*/

#include "inet.h"
#include "generic/type.h"
#include "generic/buf.h"
#include "generic/eth.h"
#include "generic/event.h"
#include "generic/ip.h"
#include "generic/ip_int.h"
#include "generic/psip.h"
#include "generic/sr.h"

struct eth_conf eth_conf[]=
{
	/*	minor		task		port	*/
	{	ETH_DEV0,	NULL,	0	},
};
int eth_conf_nr= sizeof(eth_conf)/sizeof(eth_conf[0]);

struct ip_conf ip_conf[]=
{
	/*	minor		device type	device number	*/
	{	IP_DEV0,	IPDL_ETH,	ETH0		},
};
int ip_conf_nr= sizeof(ip_conf)/sizeof(ip_conf[0]);

struct tcp_conf tcp_conf[]=
{
	/*	minor		port	*/
	{	TCP_DEV0,	IP0	},
};
int tcp_conf_nr= sizeof(tcp_conf)/sizeof(tcp_conf[0]);

struct udp_conf udp_conf[]=
{
	/*	minor		port	*/
	{	UDP_DEV0,	IP0	},
};
int udp_conf_nr= sizeof(udp_conf)/sizeof(udp_conf[0]);

/*
 * $PchHeader: /mount/hd2/minix/sys/inet/RCS/inet_config.c,v 1.2 1994/04/07 22:42:52 philip Exp $
 */
