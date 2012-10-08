/*	@(#)ar.h	1.3	96/02/27 10:32:01 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
module/ar.h

Created:	Jan 27, 1992 by Philip Homburg
*/

#ifndef _MODULE__AR_H_
#define _MODULE__AR_H_

#ifndef _ARGS
#include <_ARGS.h>
#endif

#define	ar_cap		_ar_cap
#define	ar_port		_ar_port
#define	ar_priv		_ar_priv
#define	ar_tocap	_ar_tocap
#define	ar_toport	_ar_toport
#define	ar_topriv	_ar_topriv

char *ar_cap _ARGS(( capability *_cap_p ));
char *ar_port _ARGS(( port *_port_p ));
char *ar_priv _ARGS(( private *_priv_p ));

char *ar_tocap _ARGS(( char *_s, capability *_cap_p ));
char *ar_toport _ARGS(( char *_s, port *_port_p ));
char *ar_topriv _ARGS(( char *_s, private *_priv_p ));

#endif /* _MODULE__AR_H_ */
