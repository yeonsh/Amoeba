/*	@(#)mon_fliprpc.c	1.4	96/02/27 11:01:59 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#define MONWHIZ
#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "monitor.h"
#include "module/rpc.h"


extern void _MN_xqt _ARGS(( header *hp, bufptr * retbuf, bufsize * retsiz ));

/*
 * The reason for including seperate __STDC__ prototypes has to do with
 * the fact that in old K&R C unsigned shorts are supplied to the
 * called function as int. An old-style function definition forces
 * this behavior. But this conflicts with our current ANSI-C prototype,
 * which explicitly mentions "unsigned short".
 * Another solution would be to have the parameter be "int", but I
 * don't want to open that can of worms.
 */

#ifdef __STDC__
bufsize _MN_rpc_getreq(header *hdr, bufptr buf, bufsize size)
#else
bufsize _MN_rpc_getreq(hdr,buf,size)
header *hdr;
bufptr buf;
bufsize size;
#endif
{
	register bufsize n;
	bufptr retbuf;
	bufsize retsiz;
	port saveport;

	MON_EVENT("GETREQ");
	saveport = hdr->h_port;
	while ((n=rpc_getreq(hdr,buf,size))==0 && hdr->h_command==STD_MONITOR) {
		_MN_xqt(hdr, &retbuf, &retsiz);
		rpc_putrep(hdr, retbuf, retsiz);
		hdr->h_port = saveport;
	}
	switch ((short) n) {
	case RPC_BADADDRESS:
		MON_EVENT("GETREQ: BADADDRESS");
		break;
	case RPC_ABORTED:
		MON_EVENT("GETREQ: ABORTED");
		break;
	}
	return n;
}

#ifdef __STDC__
void _MN_rpc_putrep(header *hdr, bufptr buf, bufsize size)
#else
void _MN_rpc_putrep(hdr,buf,size)
header *hdr;
bufptr buf;
bufsize size;
#endif
{
	MON_EVENT("PUTREP");
	rpc_putrep(hdr,buf,size);
}

#ifdef __STDC__
bufsize _MN_rpc_trans(header *hdr1, bufptr buf1, bufsize size1,
		 header *hdr2, bufptr buf2, bufsize size2)
#else
bufsize _MN_rpc_trans(hdr1,buf1,size1,hdr2,buf2,size2)
header *hdr1;
bufptr buf1;
bufsize size1;
header *hdr2;
bufptr buf2;
bufsize size2;
#endif
{
	register bufsize n;

	MON_EVENT("TRANS");
	n = rpc_trans(hdr1,buf1,size1,hdr2,buf2,size2);
	switch ((short) n) {
	case RPC_FAILURE:	MON_EVENT("TRANS: FAIL");	break;
	case RPC_NOTFOUND:	MON_EVENT("TRANS: NOTFOUND");	break;
	case RPC_ABORTED:	MON_EVENT("TRANS: ABORTED");	break;
	case RPC_BADADDRESS:	MON_EVENT("TRANS: BADADDRESS");	break;
	}
	return n;
}
