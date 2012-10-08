/*	@(#)ar2.c	1.4	96/02/27 11:00:29 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include <amoeba.h>
#include <module/prv.h>
#include <module/ar.h>
#include <string.h>

/*
 *	ar_to*: conversion routines from the output
 *	of the ar_* routines back to their input.
 *	They return NULL on error, and a pointer
 *	to the end of the string on success.
 *
 *	char *ar_toport(char *, port *);
 *	char *ar_topriv(char *, private *);
 *	char *ar_tocap(char *, capability *);
 *
 *		- Siebren, Apr 1990
 */

#ifndef NULL
#define NULL 0
#endif
#ifndef isdigit
#define isdigit(ch)	('0' <= (ch) && (ch) <= '9')
#endif

static int
hex_d(ch)
    char ch;
{
    if (isdigit(ch)) return ch - '0';
    if ('a' <= ch && ch <= 'f') return ch - 'a' + 10;
    if ('A' <= ch && ch <= 'F') return ch - 'A' + 10;
    return -1;
} /* hex_d */

char *
ar_toport(s, p)
    char *s;
    port *p;
{
    int i;
    /* Scan "x:x:x:x:x:x" - checkword */
    for (i = 0; i < PORTSIZE; ++i) {
	int x;
	if (i != 0 && *s++ != ':') return NULL;
	if ((x = hex_d(*s++)) < 0) return NULL;
	p->_portbytes[i] = x;
	if ((x = hex_d(*s)) >= 0) {
	    ++s;
	    p->_portbytes[i] = 16 * p->_portbytes[i] + x;
	}
    }
    return s;
} /* ar_toport */

char *
ar_topriv(s, p)
    char *s;
    private *p;
{
    objnum n;
    int i;
    rights_bits r;
    port c;

    /* Scan "d" */
    n = 0;
    while (isdigit(*s)) n = 10 * n + *s++ - '0';

    /* Scan "(x)" - the rights */
    if (*s++ != '(') return NULL;
    if ((r = hex_d(*s++)) < 0) return NULL;
    if ((i = hex_d(*s)) >= 0) {
	++s;
	r = i + 16 * r;
    }
    if (*s++ != ')') return NULL;

    if (*s++ != '/') return NULL;

    /* And the checkword */
    if ((s = ar_toport(s, &c)) == NULL) return NULL;

    if (prv_encode(p, n, r, &c)) return NULL;
    /* Restore random checkword */
    (void) memmove(&p->prv_random, &c, sizeof(c));
    return s;
} /* ar_topriv */

char *
ar_tocap(s, cap)
    char *s;
    capability *cap;
{
    if ((s = ar_toport(s, &cap->cap_port)) != NULL) {
	if (*s++ != '/') return NULL;
	s = ar_topriv(s, &cap->cap_priv);
    }
    return s;
} /* ar_tocap */
