/*	@(#)pool_dir.h	1.2	94/04/06 11:51:19 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef _POOLDIR_H_
#define _POOLDIR_H_

#include "_ARGS.h"

errstat add_pooldir _ARGS((capability *dir_cap, struct pooldir **pp));
void    del_pooldir _ARGS((struct pooldir *p));

struct archdir *find_pool_arch _ARGS((struct pooldir *pool, char *arch));

#endif
