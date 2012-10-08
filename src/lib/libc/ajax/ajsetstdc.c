/*	@(#)ajsetstdc.c	1.3	94/04/07 10:23:55 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Set STDIN, STDOUT, STDERR in environment to new capability */

#include "ajax.h"

void
_ajax_setstdcap(i, newcap)
	int i;
	capability *newcap;
{
	if (i >= 0 && i < 3) {
		static char *namelist[] = {"STDIN", "STDOUT", "STDERR"};
		capability *cap = getcap(namelist[i]);
		if (cap != NULL) {
			if (newcap == NULL) {
				static capability nullcap;
				newcap = &nullcap;
			}
			*cap = *newcap;
		}
	}
}
