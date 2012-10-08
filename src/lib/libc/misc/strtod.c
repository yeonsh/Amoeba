/*	@(#)strtod.c	1.2	94/04/07 10:50:03 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdlib.h>
#include "ext_fmt.h"
#include "_ARGS.h"

#ifndef __STDC__
#define const /*const*/
#endif

void _str_ext_cvt   _ARGS((const char *s, char **ss, struct EXTEND *e));
double _ext_dbl_cvt _ARGS((struct EXTEND *e));

double
strtod(p, pp)
const char *p;
char **pp;
{
	struct EXTEND e;

	_str_ext_cvt(p, pp, &e);
	return _ext_dbl_cvt(&e);
}
