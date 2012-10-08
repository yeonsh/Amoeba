/*	@(#)inet.h	1.1	96/02/27 14:20:00 */
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

#define port am_port_t	/* Redefine these types to something reasonable. */
#define header am_header_t
#define bufsize am_bufsize_t

#include <amoeba.h>
#include <cmdreg.h>
#include <stderr.h>
#include <sys/proto.h>

#undef port
#undef header
#undef bufsize

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
#include "tcpip_errno.h"
#include "ioctl.h"
#include "const.h"
#include "inet_config.h"

/* Globally rename some routine to avoid collisions */
#define eth_init tcpip_eth_init
#define eth_send tcpip_eth_send

PUBLIC char *prog_name;

#define TCPIP_DIRNAME "ip"

void abort ARGS(( void ));
void helloworld ARGS(( void ));

#define INIT_PANIC() static char *ip_panic_warning_file= __FILE__

#define ip_panic(print_list)  \
	( \
		printf("panic at %s, %d: ", ip_panic_warning_file, __LINE__), \
		printf print_list, \
		printf("\n"), \
		stacktrace(), \
		helloworld(), \
		abort(), \
		0 \
	)

void stacktrace ARGS(( void ));

#ifdef NO_OUTPUT_ON_CONSOLE
#define console_enable(x) 0
#endif

#define ip_warning(print_list) \
	( \
		console_enable(0), \
		printf("warning at %s, %d: ", ip_panic_warning_file, \
			__LINE__), \
		printf print_list, \
		printf("\n"), \
		/* stacktrace(), */ \
		console_enable(1), \
		0 \
	)

#define DBLOCK(level, code) \
	do { if ((level) & DEBUG) { where(); code; } } while(0)
#define DIFBLOCK(level, condition, code) \
	do { if (((level) & DEBUG) && (condition)) \
		{ where(); code; } } while(0)

