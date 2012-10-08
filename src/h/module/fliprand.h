/*	@(#)fliprand.h	1.1	96/02/27 10:32:46 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MODULE_FLIPRAND_H__
#define	__MODULE_FLIPRAND_H__

/* This stuff is separate from rawflip since it is used by the kernel */

#include "_ARGS.h"
#include "protocols/flip.h"
#include "protocols/rawflip.h"

#define	flip_oneway		_flip_oneway
#define	flip_random		_flip_random
#define	flip_random_reinit	_flip_random_reinit

void flip_oneway _ARGS(( adr_p priv, adr_p pub ));
void flip_random _ARGS(( adr_p address ));
void flip_random_reinit _ARGS(( void ));

#endif /* __MODULE_FLIPRAND_H__ */
