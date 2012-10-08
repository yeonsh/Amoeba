/*	@(#)priv2pub.c	1.3	96/02/27 11:00:09 */
/*
 * Copyright 1995 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** Convert a private port to a public port (get-port to put-port).
*/

#include "amoeba.h"
#include "module/crypt.h"

void
priv2pub(p, q)
port *	p;
port *	q;
{
    one_way(p, q);
}
