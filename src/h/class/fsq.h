/*	@(#)fsq.h	1.2	94/04/06 15:49:20 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __FSQ_H__
#define __FSQ_H__

/* Inherited classes: */
#include "bullet_rgts.h"

#include <_ARGS.h>

/* Operators: */
errstat fsq_read _ARGS((capability *_cap, 
    /* in */	long int position, 
    /* in */	int size, 
    /* out */	char *data, 
    /* out */	int *moredata, 
    /* out */	int *result));

errstat fsq_write _ARGS((capability *_cap, 
    /* in */	long int position, 
    /* in */	int size, 
    /* in */	char *data, 
    /* out */	int *written));

#endif /* __FSQ_H__ */
