/*	@(#)strmisc.h	1.4	96/02/27 10:34:01 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
module/strmisc.h

Created Oct 16, 1991 by Philip Homburg

Miscellaneous string routines
*/

#ifndef __MODULE__STRMISC_H__
#define __MODULE__STRMISC_H__

#include "_ARGS.h"

#define	strcasecmp	_strcasecmp
#define	bprintf		_bprintf

int strcasecmp _ARGS(( const char *s1, const char *s2 ));
char *bprintf _ARGS((char *begin, char *end, char *fmt, ...));

#endif /* __MODULE__STRMISC_H__ */
