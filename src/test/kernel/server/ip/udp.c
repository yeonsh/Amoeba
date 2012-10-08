/*	@(#)udp.c	1.1	96/02/27 10:54:27 */
/*
 * Test the routines of the UDP channel of the TCP/IP server.
 * The initial version is deliberately simplistic.  Somebody can
 * make something interesting later if it is needed.
 *
 *	Author: Gregory J. Sharp, June 1995
 */

#include "amoeba.h"
#include "stderr.h"
#include "module/name.h"
#include "server/ip/udp_io.h"
#include "server/ip/types.h"
#include "server/ip/gen/in.h"
#include "server/ip/gen/udp.h"
#include "server/ip/gen/udp_io.h"

#include "stdio.h"


void
usage(p)
char *	p;
{
    fprintf(stderr, "Usage: %s udp-cap\n", p);
    exit(1);
}


main(argc, argv)
int	argc;
char *	argv[];
{
    errstat		err;
    capability		udp_cap;
    capability		chan_cap;
    struct nwio_udpopt	udpopt1;
    struct nwio_udpopt	udpopt2;

    if (argc != 2)
    {
	usage(argv[0]);
	/*NOTREACHED*/
    }
    
    if ((err = name_lookup(argv[1], &udp_cap)) != STD_OK)
    {
	fprintf(stderr, "%s: lookup of '%s' failed: %s\n",
					argv[0], argv[1], err_why(err));
	exit(2);
    }

    if ((err = tcpip_open(&udp_cap, &chan_cap)) != STD_OK)
    {
	fprintf(stderr, "%s: tcpip_open failed for '%s': %s\n",
					argv[0], argv[1], tcpip_why(err));
	exit(2);
    }

    /* Test setopt and getopt */
    udpopt1.nwuo_flags = NWUO_SHARED |
			 NWUO_LP_SEL |
			 NWUO_DI_BROAD |
			 NWUO_RP_ANY |
			 NWUO_RWDATALL |
			 NWUO_DI_IPOPT;

    if ((err = udp_ioc_setopt(&chan_cap, &udpopt1)) != STD_OK)
    {
	fprintf(stderr, "%s: udp_ioc_setopt failed: %s\n",
					argv[0], tcpip_why(err));
	exit(4);
    }

    if ((err = udp_ioc_getopt(&chan_cap, &udpopt2)) != STD_OK)
    {
	fprintf(stderr, "%s: udp_ioc_getopt failed: %s\n",
					argv[0], tcpip_why(err));
	exit(4);
    }

    if (udpopt1.nwuo_flags != udpopt2.nwuo_flags)
    {
	fprintf(stderr,
	    "%s: the results of getopt are different from the settings made\n",
	    argv[0]);
	fprintf(stderr, "flags set = %x   flags get = %x\n",
				udpopt1.nwuo_flags, udpopt2.nwuo_flags);
	exit(5);
    }

    /* Test all the other udp_ioc routines */
    /* Test the tcpip_* routines */

    exit(0);
}
