/*	@(#)uniqport.c	1.4	94/04/07 11:18:19 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Uniqport - generates a new random port.
 *	      Since this number is used in check fields in must not be
 *	      predictable.
 *
 * Author: Gregory J. Sharp, March 1992
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/rnd.h"

extern long random();

static int initialised;

void
uniqport(p)
port *p;
{
    int i;
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

    q = (uint16 *) p;
    for (i = 0; i < PORTSIZE; i += sizeof (short))
	*q++ = (uint16) (random() >> 15);
}

void
uniqport_reinit()
{
    /* force reinitialization in the next call to uniqport */
    initialised = 0;
}
