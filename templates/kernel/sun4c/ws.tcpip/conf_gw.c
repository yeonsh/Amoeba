/*	@(#)conf_gw.c	1.1	96/02/16 15:50:50 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
conf_gw.c

Created Sept 25, 1991 by Philip Homburg
*/

#include "inet.h"
#include "conf_gw.h"

gw_list_t default_wg_list[]=
{
	{ { 0, 0, 0, 0 }, 0 },		/* end of list */
};
