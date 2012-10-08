/*	@(#)prv.h	1.3	96/02/27 10:33:25 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MODULE_PRV_H__
#define __MODULE_PRV_H__

/* Against the day we make rights 32 bits */
#define	PRV_ALL_RIGHTS	((rights_bits) 0xFF)

#define	prv_number	_prv_number
#define	prv_decode	_prv_decode
#define	prv_encode	_prv_encode

objnum	prv_number _ARGS((private *));
int	prv_decode _ARGS((private *, rights_bits *, port *));
int	prv_encode _ARGS((private *, objnum, rights_bits, port *));

#endif /* __MODULE_PRV_H__ */
