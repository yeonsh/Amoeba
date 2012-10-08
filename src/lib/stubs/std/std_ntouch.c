/*	@(#)std_ntouch.c	1.4	96/02/27 11:18:31 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * This is a variant of std_touch that touches 'n' objects at a time.  The objects must
 * all be on the same server.  It returns in 'done' the number of objects successfully
 * touched.  If any of the touches failed, the error status of the last object
 * unsuccessfully touched is returned.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "module/stdcmd.h"

#define	MAX_TRANS	20000	/* Largest trans buffer we use: holds 2000 privs */
#define	MIN(a, b)	((a) < (b) ? (a) : (b))

errstat
std_ntouch(p, n, privbuf, done)
port *		p;		/* in: port of server whose objects are to be touched */
int		n;		/* in: number of objects in privbuf */
private *	privbuf;	/* in: private parts of capabilities to be touched */
int *		done;		/*out: number of objects successfully touched */
{
    header	hdr;
    bufsize	status;
    bufsize	max_n;
    errstat	err;

    if (n <= 0) /* If we got null pointers for p, etc the calling program will crash */
	return STD_ARGBAD;

    *done = 0;
    err = STD_OK;

    /* We loop, sending max_n private parts at a time to be touched */
    max_n = MAX_TRANS / sizeof (private);
    while (n > 0)
    {
	hdr.h_port = *p;
	hdr.h_command = STD_NTOUCH;
	hdr.h_size = MIN((unsigned) n, max_n);

	status = trans(&hdr, (bufptr) privbuf, hdr.h_size * sizeof (private),
		       &hdr, NILBUF, 0);
	if (status != 0)
	{
	    /* If we got an RPC error we give up */
	    return ERR_CONVERT(status);
	}
	*done += hdr.h_extra;
	if (hdr.h_status != STD_OK)
	    err = ERR_CONVERT(hdr.h_status);
	if (err == STD_COMBAD)
	{
	    *done = 0;
	    return err;
	}
	n -= max_n;
	privbuf += max_n;
    }
    return err;
}
