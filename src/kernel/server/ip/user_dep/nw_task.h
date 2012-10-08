/*	@(#)nw_task.h	1.2	93/06/09 16:37:07 */
/* This is the master header for the network task.  It includes some
 * other files and defines the principal constants. */

/* The following are so basic, all the *.c files get them automatically.
 */

/* ansi c includes */

#include <time.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

/* standard amoeba includes */

#include <cmdreg.h>
#include <stderr.h>

/* standard tcpip includes */

#define TCPIP_SERVER

#include <server/ip/tcpip.h>
#include <server/ip/types.h>
#include <server/ip/hton.h>
#include <server/ip/gen/ether.h>
#include <server/ip/gen/eth_hdr.h>
#include <server/ip/gen/eth_io.h>
#include <server/ip/gen/in.h>
#include <server/ip/gen/ip_io.h>
#include <server/ip/gen/ip_hdr.h>
#include <server/ip/gen/icmp.h>
#include <server/ip/gen/icmp_hdr.h>
#include <server/ip/gen/route.h>
#include <server/ip/gen/tcp.h>
#include <server/ip/gen/tcp_io.h>
#include <server/ip/gen/tcp_hdr.h>
#include <server/ip/gen/udp.h>
#include <server/ip/gen/udp_hdr.h>
#include <server/ip/gen/udp_io.h>
#include <server/ip/gen/oneCsum.h>

/* includes for amoeba dependend parts of the implementation */

#include "args.h"
#include "errno.h"
#include "ioctl.h"
#include "const.h"

#define eth_init tcpip_eth_init

PUBLIC char *prog_name;

/* for speed... */
void bcopy _ARGS(( void *b1, void *b2, int length ));
#define memmove(dst, src, siz)	bcopy(src, dst, siz)
#define memcpy(dst, src, siz)	bcopy(src, dst, siz)

void abort ARGS(( void ));

#define panic(print_list)  \
	( \
		printf("panic at %s, %d: ", __FILE__, __LINE__), \
		printf print_list, \
		printf("\n"), \
		abort(), \
		0 \
	)

#define warning(print_list) \
	( \
		printf("warning at %s, %d: ", __FILE__, __LINE__), \
		printf print_list, \
		printf("\n"), \
		0 \
	)

