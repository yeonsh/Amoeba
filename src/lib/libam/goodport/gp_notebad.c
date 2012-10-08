/*	@(#)gp_notebad.c	1.3	94/04/07 10:02:12 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * This is part of the module for avoiding timeout errors when doing repeated
 * transactions to bad ports.  Bad ports are defined as ones that have
 * previously given us RPC_NOTFOUND errors.
 */
#include <amoeba.h>
#include <stderr.h>
#include <module/goodport.h>

/*
 * This function is called with a capability, cap, and the status returned by
 * a function that did a transaction to that cap.  If the status is
 * RPC_NOTFOUND, the port of the cap is added to the list of bad ports.  The
 * status is returned in any case.
 */
errstat
gp_notebad(cap, status)
    capability *cap;
    errstat status;
{
    if (status == RPC_NOTFOUND)
	(void)gp_badport(&cap->cap_port, GP_APPEND);
    return status;
}

