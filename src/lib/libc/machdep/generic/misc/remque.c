/*	@(#)remque.c	1.2	94/04/07 10:38:15 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** remque - delete queue entry 'elem' from the doubly linked list.
*/

#include "qelem.h"

remque(elem)
register struct qelem *elem;
{
    if (elem->q_back)
	elem->q_back->q_forw = elem->q_forw;
    if (elem->q_forw)
	elem->q_forw->q_back = elem->q_back;
}
