/*	@(#)rndtask.c	1.7	96/02/27 14:21:02 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "machdep.h"
#include "random/random.h"
#include "file.h"
#include "assert.h"
INIT_ASSERT
#include "string.h"
#include "module/rnd.h"
#include "module/prv.h"
#include "sys/proto.h"

static char infostr[] = "random number server";
static capability rndcap;

extern uint16	rand();

static void
rndinit()
{
    capability pub_cap;

    uniqport(&rndcap.cap_port);
    if (prv_encode(&rndcap.cap_priv, (objnum) 0, PRV_ALL_RIGHTS,
							&rndcap.cap_port) < 0)
    {
	panic("rnd: prv_encode failed\n");
    }

    /* publish the put capability of the server */
    priv2pub(&rndcap.cap_port, &pub_cap.cap_port);
    pub_cap.cap_priv = rndcap.cap_priv;
    dirappend(RANDOM_SVR_NAME, &pub_cap);
}

static void
rndthread _ARGS((void))
{
    header hdr;
    bufsize n;
    char buf[MAX_RANDOM];
    char *p;

    for (;;)
    {
	    hdr.h_port = rndcap.cap_port;
#ifdef USE_AM6_RPC
	    n = rpc_getreq(&hdr, NILBUF, 0);
#else
	    n = getreq(&hdr, NILBUF, 0);
#endif
	    compare(n, !=, (bufsize) RPC_FAILURE);
	    switch(hdr.h_command)
	    {
	    case STD_AGE:
	    case STD_RESTRICT:
	    case STD_TOUCH:
		    n = 0;
		    hdr.h_status = STD_OK;
		    break;
	    case STD_INFO:
		    (void) strcpy(buf, infostr);
		    hdr.h_size = n = sizeof infostr - 1;
		    hdr.h_status = STD_OK;
		    break;
	    case RND_GETRANDOM:
	    case FSQ_READ:
		    n = hdr.h_size;
		    if (n > MAX_RANDOM)
			n = MAX_RANDOM;
		    for (p = buf; p < buf + n; p += 2)
			    *(short *) p = rand();
		    hdr.h_extra = FSE_MOREDATA;
		    hdr.h_status = STD_OK;
		    break;
	    default:
		    n = 0;
		    hdr.h_status = STD_COMBAD;
		    break;
	    }
#ifdef USE_AM6_RPC
	    rpc_putrep(&hdr, buf, n);
#else
	    putrep(&hdr, buf, n);
#endif
    }
}

#ifndef NR_RND_THREADS
#define NR_RND_THREADS	3
#endif
#ifndef RND_STKSIZ
#define RND_STKSIZ	0 /* default size */
#endif

void
initrndthread()
{
    int i;

    rndinit();
    for (i = 0; i < NR_RND_THREADS; i++)
	NewKernelThread(rndthread, (vir_bytes) RND_STKSIZ);
}
