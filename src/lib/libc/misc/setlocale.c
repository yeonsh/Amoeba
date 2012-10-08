/*	@(#)setlocale.c	1.2	94/04/07 10:49:39 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * setlocale - set the programs locale
 */

#include	<locale.h>
#include	<string.h>

#ifndef __STDC__
#define const	/**/
#endif

struct lconv _lc;

char *
setlocale(category, locale)
int category;
const char *locale;
{
	if (!locale) return "C";
	if (*locale && strcmp(locale, "C")) return (char *)NULL;
	
	switch(category) {
	case LC_ALL:
	case LC_CTYPE:
	case LC_COLLATE:
	case LC_TIME:
	case LC_NUMERIC:
	case LC_MONETARY:
		return *locale ? (char *)locale : "C";
	default:
		return (char *)NULL;
	}
}
