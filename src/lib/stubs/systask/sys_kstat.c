/*	@(#)sys_kstat.c	1.3	96/02/27 11:19:09 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "systask/systask.h"
#include "module/systask.h"


#ifdef __STDC__
errstat
sys_kstat
(
    capability * cap,	  /* in:  capability for kernel's systask */
    char	 flag,	  /* in:  table selector */
    char *	 buf,	  /* in:  pointer to buffer to receive kernel data */
    bufsize	 bufsz,	  /* in:  size of 'buf' */
    int *	 result   /* out: # bytes returned in 'buf' */
)
#else
errstat
sys_kstat(cap, flag, buf, bufsz, result)
capability *	cap;	/* in:  capability for kernel's systask */
char		flag;	/* in:  table selector */
char *		buf;	/* in:  pointer to buffer to receive kernel data */
bufsize		bufsz;	/* in:  size of 'buf' */
int *		result;	/* out: # bytes returned in 'buf' */
#endif
{
    header	hdr;
    bufsize	t;

    hdr.h_port = cap->cap_port;
    hdr.h_priv = cap->cap_priv;
    hdr.h_command = SYS_KSTAT;
    hdr.h_extra = flag;

    t = trans(&hdr, NILBUF, 0, &hdr, buf, bufsz);
    if (ERR_STATUS(t))
	return ERR_CONVERT(t);
    *result = (int) t;
    return ERR_CONVERT(hdr.h_status);
}
