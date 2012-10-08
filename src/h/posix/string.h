/*	@(#)string.h	1.3	94/04/06 16:54:25 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __STRING_H__
#define __STRING_H__

#include "_ARGS.h"

#ifdef __STDC__
#define NULL	((void *)0)
#else
#ifndef NULL
#define NULL	0
#endif
#endif /* __STDC__ */

#if     !defined(_SIZE_T)
#define _SIZE_T
typedef unsigned int    size_t;         /* type returned by sizeof */
#endif /* _SIZE_T */

#ifndef _VOIDSTAR
#define _VOIDSTAR	void *
#endif

int       memcmp   _ARGS((const void *_s1, const void *_s2, size_t _n));
_VOIDSTAR memchr   _ARGS((const void *_s, int _c, size_t _n));
_VOIDSTAR memcpy   _ARGS((void *_s1, const void *_s2, size_t _n));
_VOIDSTAR memmove  _ARGS((void *_s1, const void *_s2, size_t _n));
_VOIDSTAR memset   _ARGS((void *_s, int _c, size_t _n));

char     *strcat   _ARGS((char *_s1, const char *_s2));
char	 *strchr   _ARGS((const char *_s, int _c));
int	  strcmp   _ARGS((const char *_s1, const char *_s2));
int	  strcoll  _ARGS((const char *_s1, const char *_s2));
char     *strcpy   _ARGS((char *_s1, const char *_s2));
size_t	  strcspn  _ARGS((const char *_s1, const char *_s2));
char	 *strerror _ARGS((int _errnum));
size_t	  strlen   _ARGS((const char *_s));
char     *strncat  _ARGS((char *_s1, const char *_s2, size_t _n));
int	  strncmp  _ARGS((const char *_s1, const char *_s2, size_t _n));
char	 *strncpy  _ARGS((char *_s1, const char *_s2, size_t _n));
char	 *strpbrk  _ARGS((const char *_s1, const char *_s2));
char	 *strrchr  _ARGS((const char *_s, int _c));
size_t	  strspn   _ARGS((const char *_s1, const char *_s2));
char	 *strstr   _ARGS((const char *_s1, const char *_s2));
char	 *strtok   _ARGS((char *_s1, const char *_s2));
size_t	  strxfrm  _ARGS((char *_s1, const char *_s2, size_t _n));

#endif /* __STRING_H__ */
