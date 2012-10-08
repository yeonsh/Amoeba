/*	@(#)ecvt.c	1.2	94/04/07 10:37:49 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef NOFLOAT
#include	"../../../misc/ext_fmt.h"
#include	"../../../stdio/loc_incl.h"
#include	"_ARGS.h"

void _dbl_ext_cvt  _ARGS((double value, struct EXTEND *e));
char *_ext_str_cvt _ARGS((struct EXTEND *e, int ndigit, int *decpt,
			  int * sign, int ecvtflag));

static char *
cvt(value, ndigit, decpt, sign, ecvtflag)
LONG_DOUBLE value;
int ndigit;
int *decpt;
int *sign;
int ecvtflag;
{
	struct EXTEND e;

	_dbl_ext_cvt(value, &e);
	return _ext_str_cvt(&e, ndigit, decpt, sign, ecvtflag);
}

char *
_ecvt(value, ndigit, decpt, sign)
LONG_DOUBLE value;
int ndigit;
int *decpt;
int *sign;
{
	return cvt(value, ndigit, decpt, sign, 1);
}

char *
_fcvt(value, ndigit, decpt, sign)
LONG_DOUBLE value;
int ndigit;
int *decpt;
int *sign;
{
	return cvt(value, ndigit, decpt, sign, 0);
}

#else
#ifndef lint
static int file_may_not_be_empty;
#endif
#endif	/* NOFLOAT */
