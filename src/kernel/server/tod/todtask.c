/*	@(#)todtask.c	1.6	94/04/06 10:04:31 */
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
#include "tod/tod.h"
#include "assert.h"
INIT_ASSERT
#include "string.h"
#include "module/rnd.h"
#include "module/prv.h"
#include "sys/proto.h"

static char infostr[] = "TOD server";
static capability todcap;


static void
todinit()
{
    capability	pub_cap;

    uniqport(&todcap.cap_port);
    if (prv_encode(&todcap.cap_priv, (objnum) 0, RGT_ALL, &todcap.cap_port) < 0)
	panic("tod: prv_encode failed\n");

    /* generate put capability and publish it */
    priv2pub(&todcap.cap_port, &pub_cap.cap_port);
    pub_cap.cap_priv = todcap.cap_priv;
    dirappend(TOD_SVR_NAME, &pub_cap);
}


static void
todthread _ARGS((void))
{
    header	hdr;
    bufsize	n;
    long	secs;
    int		msecs;
    char	buf[sizeof(infostr)];
    objnum	num;
    rights_bits	rights;

    for (;;) {
	hdr.h_port = todcap.cap_port;
#ifdef USE_AM6_RPC
	n = rpc_getreq(&hdr, NILBUF, 0);
#else
	n = getreq(&hdr, NILBUF, 0);
#endif
	compare((short) n, !=, RPC_FAILURE);/*AMOEBA4*/
	if (ERR_STATUS(n))
	{
	    printf("tod server: getreq returned %d\n", (short) n);/*AMOEBA4*/
	    continue;
	}
	switch(hdr.h_command)
	{
	case STD_AGE: /* age the clock?* /
	case STD_TOUCH: /* we ignore these two */
	    n = 0;
	    hdr.h_status = STD_OK;
	    break;
	case STD_INFO:
	    (void) strcpy(buf, infostr);
	    hdr.h_size = n = sizeof(infostr)-1;
	    hdr.h_status = STD_OK;
	    break;
	case STD_RESTRICT:
	    n = 0;
	    num = prv_number(&hdr.h_priv);
	    if (num != 0 || prv_decode(&hdr.h_priv, &rights,
					    &todcap.cap_priv.prv_random) < 0)
	    {
		hdr.h_status = STD_CAPBAD;
		break;
	    }
	    rights &= (rights_bits) hdr.h_offset;
	    if (prv_encode(&hdr.h_priv, num, rights, &todcap.cap_priv.prv_random) < 0)
		hdr.h_status = STD_CAPBAD;
	    else
		hdr.h_status = STD_OK;
	    break;
	case TOD_GETTIME:
	    read_hardware_clock(&secs, &msecs);
	    tod_encode(&hdr, secs, msecs, 0, 0);
	    n = 0;
	    hdr.h_status = STD_OK;
	    break;
	case TOD_SETTIME:
	    n = 0;
	    num = prv_number(&hdr.h_priv);
	    if (num != 0 || prv_decode(&hdr.h_priv, &rights,
					    &todcap.cap_priv.prv_random) < 0)
	    {
		hdr.h_status = STD_CAPBAD;
		break;
	    }
	    if ((rights & RGT_ALL) != RGT_ALL)
	    {
		hdr.h_status = STD_DENIED;
		break;
	    }
	    tod_decode(&hdr, &secs, &msecs, &n, &n);
	    write_hardware_clock(secs, msecs);
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


#ifndef NR_TOD_THREADS
#define NR_TOD_THREADS	2
#endif
#ifndef TOD_STKSIZ
#define TOD_STKSIZ	0 /* default size */
#endif

void
inittod()
{
    int i;

    todinit();
    for (i = 0; i < NR_TOD_THREADS; i++)
	NewKernelThread(todthread, (vir_bytes) TOD_STKSIZ);
}
