/*	@(#)flip_conf.c	1.3	96/02/27 14:04:38 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/************************************************************************/
/*    FLIP configuration constants                                      */
/************************************************************************/

#include "amoeba.h"
#include "sys/flip/packet.h"
#include "sys/flip/flip.h"
#ifndef UNIX
#include "map.h"	/* Get possible overrides for parameters */
#endif

/*
 * Supply default values.
 */
#ifdef COLDSTART
#define IF_COLDSTART(a,b)	(a)
#else
#define IF_COLDSTART(a,b)	(b)
#endif

#ifndef FLIP_eth_nmcast
#define FLIP_eth_nmcast		256
#endif
 
#ifndef FLIP_pktsize
#define FLIP_pktsize		(1518 + PKTBEGHDR)
#endif
#ifndef FLIP_npkt
#define FLIP_npkt		IF_COLDSTART(20, 80)
#endif
#ifndef FLIP_maxntw
#define FLIP_maxntw		IF_COLDSTART(1, 5)
#endif
#ifndef FLIP_max_hops
#define FLIP_max_hops		IF_COLDSTART(1, 25)
#endif
 
#ifndef FLIP_nkernel
#define FLIP_nkernel		IF_COLDSTART(10, 200)
#endif

#ifndef FLIP_adr_nadr
#define FLIP_adr_nadr		IF_COLDSTART(50, (2 * FLIP_nkernel))
#endif
#ifndef FLIP_adr_nntw
#define FLIP_adr_nntw		IF_COLDSTART(50, (2 * FLIP_nkernel))
#endif
#ifndef FLIP_adr_nloc
#define FLIP_adr_nloc		IF_COLDSTART(50, (3 * FLIP_nkernel))
#endif
 
#ifndef FLIP_int_maxint
#ifdef UNIX
#define FLIP_int_maxint		16
#else
#define FLIP_int_maxint		200
#endif
#endif
 
#ifndef FLIP_rpc_maxcache
#define FLIP_rpc_maxcache	256
#endif
#ifndef FLIP_rpc_npkt
#define FLIP_rpc_npkt		200
#endif
#ifndef FLIP_rpc_pktsize
#define FLIP_rpc_pktsize	(PKTENDHDR + sizeof(header) + 100)
#endif
 
#ifndef FLIP_ff_pktsize
#define FLIP_ff_pktsize		200
#endif
#ifndef FLIP_ff_maxnloc
#define FLIP_ff_maxnloc		FLIP_nkernel
#endif
#ifndef FLIP_ff_maxnfadr
#define FLIP_ff_maxnfadr	IF_COLDSTART(100, (2 * FLIP_nkernel))
#endif

#ifdef FLIPGRP
#ifndef FLIP_grp_nbarrier
#define FLIP_grp_nbarrier	20
#endif
#ifndef FLIP_grp_npkt
#define FLIP_grp_npkt		80
#endif
#ifndef FLIP_grp_pktsize
#define FLIP_grp_pktsize	(PKTENDHDR + sizeof(header) + 100)
#endif
#ifndef FLIP_grp_maxgrp
#define FLIP_grp_maxgrp		10
#endif
#endif /* FLIPGRP */


/*
 * The configuration constant defs:
 */
uint16 eth_nmcast	= FLIP_eth_nmcast;

uint16 flip_pktsize	= FLIP_pktsize;
uint16 flip_npkt	= FLIP_npkt;
uint16 flip_maxntw	= FLIP_maxntw;
f_hopcnt_t max_hops	= FLIP_max_hops;

uint16 adr_nadr		= FLIP_adr_nadr;
uint16 adr_nntw		= FLIP_adr_nntw;
uint16 adr_nloc		= FLIP_adr_nloc;

uint16 int_maxint	= FLIP_int_maxint;

uint16 rpc_maxcache	= FLIP_rpc_maxcache;
uint16 rpc_npkt		= FLIP_rpc_npkt;
uint16 rpc_pktsize	= FLIP_rpc_pktsize;
uint16 nkernel		= FLIP_nkernel;

uint16 ff_pktsize	= FLIP_ff_pktsize;
uint16 ff_maxnloc	= FLIP_ff_maxnloc;
uint16 ff_maxnfadr	= FLIP_ff_maxnfadr;

#ifdef FLIPGRP
uint16 grp_nbarrier	= FLIP_grp_nbarrier;
uint16 grp_npkt		= FLIP_grp_npkt;
uint16 grp_pktsize	= FLIP_grp_pktsize;
uint16 grp_maxgrp	= FLIP_grp_maxgrp;
#endif /* FLIPGRP */
