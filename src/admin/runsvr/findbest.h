/*	@(#)findbest.h	1.2	94/04/06 11:50:49 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef _FINDBEST_H_
#define _FINDBEST_H_

#include "_ARGS.h"

void initpoolparams	_ARGS((struct pooldir *pool));
void initarchparams	_ARGS((struct archdir *arch));
errstat findbest	_ARGS((private *priv, int npd, process_d *pd_list[], 
			       struct hostinfo **ret_host, int *ret_index));
errstat impl_std_getparams
    			_ARGS((header *h, char *buf, int size,
			       int *ret_len, int *nparams));
errstat impl_std_setparams
    			_ARGS((header *h, char *buf, int size, int nparams));

errstat set_pool_param	_ARGS((struct pooldir *p, char *param, char *val));

#endif
