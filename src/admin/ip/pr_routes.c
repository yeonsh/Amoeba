/*	@(#)pr_routes.c	1.3	96/02/27 10:14:39 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
util/ip/pr_routes.c
*/

#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include <amoeba.h>
#include <ampolicy.h>
#include <amtools.h>
#include <stderr.h>

#include <module/host.h>

#include <server/ip/types.h>
#include <server/ip/tcpip.h>
#include <server/ip/ip_io.h>
#include <server/ip/gen/in.h>
#include <server/ip/gen/route.h>
#include <server/ip/gen/netdb.h>
#include <server/ip/gen/inet.h>

#include "_ARGS.h"

char *prog_name;

int main _ARGS(( int argc, char *argv[] ));
void print_route _ARGS(( nwio_route_t *route ));
void usage _ARGS(( void ));

int main (argc, argv)
int argc;
char *argv[];
{
	int nr_routes, i;
	nwio_route_t route;
	capability ip_cap, chan_cap;
	errstat error;
	int argind;
	char *ip_cap_name;

	prog_name= argv[0];
	ip_cap_name= NULL;
	for (argind= 1; argind < argc; argind++)
	{
		if (!strcmp(argv[argind], "-?"))
			usage();
		if (!strcmp(argv[argind], "-I"))
		{
			if (ip_cap_name)
				usage();
			argind++;
			if (argind >= argc)
				usage();
			ip_cap_name= argv[argind];
			continue;
		}
		usage();
	}
	if (!ip_cap_name)
	{
		ip_cap_name= getenv("IP_SERVER");
		if (ip_cap_name && !ip_cap_name[0])
			ip_cap_name= NULL;
	}
	if (!ip_cap_name)
		ip_cap_name= IP_SVR_NAME;
		
	error= ip_host_lookup(ip_cap_name, "ip", &ip_cap);
	if (error != STD_OK)
	{
		fprintf(stderr, "%s: unable to lookup '%s': %s\n", argv[0],
			ip_cap_name, err_why(error));
		exit(1);
	}
	error= tcpip_open(&ip_cap, &chan_cap);
	if (error != STD_OK)
	{
		fprintf(stderr, "%s: unable to tcpip_open: %s\n", argv[0],
			err_why(error));
		exit(1);
	}

	route.nwr_ent_no= 0;
	error= ip_ioc_getroute(&chan_cap, &route);
	if (error != STD_OK)
	{
		std_destroy(&chan_cap);
		fprintf(stderr, "%s: unable to ip_ioc_getroute(): %s\n",
			argv[0], err_why(error));
		exit(1);
	}
	nr_routes= route.nwr_ent_count;
	print_route(&route);
	for (i= 1; i<nr_routes; i++)
	{
		route.nwr_ent_no= i;
		error= ip_ioc_getroute(&chan_cap, &route);
		if (error != STD_OK)
		{
			std_destroy(&chan_cap);
			fprintf(stderr, "%s: unable to ip_ioc_getroute: %s\n",
				argv[0], err_why(error));
			exit(1);
		}
		print_route(&route);
	}
	std_destroy(&chan_cap);
	return 0;
}

void print_route(route)
nwio_route_t *route;
{
	if (!(route->nwr_flags & NWRF_INUSE))
		return;

	printf("%d ", route->nwr_ent_no);
	printf("DEST= %s, ", inet_ntoa(route->nwr_dest));
	printf("NETMASK= %s, ", inet_ntoa(route->nwr_netmask));
	printf("GATEWAY= %s, ", inet_ntoa(route->nwr_gateway));
	printf("dist= %d ", route->nwr_dist);
	printf("pref= %d", route->nwr_pref);
	if (route->nwr_flags & NWRF_STATIC)
		printf(" fixed");
	printf("\n");
}

void usage()
{
	fprintf(stderr, "USAGE: %s [ -I <ip-capability> ]\n", prog_name);
	exit(1);
}

