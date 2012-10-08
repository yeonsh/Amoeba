/*	@(#)rpc.h	1.1	96/02/27 10:33:45 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MODULE_RPC_H__
#define __MODULE_RPC_H__

#include "amoeba.h"
#include "protocols/flip.h"
#include "_ARGS.h"

#define	rpc_getreq	_rpc_getreq
#define	rpc_putrep	_rpc_putrep
#define	rpc_trans	_rpc_trans

long	rpc_getreq _ARGS((header *, bufptr, f_size_t));
long	rpc_putrep _ARGS((header *, bufptr, f_size_t));
long	rpc_trans  _ARGS((header *, bufptr, f_size_t,
						header *, bufptr, f_size_t));

#endif /* __MODULE_RPC_H__ */
