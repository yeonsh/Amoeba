/*	@(#)std_status.c	1.5	96/02/27 11:18:50 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "module/stdcmd.h"

errstat
std_status(cap, info, n, len)
capability *	cap;	/* in: capability of object for which info is wanted */
char *		info;	/* in: pointer to buffer to hold info */
int		n;	/* in: size of buffer 'info' */
int *		len;	/* out: # bytes actually returned in 'info' */
{
    header	hdr;
    bufsize	result;

    hdr.h_size = n;	/* for servers with Ail-generated main-loop */
    hdr.h_port = cap->cap_port;
    hdr.h_priv = cap->cap_priv;
    hdr.h_command = STD_STATUS;
    result = trans(&hdr, NILBUF, 0, &hdr, info, (bufsize) n);
    if (ERR_STATUS(result))
	return ERR_CONVERT(result);
    if (hdr.h_status == STD_OK)
    {
	if (result < (unsigned) n)
	    *len = result;
	else if ((*len = (int) hdr.h_size) > n)
	    hdr.h_status = STD_OVERFLOW;
    }
    return ERR_CONVERT(hdr.h_status);
}
