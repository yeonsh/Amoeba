/*	@(#)insque.c	1.2	94/04/07 10:38:02 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** insque - inserts new queue element 'elem' immediately after queue element
**	    'pred'.
*/

#include "qelem.h"

insque(elem, pred)
register struct qelem *elem;
register struct qelem *pred;
{
    elem->q_back = pred;
    elem->q_forw = pred->q_forw;
    if (pred->q_forw)
	pred->q_forw->q_back = elem;
    pred->q_forw = elem;
}
