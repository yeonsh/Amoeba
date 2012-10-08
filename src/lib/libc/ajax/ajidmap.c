/*	@(#)ajidmap.c	1.2	94/04/07 10:23:06 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Return an array initialized with the numbers 0 .. NOFILE-1 */

#include "ajax.h"
#include "fdstuff.h"

int *
_ajax_idmapping()
{
	static int idmapping[NOFILE] =
		{0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
				10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
#if NOFILE > 20
	if (idmapping[20] == 0) {
		int i;
		
		for (i = 20; i < NOFILE; ++i)
			idmapping[i] = i;
	}
#endif
	
	return idmapping;
}
