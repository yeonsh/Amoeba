/*	@(#)flip_random.c	1.3	96/02/27 11:01:25 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * flip_random - generates a new random FLIP port.
 *
 * Author: Gregory J. Sharp, Jan 1994
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "protocols/flip.h"
#include "module/rnd.h"
#include "module/fliprand.h"

extern long random();

static int initialised;

void
flip_random(adr)
adr_p adr;
{
    uint16 * q;

    if (!initialised)
    {
	errstat err;
	unsigned long seed;

	err = rnd_getrandom((char *) &seed, sizeof seed);
	if (err != STD_OK)
	{
	    err = rnd_getrandom((char *) &seed, sizeof seed);
	    if (err != STD_OK)
		_ajax_putnum("uniqport: rnd_getrandom error", err);
	}
	srandom(seed);
	initialised = 1;
    }

    q = (uint16 *) adr;
    *q++ = (uint16) (random() >> 15);
    *q++ = (uint16) (random() >> 15);
    *q++ = (uint16) (random() >> 15);
    *q   = (uint16) (random() >> 15);
    adr->a_space = 0;
}

void
flip_random_reinit()
{
    /* force reinitialization in the next call to uniqport */
    initialised = 0;
}
