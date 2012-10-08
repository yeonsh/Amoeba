/*	@(#)getreq.c	1.6	94/04/07 14:07:15 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "host_os.h"
#include "amoeba.h"
#include "amparam.h"
#include "cmdreg.h"
#include "stderr.h"
#include <signal.h>

#ifdef __STDC__
bufsize getreq(header *hdr, bufptr buf, _bufsize cnt)
#else
bufsize
getreq(hdr, buf, cnt)
header *	hdr;
bufptr		buf;
bufsize		cnt;
#endif
{
#ifdef USE_AM4_RPC
    extern trpar		am_tp;
    register struct param *	par = &am_tp.tp_par[0];
#endif

    if (cnt > 30000) {
	(void) raise(SIGSYS);
	return RPC_FAILURE;
    }

#ifdef USE_AM4_RPC
    par->par_hdr = hdr;
    par->par_buf = buf;
    par->par_cnt = cnt;
    return _amoeba(AM_GETREQ);
#else
    return flrpc_getreq(hdr, buf, (long) cnt);
#endif
}
