/*	@(#)math.h	1.3	96/02/27 10:34:31 */
#ifndef _MATH_H
#define _MATH_H

/*
 * (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#ifdef	__STDC__
double __huge_val(void);        /* may be infinity */
#define HUGE_VAL        (__huge_val())
#else
#if defined(vax) || defined(pdp)
#define HUGE_VAL	1.701411834604692293e+38
#else
#define HUGE_VAL	4.494232837155780788e+307
#endif
#define HUGE		HUGE_VAL
#endif /* __STDC__ */

#include "_ARGS.h"

int     __IsNan	_ARGS((double _d));      /* test for Not A Number */

double  acos	_ARGS((double _x));
double  asin	_ARGS((double _x));
double  atan	_ARGS((double _x));
double  atan2	_ARGS((double _y, double _x));

double  cos	_ARGS((double _x));
double  sin	_ARGS((double _x));
double  tan	_ARGS((double _x));

double  cosh	_ARGS((double _x));
double  sinh	_ARGS((double _x));
double  tanh	_ARGS((double _x));

double  exp	_ARGS((double _x));
double  log	_ARGS((double _x));
double  log10	_ARGS((double _x));

double  sqrt	_ARGS((double _x));
double  ceil	_ARGS((double _x));
double  fabs	_ARGS((double _x));
double  floor	_ARGS((double _x));

double  pow	_ARGS((double _x, double _y));

double  frexp	_ARGS((double _x, int *_exp));
double  ldexp	_ARGS((double _x, int _exp));
double  modf	_ARGS((double _x, double *_iptr));
double  fmod	_ARGS((double _x, double _y));

#ifdef _XOPEN_SOURCE
double	gamma	_ARGS((double _x));
double	hypot	_ARGS((double _x, double _y));
double	j0	_ARGS((double _x));
double	j1	_ARGS((double _x));
double	jn	_ARGS((int _n, double _x));
double	y0	_ARGS((double _x));
double	y1	_ARGS((double _x));
double	yn	_ARGS((int _n, double _x));
#endif /* _XOPEN_SOURCE */

#endif /* _MATH_H */
