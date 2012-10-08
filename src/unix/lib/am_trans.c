/*	@(#)am_trans.c	1.2	94/04/07 14:05:49 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "host_os.h"
#include "amoeba.h"
#include "amparam.h"

bufsize
am_trans(hdr1, buf1, cnt1, hdr2, buf2, cnt2)
header *	hdr1;
bufptr		buf1;
bufsize		cnt1;
header *	hdr2;
bufptr		buf2;
bufsize		cnt2;
{
    extern trpar		am_tp;
    register struct param *	req = &am_tp.tp_par[0];
    register struct param *	rep = &am_tp.tp_par[1];

    req->par_hdr = hdr1;
    req->par_buf = buf1;
    req->par_cnt = cnt1;
    rep->par_hdr = hdr2;
    rep->par_buf = buf2;
    rep->par_cnt = cnt2;
    return _amoeba(AM_TRANS);
}
