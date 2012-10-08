/*	@(#)ip.c	1.1	96/02/27 10:54:24 */
/*
 * Test the routines of the IP channel of the TCP/IP server.
 * The initial version is deliberately simplistic.  Somebody can
 * make something interesting later if it is needed.
 *
 *	Author: Gregory J. Sharp, June 1995
 */

#include "amoeba.h"
#include "stderr.h"
#include "module/name.h"
#include "server/ip/ip_io.h"
#include "server/ip/types.h"
#include "server/ip/gen/in.h"
#include "server/ip/gen/ip_io.h"
#include "server/ip/gen/route.h"

#include "stdio.h"


void
usage(p)
char *	p;
{
    fprintf(stderr, "Usage: %s ip-cap\n", p);
    exit(1);
}


main(argc, argv)
int	argc;
char *	argv[];
{
    errstat		err;
    capability		ip_cap;
    capability		chan_cap;
    struct nwio_ipopt	ipopt1;
    struct nwio_ipopt	ipopt2;

    if (argc != 2)
    {
	usage(argv[0]);
	/*NOTREACHED*/
    }
    
    if ((err = name_lookup(argv[1], &ip_cap)) != STD_OK)
    {
	fprintf(stderr, "%s: lookup of '%s' failed: %s\n",
					argv[0], argv[1], err_why(err));
	exit(2);
    }

    if ((err = tcpip_open(&ip_cap, &chan_cap)) != STD_OK)
    {
	fprintf(stderr, "%s: tcpip_open failed for '%s': %s\n",
					argv[0], argv[1], tcpip_why(err));
	exit(2);
    }

    /* Test setopt and getopt */
    ipopt1.nwio_flags = NWIO_EXCL |
			NWIO_DI_LOC |
			NWIO_EN_BROAD |
			NWIO_REMANY |
			NWIO_PROTOSPEC |
			NWIO_HDR_O_ANY |
			NWIO_RWDATALL;
    ipopt1.nwio_proto = IPPROTO_TCP;

    if ((err = ip_ioc_setopt(&chan_cap, &ipopt1)) != STD_OK)
    {
	fprintf(stderr, "%s: ip_ioc_setopt failed: %s\n",
					argv[0], tcpip_why(err));
	exit(4);
    }

    if ((err = ip_ioc_getopt(&chan_cap, &ipopt2)) != STD_OK)
    {
	fprintf(stderr, "%s: ip_ioc_getopt failed: %s\n",
					argv[0], tcpip_why(err));
	exit(4);
    }

    if (ipopt1.nwio_flags != ipopt2.nwio_flags ||
	ipopt1.nwio_proto != ipopt2.nwio_proto)
    {
	fprintf(stderr,
	    "%s: the results of getopt are different from the settings made\n",
	    argv[0]);
	fprintf(stderr, "flags set = %x   flags get = %x\n",
				ipopt1.nwio_flags, ipopt2.nwio_flags);
	fprintf(stderr, "proto set = %x   proto get = %x\n",
				ipopt1.nwio_proto, ipopt2.nwio_proto);
	exit(5);
    }

    /* Test all the other ip_ioc routines */
    /* Test the tcpip_* routines */

    exit(0);
}
