/*	@(#)aitrans.c	1.5	94/04/07 14:05:00 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "host_os.h"
#include "amoeba.h"
#include "amparam.h"
#include "stderr.h"
#include "stdio.h"
#include "assert.h"
#include <signal.h>

/*
 * This module implements a trans that dynamically chooses the 4.0 rpc
 * protocol or the 5.0 rpc protocol. It first tries the old one, and if it
 * returns a RPC_NOTFOUND, it tries the new one. To speed things up,
 * it keeps a cache of used ports and the protocol through which the cached
 * port is reachable. Because the cache is per process and disappears when
 * a process dies, we don't bother to invalidate entries.
 */

#define MAXCACHE 	1024

typedef struct entry {
    port	e_port;
    int		e_old;
} entry_t, *entry_p;

static entry_t	cache[MAXCACHE];


static int
lookup(p, old)
    port *p;
    int *old;
{
    int i;

    for(i=0; i < MAXCACHE; i++) {
	if(PORTCMP(p, &cache[i].e_port)) {
	    *old = cache[i].e_old;
	    return(1);
	}
    }
    return(0);
}    
   

static void 
install(p, old)
    port *p;
    int old;
{
    int i;
    entry_p free = 0;

    for(i=0; i < MAXCACHE; i++) {
	if(PORTCMP(p, &cache[i].e_port)) {
	    cache[i].e_port = *p;
	    cache[i].e_old = old;
	    return;
	} else 	if(NULLPORT(&cache[i].e_port))  {
	    free = &cache[i];
	}
    }
    assert(free != 0);
    free->e_port = *p;
    free->e_old = old;
}
    



extern int _flip_notpresent;

#ifdef __STDC__
bufsize trans(header *hdr1, bufptr buf1, _bufsize cnt1,
	      header *hdr2, bufptr buf2, _bufsize cnt2)
#else
bufsize
trans(hdr1, buf1, cnt1, hdr2, buf2, cnt2)
header *	hdr1;
bufptr		buf1;
bufsize		cnt1;
header *	hdr2;
bufptr		buf2;
bufsize		cnt2;
#endif
{
    extern trpar		am_tp;
    register struct param *	req = &am_tp.tp_par[0];
    register struct param *	rep = &am_tp.tp_par[1];
    bufsize			r;
    int				old;
    port			install_port;

    if (cnt1 > 30000 || cnt2 > 30000) {
	(void) raise(SIGSYS);
	return RPC_FAILURE;
    }
    hdr1->h_signature._portbytes[0] = 1; /* no security for old rpc. */
    install_port = hdr1->h_port;
    if(lookup(&hdr1->h_port, &old)) {
	if(old) {
	    req->par_hdr = hdr1;
	    req->par_buf = buf1;
	    req->par_cnt = cnt1;
	    rep->par_hdr = hdr2;
	    rep->par_buf = buf2;
	    rep->par_cnt = cnt2;
	    r = (bufsize) _amoeba(AM_TRANS);
	} else {
	    r = (bufsize) flrpc_trans(hdr1, buf1, cnt1, hdr2, buf2, cnt2);
	}
	return(r);
    } else {
	_flip_notpresent = 0;
	r = (bufsize) flrpc_trans(hdr1, buf1, cnt1, hdr2, buf2, cnt2);
	if(_flip_notpresent || ((short) r) == RPC_NOTFOUND) {
	    req->par_hdr = hdr1;
	    req->par_buf = buf1;
	    req->par_cnt = cnt1;
	    rep->par_hdr = hdr2;
	    rep->par_buf = buf2;
	    rep->par_cnt = cnt2;
	    r = (bufsize) _amoeba(AM_TRANS);
	    if(!ERR_STATUS(r)) {
		install(&install_port, 1);
	    }
	} else {
		install(&install_port, 0);
        }
	return(r);
    }
}
