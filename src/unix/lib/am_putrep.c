/*	@(#)am_putrep.c	1.2	94/04/07 14:05:42 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "host_os.h"
#include "amoeba.h"
#include "amparam.h"

void
am_putrep(hdr, buf, cnt)
header *	hdr;
bufptr		buf;
bufsize		cnt;
{
    extern trpar		am_tp;
    register struct param *	par = &am_tp.tp_par[0];

    par->par_hdr = hdr;
    par->par_buf = buf;
    par->par_cnt = cnt;
    (void) _amoeba(AM_PUTREP);
}
