/*	@(#)add_route.c	1.3	96/02/27 10:13:35 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
add_route.c

Created August 7, 1991 by Philip Homburg

Modified: Gregory J. Sharp, Sept 1995
			Unified IP admin command-line interfaces.
*/

#define _POSIX2_SOURCE	1

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <_ARGS.h>
#include <ampolicy.h>
#include <amtools.h>
#include <module/host.h>

#include <server/ip/hton.h>
#include <server/ip/types.h>
#include <server/ip/tcpip.h>
#include <server/ip/gen/netdb.h>
#include <server/ip/gen/in.h>
#include <server/ip/gen/inet.h>
#include <server/ip/gen/route.h>
#include <server/ip/ip_io.h>

char *prog_name;

void main _ARGS(( int argc, char *argv[] ));
void usage _ARGS(( void ));

void main(argc, argv)
int argc;
char *argv[];
{
	int c;
	char *ip_arg, *d_arg, *g_arg, *n_arg;
	capability ip_cap, chan_cap;
	struct hostent *hostent;
	ipaddr_t gateway, destination, netmask;
	u8_t high_byte;
	errstat result;
	nwio_route_t route;

	prog_name= argv[0];

	ip_arg= NULL;
	d_arg= NULL;
	g_arg= NULL;
	n_arg= NULL;

	while ((c= getopt(argc, argv, "?I:g:d:n:")) != -1)
	{
		switch(c)
		{
		case 'I':
			if (ip_arg)
				usage();
			ip_arg= optarg;
			break;
		case 'g':
			if (g_arg)
				usage();
			g_arg= optarg;
			break;
		case 'd':
			if (d_arg)
				usage();
			d_arg= optarg;
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
	if (optind != argc || !g_arg || (n_arg && !d_arg))
		usage();
	
	hostent= gethostbyname(g_arg);
	if (!hostent)
	{
		fprintf(stderr, "%s: unknown host '%s'\n", prog_name, g_arg);
		exit(1);
	}
	gateway= *(ipaddr_t *)(hostent->h_addr);

	destination= 0;
	netmask= 0;

	if (d_arg)
	{
		hostent= gethostbyname(d_arg);
		if (!hostent)
		{
			fprintf(stderr, "%s: unknown host '%s'\n", d_arg);
			exit(1);
		}
		destination= *(ipaddr_t *)(hostent->h_addr);
		high_byte= *(u8_t *)&destination;
		if (!(high_byte & 0x80))	/* class A or 0 */
		{
			if (destination)
				netmask= HTONL(0xff000000);
		}
		else if (!(high_byte & 0x40))	/* class B */
		{
			netmask= HTONL(0xffff0000);
		}
		else if (!(high_byte & 0x20))	/* class C */
		{
			netmask= HTONL(0xffffff00);
		}
		else				/* class D is multicast ... */
		{
			fprintf(stderr, "%s: warning marsian address '%s'\n",
				inet_ntoa(destination));
			netmask= HTONL(0xffffffff);
		}
	}

	if (n_arg)
	{
		hostent= gethostbyname(n_arg);
		if (!hostent)
		{
			fprintf(stderr, "%s: unknown host '%s'\n", n_arg);
			exit(1);
		}
		netmask= *(ipaddr_t *)(hostent->h_addr);
	}
		
	if (!ip_arg)
	{
		ip_arg= getenv("IP_SERVER");
		if (ip_arg && !ip_arg[0])
			ip_arg= NULL;
	}
	if (!ip_arg)
		ip_arg= IP_SVR_NAME;

	result= ip_host_lookup(ip_arg, "ip", &ip_cap);
	if (result != STD_OK)
	{
		fprintf(stderr, "%s: unable to lookup '%s': %s\n",
			prog_name, ip_arg, err_why(result));
		exit(1);
	}

	result= tcpip_open(&ip_cap, &chan_cap);
	if (result != STD_OK)
	{
		fprintf(stderr, "%s: unable to tcpip_open(): %s\n",
			prog_name, err_why(result));
		exit(1);
	}

	printf("adding route to %s ", inet_ntoa(destination));
	printf("with netmask %s ", inet_ntoa(netmask));
	printf("using gateway %s\n", inet_ntoa(gateway));

	route.nwr_dest= destination;
	route.nwr_netmask= netmask;
	route.nwr_gateway= gateway;
	route.nwr_dist= 1;
	route.nwr_flags= NWRF_STATIC;

	result= ip_ioc_setroute(&chan_cap, &route);
	if (result != STD_OK)
	{
		fprintf(stderr, "%s: ip_ioc_setroute() failed: %s\n",
			prog_name, err_why(result));
		std_destroy(&chan_cap);
		exit(1);
	}
	std_destroy(&chan_cap);
	exit(0);
}

void usage()
{
	fprintf(stderr,
		"USAGE: %s -g <gateway> [-d <destination> [-n <netmask> ]]\n", 
			prog_name);
	fprintf(stderr, "\t\t\t\t[-I <IP server cap/host>]\n");
	exit(1);
}

