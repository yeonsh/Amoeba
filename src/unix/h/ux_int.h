/*	@(#)ux_int.h	1.4	96/02/27 11:55:06 */
/*
 * Copyright 1995 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __UX_INT_H__
#define __UX_INT_H__

/* NOTE WELL:  if you change any of the FIRST/LAST constants then make sure
 *		that you adjust the installflipdevice shell script
 */

#define FIRST_RPC	0
#define LAST_RPC 	127
#define NRPC		(LAST_RPC - FIRST_RPC + 1)

#define FIRST_CTRL	128
#define LAST_CTRL	143
#define NCTRL		1	/* (LAST_CTRL - FIRST_CTRL + 1) */

#define FIRST_IP	144
#define LAST_IP		159
#define NIP		1	/* (LAST_IP - FIRST_IP + 1) */

#define FIRST_FLIP	160
#define LAST_FLIP	175
#define NFLIP		0	/* (LAST_FLIP - FIRST_FLIP + 1) */

#define FLRPC_TRANS	9
#define FLRPC_GETREQ	10
#define FLRPC_PUTREP	11
#define FLCTRL_STAT	12
#define FLCTRL_DUMP	13
#define FLCTRL_CTRL	14
#define FL_IP_WRITE	15
#define FL_IP_READ	16

#define FL_SIGNAL	99

#define FL_DS_USER      1

#endif /* __UX_INT_H__ */
