/*	@(#)tod_settime.c	1.3	94/04/07 11:14:59 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "tod/tod.h"
#include "module/tod.h"

errstat
tod_settime(seconds, millisecs)
long	seconds;	/* in: */
int	millisecs;	/* in: */
{
    header	hdr;
    bufsize	t;
    errstat	err;
    extern capability *tod_cap;

    if (tod_cap == 0)
	if ((err = tod_defcap()) != STD_OK)
	    return err;
    hdr.h_port = tod_cap->cap_port;
    hdr.h_priv = tod_cap->cap_priv;
    hdr.h_command = TOD_SETTIME;
    tod_encode(&hdr, seconds, millisecs, 0, 0);

    if ((t = trans(&hdr, NILBUF, 0, &hdr, NILBUF, 0)) != 0)
    {
	tod_cap = 0;	  /* user may want to change the capability */
	return ERR_CONVERT(t);
    }
    return ERR_CONVERT(hdr.h_status);
}

