/*	@(#)putrep.c	1.6	94/04/07 14:07:34 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "host_os.h"
#include "amoeba.h"
#include "amparam.h"
#include <signal.h>

#ifdef __STDC__
void putrep(header *hdr, bufptr buf, _bufsize cnt)
#else
void
putrep(hdr, buf, cnt)
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
	return;
    }

#ifdef USE_AM4_RPC
    par->par_hdr = hdr;
    par->par_buf = buf;
    par->par_cnt = cnt;
    (void) _amoeba(AM_PUTREP);
#else
    hdr->h_signature._portbytes[0] = 1;
    flrpc_putrep(hdr, buf, (long) cnt);
#endif
}
