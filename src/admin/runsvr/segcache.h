/*	@(#)segcache.h	1.2	94/04/06 11:52:20 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef _SEG_CACHE_H_
#define _SEG_CACHE_H_

#include "_ARGS.h"

void	   setupsegcache _ARGS((struct hostinfo *p));
segment_d *whichseg	 _ARGS((process_d *pd));
long	   checksegbonus _ARGS((struct hostinfo *p, segment_d *sd));
void	   useseg	 _ARGS((struct hostinfo *p, segment_d *sd));

#endif
