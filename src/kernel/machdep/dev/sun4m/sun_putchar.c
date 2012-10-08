/*	@(#)sun_putchar.c	1.2	94/04/06 09:39:20 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "global.h"
#include "machdep.h"
#include "openprom.h"
#include "assert.h"
INIT_ASSERT

/*
 * sun_putchar() (ENTRY POINT) -- place a character on the given output device.
 */


int
sun_putchar(c)
char c;		      	/* Character to put */
{
    if (c == '\r')
	return 0;		/* Ignore CR half of CRLF */

    assert(prom_devp->op_romvec_version >= 2);

    if (c == '\n')
	(*prom_devp->op_write)( *prom_devp->op_stdout, "\r\n", 2 );
    else
	(*prom_devp->op_write)( *prom_devp->op_stdout, &c, 1 );

    return 0;
}
