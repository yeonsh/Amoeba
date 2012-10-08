/*	@(#)pd_size.c	1.3	96/02/27 11:03:36 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** pd_size
**	Return the size of the given process descriptor:
**	Find the last task descriptor by going through them all until the end
**	and then subtract the start of the process descriptor from the end to
**	get the size.
**	If the process descriptor is invalid (bad thread size) we return -1.
*/

#include "amoeba.h"
#include "module/proc.h"

int
pd_size(pd)
process_d *	pd;
{
    register thread_d *	td;
    register int	i;

    for (td = PD_TD(pd), i = pd->pd_nthread; --i >= 0; td = TD_NEXT(td))
	if (td->td_len < sizeof (thread_d) || td->td_len > MAX_THREAD_SIZE)
	    return -1;
    return (char *) td - (char *) pd;
}
