/*	@(#)isattycap.c	1.3	96/02/27 10:11:17 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "ailamoeba.h"
#include "stderr.h"
#include "posix/termios.h"
#include "class/tios.h"

/*
 *	Iff this moves to the library; these macro's need to be looked at
 */

#ifdef NDEBUG
#define MON_EVENT(s)	do { ; } while(0)
#else
#include "monitor.h"
#endif

#ifndef RPC_RETRY
#define RPC_RETRY	2
#endif

/*
 *	Test whether a capability points to a tty
 */

int
isattycap(cap)
    capability *cap;
{
    errstat err;
    struct termios tios;
    int i = RPC_RETRY;
    while (i-- && (err = tios_getattr(cap, &tios)) == RPC_FAILURE)
	;
    if (err == STD_OK) MON_EVENT("Isattycap: y");
    else MON_EVENT("Isattycap: n");
    return err == STD_OK;
} /* isattycap */
