/*	@(#)rawflip.h	1.4	96/02/27 10:33:31 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MODULE_RAWFLIP_H__
#define __MODULE_RAWFLIP_H__

#include "_ARGS.h"
#include "protocols/flip.h"
#include "protocols/rawflip.h"
#include "module/fliprand.h"

#define	flip_init		_flip_init
#define	flip_end		_flip_end
#define	flip_register		_flip_register
#define	flip_unregister		_flip_unregister
#define	flip_broadcast		_flip_broadcast
#define	flip_unicast		_flip_unicast
#define	flip_multicast		_flip_multicast


int flip_init _ARGS(( long ident,
		    void (*receive) _ARGS((long ident, char *pkt, adr_p dstaddr,
					   adr_p srcaddr, f_msgcnt_t messid,
					   f_size_t offset, f_size_t length,
					   f_size_t total, int flag)),
		    void (*notdeliver) _ARGS(( long ident, char *pkt,
					      adr_p dstaddr, f_msgcnt_t messid,
					      f_size_t offset, f_size_t length,
					      f_size_t total, int flag )) ));
int flip_end _ARGS(( int ifno ));
int flip_register _ARGS(( int ifno, adr_p addr ));
int flip_unregister _ARGS(( int ifno, int ep ));
int flip_broadcast _ARGS(( int ifno, char * pkt, int ep,
				    f_size_t len, f_hopcnt_t hpcnt ));
int flip_unicast _ARGS(( int ifno, char * pkt, int flags, adr_p dst,
				    int ep, f_size_t length, interval ltime ));
int flip_multicast _ARGS(( int ifno, char * pkt, int flags, adr_p dst, int ep,
				    f_size_t length, int n, interval ltime ));

/*
 * Arguably this guy should be invisible to the outside world but it must
 * go somewhere related to rawflip.
 */

#define	rawflip		_rawflip

int rawflip _ARGS(( struct rawflip_parms * p ));

#endif /* __MODULE_RAWFLIP_H__ */
