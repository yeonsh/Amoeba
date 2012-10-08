/*	@(#)eth.c	1.1	96/02/27 10:54:22 */
/*
 * Test the routines of the ETH channel of the TCP/IP server.
 * The initial version is deliberately simplistic.  Somebody can
 * make something interesting later if it is needed.
 *
 *	Author: Gregory J. Sharp, June 1995
 */

#include "amoeba.h"
#include "stderr.h"
#include "module/name.h"
#include "server/ip/eth_io.h"
#include "server/ip/types.h"
#include "server/ip/gen/ether.h"
#include "server/ip/gen/eth_io.h"

#include "stdio.h"


int
comp_eaddr(a, b)
ether_addr_t *	a;
ether_addr_t *	b;
{
    int	i;

    for (i = 0; i < 6; i++)
    {
	if (a->ea_addr[i] != b->ea_addr[i])
	    return 0;
    }
    return 1;
}


void
usage(p)
char *	p;
{
    fprintf(stderr, "Usage: %s eth-cap\n", p);
    exit(1);
}


main(argc, argv)
int	argc;
char *	argv[];
{
    errstat		err;
    capability		eth_cap;
    capability		chan_cap;
    struct nwio_ethstat	ethstat;
    struct nwio_ethopt	ethopt1;
    struct nwio_ethopt	ethopt2;

    if (argc != 2)
    {
	usage(argv[0]);
	/*NOTREACHED*/
    }
    
    if ((err = name_lookup(argv[1], &eth_cap)) != STD_OK)
    {
	fprintf(stderr, "%s: lookup of '%s' failed: %s\n",
					argv[0], argv[1], err_why(err));
	exit(2);
    }

    if ((err = tcpip_open(&eth_cap, &chan_cap)) != STD_OK)
    {
	fprintf(stderr, "%s: tcpip_open failed for '%s': %s\n",
					argv[0], argv[1], tcpip_why(err));
	exit(2);
    }

    /* Test getstat */
    if ((err = eth_ioc_getstat(&chan_cap, &ethstat)) != STD_OK)
    {
	fprintf(stderr, "%s: eth_ioc_getstat failed: %s\n",
					argv[0], tcpip_why(err));
	exit(3);
    }
    else
    {
	printf("%s: Ethernet addr of chan = %x:%x:%x:%x:%x:%x\n",
			argv[0],
			ethstat.nwes_addr.ea_addr[0],
			ethstat.nwes_addr.ea_addr[1],
			ethstat.nwes_addr.ea_addr[2],
			ethstat.nwes_addr.ea_addr[3],
			ethstat.nwes_addr.ea_addr[4],
			ethstat.nwes_addr.ea_addr[5]);
    }

    /* Test setopt and getopt */

    ethopt1.nweo_flags = NWEO_EXCL | NWEO_DI_LOC | NWEO_DI_PROMISC |
			 NWEO_DI_MULTI | NWEO_EN_BROAD |
			 NWEO_REMANY | NWEO_TYPEANY | NWEO_RWDATALL;
    ethopt1.nweo_type = ETH_RARP_PROTO;
    /* Stick in an (unused) unicast address */
    ethopt1.nweo_rem.ea_addr[0] = 0;
    ethopt1.nweo_rem.ea_addr[1] = 6;
    ethopt1.nweo_rem.ea_addr[2] = 5;
    ethopt1.nweo_rem.ea_addr[3] = 4;
    ethopt1.nweo_rem.ea_addr[4] = 3;
    ethopt1.nweo_rem.ea_addr[5] = 2;
    /* Stick in an (unused) multicast address */
    ethopt1.nweo_multi.ea_addr[0] = 1;
    ethopt1.nweo_multi.ea_addr[1] = 6;
    ethopt1.nweo_multi.ea_addr[2] = 5;
    ethopt1.nweo_multi.ea_addr[3] = 4;
    ethopt1.nweo_multi.ea_addr[4] = 3;
    ethopt1.nweo_multi.ea_addr[5] = 2;


    if ((err = eth_ioc_setopt(&chan_cap, &ethopt1)) != STD_OK)
    {
	fprintf(stderr, "%s: eth_ioc_setopt failed: %s\n",
					argv[0], tcpip_why(err));
	exit(4);
    }

    if ((err = eth_ioc_getopt(&chan_cap, &ethopt2)) != STD_OK)
    {
	fprintf(stderr, "%s: eth_ioc_getopt failed: %s\n",
					argv[0], tcpip_why(err));
	exit(4);
    }

    if (ethopt1.nweo_flags != ethopt2.nweo_flags ||
	ethopt1.nweo_type != ethopt2.nweo_type ||
	comp_eaddr(&ethopt1.nweo_rem, &ethopt2.nweo_rem) == 0 ||
	comp_eaddr(&ethopt1.nweo_multi, &ethopt2.nweo_multi) == 0)
    {
	fprintf(stderr,
	    "%s: the results of getopt are different from the settings made\n",
	    argv[0]);
	fprintf(stderr, "flags set = %x   flags get = %x\n",
				ethopt1.nweo_flags, ethopt2.nweo_flags);
	fprintf(stderr, " type set = %x    type get = %x\n",
				ethopt1.nweo_type, ethopt2.nweo_type);
	exit(5);
    }

    /* Now we need a test of tcpip_read and tcpip_write */
    /* XXX - some other time */

    exit(0);
}
