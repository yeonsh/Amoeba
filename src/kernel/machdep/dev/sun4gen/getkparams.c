/*	@(#)getkparams.c	1.1	96/02/27 13:54:36 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef NO_KERNEL_SECURITY

#include "amoeba.h"
#include "kparam.h"
#include "kpar_offset.h"

#define	CHKDATA_BUFSZ	(sizeof (struct prom_data))

struct prom_data
{
    uint8	nulls[CHKDATA_NULLS];
    uint32	magic;
    port	port_data;
    port	chkfield;
    port	extra1; /* Room for growth in the future */
    port	extra2; /* Room for growth in the future */
    uint32	chksum;
};

union p_data
{
    uint8		buf[CHKDATA_BUFSZ];
    struct prom_data	d;
};


/*
 * getkparams_from_prom:
 *	Looks in the eeprom/nvram to find some caps hidden in the OEM logo.
 *	Returns 1 if it succeeds and 0 otherwise.
 */

int
getkparams_from_prom(kp)
struct kparam *kp;
{
    union p_data	p;
    int i;

    if (eeprom_read(CHKDATA_OFFSET, p.buf, CHKDATA_BUFSZ) == 0)
	return 0;

    /* The first CHKDATA_NULLS bytes must be NULLs */
    for (i = 0; i < CHKDATA_NULLS; i++)
    {
	if (p.d.nulls[i] != '0')
	    return 0;
    }
    if (p.d.magic != CHKDATA_MAGIC)
	return 0;

    kp->kp_port = p.d.port_data;
    kp->kp_chkf = p.d.chkfield;

    /* We could look at the checksum but we'll do that later */
    return 1;
}

#endif /* NO_KERNEL_SECURITY */
