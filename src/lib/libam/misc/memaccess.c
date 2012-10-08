/*	@(#)memaccess.c	1.3	94/04/07 10:08:17 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "memaccess.h"

#if !defined(__STDC__) && !defined(lint)

/*
 * Routines used to protect against "too smart" optimisations when doing
 * memory mapped I/O.  In ANSI C, the "volatile" type qualifier should
 * be used instead.
 */

long mem_get_long(p) long *p; { return *p; }
void mem_put_long(p, val) long *p; long val; { *p = val; }
void mem_OR_long (p, val) long *p; long val; { *p |= val; }
void mem_AND_long(p, val) long *p; long val; { *p &= val; }

short mem_get_short(p) short *p; { return *p; }
void mem_put_short(p, val) short *p; short val; { *p = val; }
void mem_OR_short (p, val) short *p; short val; { *p |= val; }
void mem_AND_short(p, val) short *p; short val; { *p &= val; }

char mem_get_byte(p) char *p; { return *p; }
void mem_put_byte(p, val) char *p; char val; { *p = val; }
void mem_OR_byte (p, val) char *p; char val; { *p |= val; }
void mem_AND_byte(p, val) char *p; char val; { *p &= val; }

void mem_sync() {}

#else
#ifndef lint
static int nonempty;
#endif
#endif
