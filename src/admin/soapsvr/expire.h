/*	@(#)expire.h	1.2	94/04/06 11:57:41 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef EXPIRE_H
#define EXPIRE_H

#include "_ARGS.h"

int  expire_std_age	_ARGS((header *hdr));
int  expire_age  	_ARGS((header *hdr));
int  expire_dirs	_ARGS((header *hdr, char *inbuf, int n,
			       char *outbuf, int outsize));
void expire_init	_ARGS((void));
void expire_delete_dirs _ARGS((void));

#endif
