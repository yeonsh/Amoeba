/*	@(#)interrupt.c	1.2	94/04/06 09:45:18 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "machdep.h"
#include "openprom.h"
#include "assert.h"
INIT_ASSERT
#include "debug.h"

struct proc_int {
	uint32	pi_pending;
	uint32	pi_clear;
	uint32	pi_set;
};

struct syst_int {
	uint32	si_pending;
	uint32	si_mask;
	uint32	si_clear;
	uint32	si_set;
};

static
struct int_addresses {
	struct	proc_int	*ia_proc;
	struct	syst_int	*ia_syst;
} ia;

static
ob_interrupt(p)
nodelist *p;
{
	int proplen;

	proplen = obp_getattr(p->l_curnode, "address",
						(void *) &ia, sizeof(ia));
	compare (proplen, ==, sizeof(ia));
}

void
init_ob_interrupts()
{

	obp_regnode("name", "interrupt", ob_interrupt);
	printf("pi_pending = %x\n", ia.ia_proc->pi_pending);
	printf("si_pending = %x\n", ia.ia_syst->si_pending);
	printf("si_mask = %x\n", ia.ia_syst->si_mask);
}
