/*	@(#)main.c	1.1	91/11/25 13:15:26 */
/*
main.c

Created Sept 30, 1991 by Philip Homburg
*/

#include <stdlib.h>

#include "nw_task.h"
#include "amoeba_incl.h"

#include "generic/arp.h"
#include "generic/buf.h"
#include "generic/clock.h"
#include "generic/eth.h"
#include "generic/ip.h"
#include "generic/sr.h"
#include "generic/tcp.h"
#include "generic/type.h"
#include "generic/udp.h"

#include "am_eth.h"
#include "am_sr.h"

mutex mu_generic;
char *prog_name;

int main ARGS(( int argc, char *argv[] ));
static void usage ARGS(( void ));

int main(argc, argv)
int argc;
char *argv[];
{
	int ac;
	char **av, *end;
	int minor;

	printf("Just started.\n");
	prog_name= argv[0];

	mu_init(&mu_generic);
	mu_lock(&mu_generic);

	eth_init_ehook_names();
	sr_init_cap_names();
	for (ac= argc-1, av= argv+1; ac; ac--, av++)
	{
		if (av[0][0]== '-')
		{
			switch (av[0][1])
			{
			case 'e':
				minor= strtol(av[0]+2, &end, 0);
				if (*end)
					usage();
				ac--;
				av++;
				if (!ac)
					usage();
				eth_set_ehook_name(minor, *av);
				break;
			case 'c':
				minor= strtol(av[0]+2, &end, 0);
				if (*end)
					usage();
				ac--;
				av++;
				if (!ac)
					usage();
				sr_set_cap_name(minor, *av);
				break;
			case 'l':
				if (av[0][2] != '\0')
					usage();
				sr_enable_linger_right();
				break;
			default:
				usage();
			}
		}
		else
			usage();
	}
#if DEBUG
 { where(); printf("starting bf_init()\n"); }
#endif
	bf_init();
#if DEBUG
 { where(); printf("starting clck_init()\n"); }
#endif
	clck_init();
#if DEBUG
 { where(); printf("starting sr_init()\n"); }
#endif
	sr_init();
#if DEBUG
 { where(); printf("starting eth_init()\n"); }
#endif
	eth_init();
#if DEBUG
 { where(); printf("starting arp_init()\n"); }
#endif
	arp_init();
#if DEBUG
 { where(); printf("starting ip_init()\n"); }
#endif
	ip_init();
#if DEBUG
 { where(); printf("starting tcp_init()\n"); }
#endif
	tcp_init();
#if DEBUG
 { where(); printf("starting udp_init()\n"); }
#endif
	udp_init();
#if DEBUG
 { where(); printf("nw_init done\n"); }
 { where(); printf("doing thread_exit\n"); }
#endif
	mu_unlock(&mu_generic);
	thread_exit();
}

static void usage()
{
	printf("USAGE: %s [-e<port_nr> ehook_cap] [-c<minor> tcpip_cap] ...\n", prog_name);
	exit(1);
}
