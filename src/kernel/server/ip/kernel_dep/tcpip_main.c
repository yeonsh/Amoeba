/*	@(#)tcpip_main.c	1.3	96/02/27 14:20:18 */
/*
tcpip_main.c
*/

#include <stdlib.h>

#include "inet.h"
#include "amoeba_incl.h"
#include "generic/type.h"

#include "generic/arp.h"
#include "generic/assert.h"
#include "generic/buf.h"
#include "generic/clock.h"
#include "generic/eth.h"
#include "generic/ip.h"
#include "generic/ipr.h"
#include "generic/tcp.h"
#include "generic/sr.h"
#include "generic/udp.h"

#include "conf_gw.h"
#include "am_sr.h"

INIT_PANIC();

mutex mu_generic;
char *prog_name;

int killer_inet = 0; /* see tcp_send.c */

static void tcpip_dirinit ARGS(( void ));
static void tcpip_chmod ARGS(( void ));
static void add_default_gws ARGS(( void ));

void tcpipinit _ARGS(( void ));

void tcpipinit()
{
	prog_name= "kernel TCP/IP";

	mu_init(&mu_generic);
	mu_lock(&mu_generic);

	tcpip_dirinit();

	sr_init_cap_names();
	sr_set_cap_name(ETH_DEV0, "eth");
	sr_set_cap_name(IP_DEV0, "ip");
	sr_set_cap_name(TCP_DEV0, "tcp");
	sr_set_cap_name(UDP_DEV0, "udp");
#if 0
	sr_enable_linger_right();
#endif

	bf_init();
	clck_init();
	sr_init();
	eth_init();
	arp_init();
	ip_init();
	tcp_init();
	udp_init();
	add_default_gws();

	mu_unlock(&mu_generic);
	tcpip_chmod();
}

PRIVATE void tcpip_dirinit()
{
	extern capset _sp_rootdir;

	static char *colnames[] = { "owner", "group", "other", 0 };
	static long cols[NROOTCOLUMNS] = {0xFF & ~(SP_DELRGT)};

	capset psdir;

	/*
	** Create an internal soap directory and append it to the root.
	*/
	if (sp_create(&_sp_rootdir, colnames, &psdir) != STD_OK)
		ip_panic(( "tcpip_dirinit: can't create directory" ));
	if (sp_append(&_sp_rootdir, TCPIP_DIRNAME, &psdir, NROOTCOLUMNS,
		cols) != STD_OK)
		ip_panic(( "tcpip_dirinit: can't append directory" ));
}

PRIVATE void tcpip_chmod()
{
	static long cols[NROOTCOLUMNS] = {0xFF & ~(SP_DELRGT | SP_MODRGT)};

	if (dirchmod(TCPIP_DIRNAME, NROOTCOLUMNS, cols))
		ip_panic(( "tcpip_chmod: can't chmod directory" ));
}

PRIVATE void add_default_gws()
{
	int i, err;

	for (i= 0; default_wg_list[i].ipaddr[0]; i++)
	{
		err = ipr_add_oroute(0, (ipaddr_t)0, (ipaddr_t)0,
			*(ipaddr_t *)default_wg_list[i].ipaddr,
			(time_t)0, 1, TRUE, (i32_t) 0, (oroute_t **) NULL);
		assert(err == NW_OK);
	}
}
