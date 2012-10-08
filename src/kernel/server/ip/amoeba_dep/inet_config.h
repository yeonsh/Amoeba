/*	@(#)inet_config.h	1.1	96/02/27 14:14:46 */
/*
inet/inet_config.h

Created:	Nov 11, 1992 by Philip Homburg

Defines values for configurable parameters. The structure definitions for
configuration information are also here.
*/

#ifndef INET__INET_CONFIG_H
#define INET__INET_CONFIG_H


#define ETH_PORT_NR	1
#define ARP_PORT_NR	1
#define PSIP_PORT_NR	1
#define IP_PORT_NR	1
#define TCP_PORT_NR	1
#define UDP_PORT_NR	1

struct eth_conf
{
	int ec_minor;		/* Which minor should be used for registering
				 * this device */
	char *ec_task;		/* Kernel task name that implements the
				 * device */
	int ec_port;		/* Port number for that task */
};
extern struct eth_conf eth_conf[];
extern int eth_conf_nr;

struct arp_conf
{
	int ac_port;		/* ethernet port number */
};
extern struct arp_conf arp_conf[];
extern int arp_conf_nr;

struct psip_conf
{
	int pc_minor;		/* Minor device to be used for registering
				 * this device */
};
extern struct psip_conf psip_conf[];
extern int psip_conf_nr;

struct ip_conf
{
	int ic_minor;		/* Minor device to be used for registering
				 * this device */
	int ic_devtype;		/* underlying device type */
	int ic_port;		/* port of underlying device */
};
extern struct ip_conf ip_conf[];
extern int ip_conf_nr;

struct tcp_conf
{
	int tc_minor;		/* Which minor should be used for registering
				 * this device */
	int tc_port;		/* IP port number */
};
extern struct tcp_conf tcp_conf[];
extern int tcp_conf_nr;

struct udp_conf
{
	int uc_minor;		/* Which minor should be used for registering
				 * this device */
	int uc_port;		/* IP port number */
};
extern struct udp_conf udp_conf[];
extern int udp_conf_nr;

#endif /* INET__INET_CONFIG_H */

/*
 * $PchHeader: /mount/hd2/minix/sys/inet/RCS/inet_config.h,v 1.2 1994/04/07 22:42:52 philip Exp $
 */
