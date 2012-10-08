/*	@(#)eh_main.c	1.1	91/11/25 13:09:10 */
/*
eh_main.c
*/

#include <stdlib.h>

#include "nw_task.h"
#include "amoeba_incl.h"

#include "generic/buf.h"
#include "generic/clock.h"
#include "generic/eth.h"
#include "generic/sr.h"

#include "am_sr.h"

PUBLIC mutex mu_generic;
PUBLIC  char *prog_name;

void ehinit ARGS(( void ));

PUBLIC void ehinit()
{
	prog_name= "kernel ETHERHOOK";

	mu_init(&mu_generic);
	mu_lock(&mu_generic);

	sr_init_cap_names();
	sr_set_cap_name(ETH_DEV0, "eth");
#if 0
	sr_enable_linger_right();
#endif

#if DEBUG & 256
 { where(); printf("starting bf_init()\n"); }
#endif
	bf_init();
#if DEBUG & 256
 { where(); printf("starting clck_init()\n"); }
#endif
	clck_init();
#if DEBUG & 256
 { where(); printf("starting sr_init()\n"); }
#endif
	sr_init();
#if DEBUG & 256
 { where(); printf("starting eth_init()\n"); }
#endif
	eth_init();
#if DEBUG & 256
 { where(); printf("nw_init done\n"); }
#endif
	mu_unlock(&mu_generic);
}
