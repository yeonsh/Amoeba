/*	@(#)tcp.c	1.1	96/02/27 10:54:25 */
/*
 * Test the routines of the TCP channel of the TCP/IP server.
 * The initial version is deliberately simplistic.  Somebody can
 * make something interesting later if it is needed.
 *
 *	Author: Gregory J. Sharp, June 1995
 */

#include "amoeba.h"
#include "stderr.h"
#include "module/name.h"
#include "server/ip/tcp_io.h"
#include "server/ip/types.h"
#include "server/ip/gen/in.h"
#include "server/ip/gen/tcp.h"
#include "server/ip/gen/tcp_io.h"

#include "stdio.h"


void
usage(p)
char *	p;
{
    fprintf(stderr, "Usage: %s tcp-cap\n", p);
    exit(1);
}


main(argc, argv)
int	argc;
char *	argv[];
{
    errstat		err;
    capability		tcp_cap;
    capability		chan_cap;
    struct nwio_tcpopt	tcpopt1;
    struct nwio_tcpopt	tcpopt2;
    struct nwio_tcpconf	tcpconf;

    if (argc != 2)
    {
	usage(argv[0]);
	/*NOTREACHED*/
    }
    
    if ((err = name_lookup(argv[1], &tcp_cap)) != STD_OK)
    {
	fprintf(stderr, "%s: lookup of '%s' failed: %s\n",
					argv[0], argv[1], err_why(err));
	exit(2);
    }

    if ((err = tcpip_open(&tcp_cap, &chan_cap)) != STD_OK)
    {
	fprintf(stderr, "%s: tcpip_open failed for '%s': %s\n",
					argv[0], argv[1], tcpip_why(err));
	exit(2);
    }

    /* Before we can do anything we have to do a setconf */
    tcpconf.nwtc_flags = NWTC_EXCL |
			 NWTC_LP_SEL |
			 NWTC_UNSET_RA |
			 NWTC_UNSET_RA;
    if ((err = tcp_ioc_setconf(&chan_cap, &tcpconf)) != STD_OK)
    {
	fprintf(stderr, "%s: tcp_ioc_setconf failed: %s\n",
					argv[0], tcpip_why(err));
	exit(4);
    }

    /* Test setopt and getopt */
    tcpopt1.nwto_flags = NWTO_SND_NOTURG |
			 NWTO_RCV_NOTURG |
			 NWTO_NOTDEL_RST;

    if ((err = tcp_ioc_setopt(&chan_cap, &tcpopt1)) != STD_OK)
    {
	fprintf(stderr, "%s: tcp_ioc_setopt failed: %s\n",
					argv[0], tcpip_why(err));
	exit(4);
    }

    if ((err = tcp_ioc_getopt(&chan_cap, &tcpopt2)) != STD_OK)
    {
	fprintf(stderr, "%s: tcp_ioc_getopt failed: %s\n",
					argv[0], tcpip_why(err));
	exit(4);
    }

    if (tcpopt1.nwto_flags != tcpopt2.nwto_flags)
    {
	fprintf(stderr,
	    "%s: the results of getopt are different from the settings made\n",
	    argv[0]);
	fprintf(stderr, "flags set = %x   flags get = %x\n",
				tcpopt1.nwto_flags, tcpopt2.nwto_flags);
	exit(5);
    }

    /* Test all the other tcp_ioc_* routines */
    /* Test the tcpip_* routines */

    exit(0);
}
