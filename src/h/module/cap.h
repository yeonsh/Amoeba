/*	@(#)cap.h	1.1	96/02/27 10:32:11 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef _MODULE__CAP_H_
#define _MODULE__CAP_H_

#include "_ARGS.h"

#if 0 /* not yet */
#define	cap_cmp		_cap_cmp
#define cc_restrict	_cc_restrict
#define cc_init		_cc_init
#endif

int cap_cmp	_ARGS((capability *_cap1, capability *_cap2));

int cc_restrict _ARGS((capability *_cap, rights_bits _mask,
		       capability *_new, int (*_restrict)() ));
int cc_init	_ARGS((int _ncache));

#endif /* _MODULE__CAP_H_ */
