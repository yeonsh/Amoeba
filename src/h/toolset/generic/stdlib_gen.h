/*	@(#)stdlib_gen.h	1.4	94/04/06 17:21:18 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __STDLIB_GEN_H__
#define __STDLIB_GEN_H__
/* generic part of compiler-specific stdlib.h */

#define EXIT_FAILURE    1
#define EXIT_SUCCESS    0
#define RAND_MAX        32767
#define MB_CUR_MAX      1

typedef struct { int quot, rem; } div_t;
typedef struct { long quot, rem; } ldiv_t;

#if defined(__STDC__) && defined(__ACK)
#define NULL            ((void *)0)
#else
#ifndef NULL
#define NULL       0
#endif
#endif /* __STDC__ */

#ifndef _VOIDSTAR
#define _VOIDSTAR       void *
#endif

#include "../../_ARGS.h"

_VOIDSTAR bsearch _ARGS((const void *_key, const void *_base,
			 size_t _nmemb, size_t _size,
			 int (*_compar) _ARGS((const void *, const void *)) ));
_VOIDSTAR calloc  _ARGS((size_t _nmemb, size_t _size));
_VOIDSTAR malloc  _ARGS((size_t _size));
_VOIDSTAR realloc _ARGS((void *_ptr, size_t _size));

void   abort	_ARGS((void));
int    abs	_ARGS((int _j));
int    atexit	_ARGS((void (*_func) _ARGS((void)) ));
double atof	_ARGS((const char *_nptr));
int    atoi	_ARGS((const char *_nptr));
long   atol	_ARGS((const char *_nptr));
div_t  div	_ARGS((int _numer, int _denom));
void   exit	_ARGS((int _status));
void   free	_ARGS((void *_ptr));
char  *getenv	_ARGS((const char *_name));
long   labs	_ARGS((long _j));
ldiv_t ldiv	_ARGS((long _numer, long _denom));
int    mblen	_ARGS((const char *_s, size_t _n));
size_t mbstowcs	_ARGS((wchar_t *_pwcs, const char *_s, size_t _n));
int    mbtowc	_ARGS((wchar_t *_pwc, const char *_s, size_t _n));
void   qsort	_ARGS((void *_base, size_t _nmemb, size_t _size,
		       int (*_compar) _ARGS((const void *, const void *)) ));
int    rand	_ARGS((void));
void   srand	_ARGS((unsigned int _seed));
double strtod	_ARGS((const char *_nptr, char **_endptr));
long   strtol	_ARGS((const char *_nptr, char **_endptr, int _base));
unsigned long int
       strtoul	_ARGS((const char *_nptr, char **_endptr, int _base));
int    system	_ARGS((const char *_string));
size_t wcstombs _ARGS((char *_s, const wchar_t *_pwcs, size_t _n));
int    wctomb	_ARGS((char *_s, wchar_t _wchar));

#endif /* __STDLIB_GEN_H__ */
