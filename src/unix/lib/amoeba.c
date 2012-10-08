/*	@(#)amoeba.c	1.4	94/04/07 14:06:06 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include <sys/types.h>
#ifdef FCNTL
#include <fcntl.h>
#else
#include <sgtty.h>
#endif
#include <errno.h>

#include "host_os.h"
#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "amparam.h"

trpar	am_tp= { { {0, 0, 0}, {0, 0, 0} }, 300};

#ifdef __STDC__
#include <stdlib.h>

static unaligned_trpar cvtbuf;
#endif

_amoeba(req)
int	req;
{
    static int fd = -1;

    if (fd < 0)
    {
	if ((fd = open("/dev/amoeba", 0)) < 0)
	{
	    return (errno == ENOENT || errno == ENODEV) ?
					RPC_NOTFOUND : RPC_TRYAGAIN;
	}
#ifdef FCNTL
	fcntl(fd, F_SETFD, 1);
#else
	ioctl(fd, FIOCLEX, (struct sgttyb *) 0); /* close on exec */
#endif
    }

#ifdef __ACK
    /* convert the trpar buffer to the format in which the amoeba driver
     * in the kernel expects it, i.e. without the holes created by ACK.
     */
    {
	memcpy(&cvtbuf[0], &am_tp.tp_par[0], PARAM_SIZE);
	memcpy(&cvtbuf[PARAM_SIZE], &am_tp.tp_par[1], PARAM_SIZE);
	memcpy(&cvtbuf[2*PARAM_SIZE], &am_tp.tp_maxloc, sizeof(uint16));
	return sysamoeba(fd, req, cvtbuf);
    }
#else
    return sysamoeba(fd, req, &am_tp);
#endif
}
