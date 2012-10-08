/*	@(#)stdlib.h	1.5	96/02/27 10:34:57 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __STDLIB_H__
#define __STDLIB_H__

#define EXIT_FAILURE    1
#define EXIT_SUCCESS    0
#define RAND_MAX        32767
#define MB_CUR_MAX      1

typedef struct { int quot, rem; } div_t;
typedef struct { long quot, rem; } ldiv_t;

#ifdef __STDC__
#define NULL            ((void *)0)
#else
#ifndef NULL
#define NULL       0
#endif
#endif /* __STDC__ */

#if     !defined(_SIZE_T)
#define _SIZE_T
typedef unsigned int    size_t;         /* type returned by sizeof */
#endif  /* _SIZE_T */

#if     !defined(_WCHAR_T)
#define _WCHAR_T
typedef char    wchar_t;
#endif  /* _WCHAR_T */

#ifndef _VOIDSTAR
#define _VOIDSTAR	void *
#endif

#include "_ARGS.h"

_VOIDSTAR bsearch _ARGS((const void *_key, const void *_base,
			 size_t _nmemb, size_t _size,
			 int (*_compar) _ARGS((const void *, const void *)) ));

void   abort	_ARGS((void));
int    abs	_ARGS((int _j));
int    atexit	_ARGS((void (*_func) _ARGS((void)) ));
double atof	_ARGS((const char *_nptr));
int    atoi	_ARGS((const char *_nptr));
long   atol	_ARGS((const char *_nptr));
div_t  div	_ARGS((int _numer, int _denom));
void   exit	_ARGS((int _status));
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

#ifndef __MAL_DEBUG

_VOIDSTAR calloc  _ARGS((size_t _nmemb, size_t _size));
_VOIDSTAR malloc  _ARGS((size_t _size));
_VOIDSTAR realloc _ARGS((void *_ptr, size_t _size));
void      free	  _ARGS((void *_ptr));

#else

_VOIDSTAR _calloc_debug  _ARGS((size_t _nmemb, size_t _size,
			       char *_file, int _line));
_VOIDSTAR _malloc_debug  _ARGS((size_t _size, char *_file, int _line));
_VOIDSTAR _realloc_debug _ARGS((void *_ptr, size_t _size,
			       char *_file, int _line));
void      _free_debug    _ARGS((void *_ptr, char *_file, int _line));

#define calloc(n, s)	_calloc_debug(n, s, __FILE__, __LINE__)
#define malloc(s)	_malloc_debug(s, __FILE__, __LINE__)
#define realloc(p, s)	_realloc_debug(p, s, __FILE__, __LINE__)
#define free(p)		_free_debug(p, __FILE__, __LINE__)

#endif

#endif /* __STDLIB_H__ */
