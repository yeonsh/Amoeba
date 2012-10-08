/*	@(#)timeout.c	1.2	94/04/07 14:07:58 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "host_os.h"
#include "amoeba.h"
#include "amparam.h"

interval
timeout(maxloc)
interval	maxloc;
{
    extern trpar	am_tp;
    register interval	old = am_tp.tp_maxloc * 100;

    am_tp.tp_maxloc = (uint16) (maxloc / 100);
    return old;
}
