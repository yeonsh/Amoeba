/*	@(#)rnd_getrand.c	1.5	96/02/27 11:17:09 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "ampolicy.h"
#include "random/random.h"
#include "module/name.h"
#include "module/rnd.h"


static capability *	rnd_cap;


void
rnd_setcap(cap)
capability *	cap;
{
    static capability tmpcap;

    tmpcap = *cap;
    rnd_cap= &tmpcap;
}


errstat
rnd_defcap()
{
    static capability	cap;
    capability *	rnd;
    errstat		err;

    if ((rnd = getcap("RANDOM")) == 0)
    {
	if ((err = name_lookup(DEF_RNDSVR, &cap)) == STD_OK)
	    rnd_setcap(&cap);
	return err;
    }
    rnd_setcap(rnd);
    return STD_OK;
}


errstat
rnd_getrandom(buf, size)
char *	buf;
int	size;
{
    header		hdr;
    bufsize		t;
    errstat		err;

    if (rnd_cap == 0)
	if ((err = rnd_defcap()) != STD_OK)
	    return err;
    hdr.h_port = rnd_cap->cap_port;
    hdr.h_priv = rnd_cap->cap_priv;
    hdr.h_command = RND_GETRANDOM;
    hdr.h_size = size;
    t = trans(&hdr, NILBUF, 0, &hdr, buf, (bufsize) size);
    if (ERR_STATUS(t))
    {
	rnd_cap = 0;
	return ERR_CONVERT(t);
    }
    if (ERR_STATUS(hdr.h_status)) {
	/* Non-random number server specified?  Switch back to default. */
	rnd_cap = 0;
	return ERR_CONVERT(hdr.h_status);
    }
    if (t != size)
	return STD_OVERFLOW;
    return STD_OK;
}
