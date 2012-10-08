/*	@(#)hugeval.c	1.2	96/02/27 11:13:01 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifdef __STDC__
#include <math.h>

#ifdef IEEE_FP
#include "byteorder.h"

static union
{
    unsigned long	i[2];
    double		d;
} __huge_data;

static int initialised;

#endif /* IEEE_FP */

double
__huge_val()
{
#ifdef IEEE_FP

    if (!initialised)
    {
	initialised = 1;
#ifdef BIG_ENDIAN
	__huge_data.i[0] = 0x7ff00000;
	__huge_data.i[1] = 0;
#else
	__huge_data.i[0] = 0;
	__huge_data.i[1] = 0x7ff00000;
#endif /* BIG_ENDIAN */
    }
    return __huge_data.d;

#else /* IEEE_FP */
    /*
     * We don't really care about non-ieee fp processors but it never hurts
     * to leave something here for those who don't have ieee fp.
     */
    return 1.0e+1000;	/* This will generate a warning */
#endif /* IEEE_FP */
}

#else /* __STDC__ */
#ifndef lint
static int file_may_not_be_empty;
#endif /* lint */
#endif /* __STDC__ */
