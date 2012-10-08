/*	@(#)ifconfig.c	1.3	96/02/27 10:14:22 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
ifconfig.c
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <amoeba.h>
#include <amtools.h>
#include <cmdreg.h>
#include <stderr.h>

#include <module/host.h>

#include <server/ip/tcpip.h>
#include <server/ip/types.h>
#include <server/ip/ip_io.h>
#include <server/ip/gen/in.h>
#include <server/ip/gen/inet.h>
#include <server/ip/gen/ip_io.h>

void usage _ARGS(( void ));
void set_hostaddr _ARGS(( capability *chan_cap, char *host_s, int ins ));
void set_netmask _ARGS(( capability *chan_cap, char *net_s, int ins ));
int check_ipaddrset _ARGS(( capability *chan_cap ));
int check_netmaskset _ARGS(( capability *chan_cap ));
int get_ipconf _ARGS(( capability *chan_cap, struct nwio_ipconf *ref_ipconf ));
void sig_hand _ARGS(( int signal ));
int main _ARGS(( int argc, char *argv[] ));

#define GET_IPCONF_TO 1
char *prog_name;

main(argc, argv)
int argc;
char *argv[];
{
	char *h_arg, *n_arg;
	char *ip_cap_name;
	int i_flag;
	int c;
	char *hostaddr_s, *netmask_s;
	int ins;
	errstat error;
	capability ip_cap, chan_cap;
	nwio_ipconf_t ipconf;

	prog_name= argv[0];

	h_arg= NULL;
	n_arg= NULL;

	i_flag= 0;

	while ((c= getopt(argc, argv, "?h:in:")) != -1)
	{
		switch(c)
		{
		case 'h':
			if (h_arg)
				usage();
			h_arg= optarg;
			break;
		case 'i':
			if (i_flag)
				usage();
			i_flag= 1;
			break;
		case 'n':
			if (n_arg)
				usage();
			n_arg= optarg;
			break;
		case '?':
			usage();
		default:
			fprintf(stderr, "%s: getopt failed\n", prog_name);
			exit(1);
		}
	}

	if (optind >= argc)
		usage();
	ip_cap_name= argv[optind];
	optind++;

	if (optind != argc)
		usage();

	hostaddr_s= h_arg;
	netmask_s= n_arg;
	ins= i_flag;

	error= ip_host_lookup(ip_cap_name, "ip", &ip_cap);
	if (error != STD_OK)
	{
		fprintf(stderr, "%s: unable lookup %s: %s\n",
			prog_name, ip_cap_name, err_why(error));
		exit(1);
	}
	error= tcpip_open(&ip_cap, &chan_cap);
	if (error != STD_OK)
	{
		fprintf(stderr, "%s: unable to tcpip_open: %s\n", prog_name,
			err_why(error));
		exit(1);
	}
	if (hostaddr_s)
		set_hostaddr(&chan_cap, hostaddr_s, ins);

	if (netmask_s)
		set_netmask (&chan_cap, netmask_s, ins);

	if (!get_ipconf(&chan_cap, &ipconf))
	{
		std_destroy(&chan_cap);
		printf("hostaddr not set\n");
		exit(1);
	}
	printf("hostaddr= %s", inet_ntoa(ipconf.nwic_ipaddr));
	if (ipconf.nwic_flags & NWIC_NETMASK_SET)
		printf(" netmask= %s", inet_ntoa(ipconf.nwic_netmask));
	printf("\n");
	std_destroy(&chan_cap);
	return 0;
}

void set_hostaddr (chan_cap, hostaddr_s, ins)
capability *chan_cap;
char *hostaddr_s;
int ins;
{
	ipaddr_t ipaddr;
	struct nwio_ipconf ipconf;
	errstat error;

	ipaddr= inet_addr (hostaddr_s);
	if (ipaddr == (ipaddr_t)(-1))
	{
		fprintf(stderr, "%s: invalid host address (%s)\n",
			prog_name, hostaddr_s);
		exit(1);
	}
	if (ins && check_ipaddrset(chan_cap))
		return;

	ipconf.nwic_flags= NWIC_IPADDR_SET;
	ipconf.nwic_ipaddr= ipaddr;

	error= ip_ioc_setconf(chan_cap, &ipconf);
	if (error<0)
	{
		fprintf(stderr, "%s: unable to ip_ioc_setconf: %s\n",
			prog_name, err_why(error));
		exit(1);
	}
}

int check_ipaddrset (chan_cap)
capability *chan_cap;
{
	struct nwio_ipconf ipconf;

	if (!get_ipconf(chan_cap, &ipconf))
		return 0;

assert (ipconf.nwic_flags & NWIC_IPADDR_SET);

	return 1;
}

int get_ipconf (chan_cap, ref_ipaddr)
capability *chan_cap;
struct nwio_ipconf *ref_ipaddr;
{
	void (*old_sighand) _ARGS(( int ));
	int old_alarm;
	errstat error;

	old_sighand= signal (SIGALRM, sig_hand);
	old_alarm= alarm (GET_IPCONF_TO);

	error= ip_ioc_getconf (chan_cap, ref_ipaddr);

	alarm(0);
	signal(SIGALRM, old_sighand);
	alarm(old_alarm);

	if (error != STD_INTR && error != STD_OK)
	{
		fprintf (stderr, "%s: unable to ip_ioc_getconf: %s\n",
			prog_name, err_why(error));
		exit(1);
	}
	return error == STD_OK;
}

void sig_hand(signal)
int signal;
{
}

void usage()
{
	fprintf(stderr, "USAGE: %s [-h ipaddr] [-i] [-n netmask] ip-cap\n",
		prog_name);
	exit(1);
}

void set_netmask (chan_cap, netmask_s, ins)
capability *chan_cap;
char *netmask_s;
int ins;
{
	ipaddr_t netmask;
	struct nwio_ipconf ipconf;
	errstat error;

	netmask= inet_addr (netmask_s);
	if (netmask == (ipaddr_t)(-1))
	{
		fprintf(stderr, "%s: invalid netmask (%s)\n",
			prog_name, netmask_s);
		exit(1);
	}
	if (ins && check_netmaskset(chan_cap))
		return;

	ipconf.nwic_flags= NWIC_NETMASK_SET;
	ipconf.nwic_netmask= netmask;

	error= ip_ioc_setconf(chan_cap, &ipconf);
	if (error != STD_OK)
	{
		fprintf(stderr, "%s: unable to ip_ioc_setconf: %s\n",
			prog_name, err_why(error));
		exit(1);
	}
}

int check_netmaskset (chan_cap)
capability *chan_cap;
{
	struct nwio_ipconf ipconf;

	if (!get_ipconf(chan_cap, &ipconf))
	{
		printf("unable to determine whether netmask set or not, please set host addr first\n");
		exit(1);
	}

	return (ipconf.nwic_flags & NWIC_NETMASK_SET);
}
