/*	@(#)ip_debug.c	1.2	94/04/07 10:34:23 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
ip_debug.c

Created Oct 18, 1991 by Philip Homburg
*/

#include <amoeba.h>
#include <server/ip/debug.h>

mutex ip_debug_mutex;
