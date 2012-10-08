/*	@(#)sp_mask.c	1.2	94/04/07 10:19:14 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** sp_mask
**	Looks in the environment for the user's default capability masks
**	and returns them in maskar.  maskar has space for nmasks entries
**	in it.  The value of the function is the minimum of nmasks and the
**	number of the entries in the environment variable.  If there is no
**	environment variable then all nmasks entries are set to 0xFF
*/

#include "amoeba.h"
#include "capset.h"
#include "soap/soap.h"
#include "stdlib.h"

int
sp_mask(nmasks, maskar)
int nmasks;	/* in/out: initially the length of maskar */
long *maskar;	/* out: the value of the mask in the environment */
{
	register i;
	char *mask,*p;
	long maskval;

	if (nmasks > SP_MAXCOLUMNS)
		nmasks = SP_MAXCOLUMNS;

	if ((mask = getenv("SPMASK")) == 0) {
		for (i = 0; i < nmasks; i++)
			maskar[i] = 0xFF;
	} else {
		i = 0;
		for (;;) {
			maskval = strtol(mask, &p, 0);
			if (p == mask)
				break;
			if (i >= nmasks)
				break;
			maskar[i++] = maskval;
			if (*p != ':')
				break;
			mask = p+1;
		}
	}
	return i; /* number of columns filled in */
}
