/*	@(#)sun_putchar.c	1.2	94/04/06 09:33:11 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * FILE: sun_putchar.c -- deal with output to the console via the PROM
 *
 * Author: Philip Shafer <phil@procyon.ics.Hawaii.Edu>
 *	   (Univerity of Hawai'i, Manoa) November 1990
 * Modified: Gregory J. Sharp, July 1991
 *	The sun_putchar routine started life in console.c as putchar but
 *	we need to be able to support ttys on the serial ports and the
 *	console possibly being on other ports.  We only use this when we
 *	want to write on the bitmap.
 */

#include "amoeba.h"
#include "global.h"
#include "mmuconst.h"
#include "machdep.h"
#include "openprom.h"
#include "map.h"
#include "sys/proto.h"
#include "mmuproto.h"


extern int setints();

/*
 * sun_putchar() (ENTRY POINT) -- place a character on the given output device.
 */


int
sun_putchar(c)
char c;		      	/* Character to put */
{
    int oldints, ctx;

    if (c == '\r')
	return 0;		/* Ignore CR half of CRLF */

    oldints = sparc_disable();
    ctx = mmu_getctx();
    mmu_setctx( PROM_CTX );

    if (prom_devp->op_romvec_version <= 1)
    {
	if ( c == '\n' )
	    (*prom_devp->v1_printf)( "\r\n" );
	else
	    (*prom_devp->v1_printf)( "%c", c );
    }
    else
    {
	if ( c == '\n' )
	    (*prom_devp->op_write)( *prom_devp->op_stdout, "\r\n", 2 );
	else
	    (*prom_devp->op_write)( *prom_devp->op_stdout, &c, 1 );
    }

    mmu_setctx( ctx );
    (void) setints( oldints );
    return 0;
}
