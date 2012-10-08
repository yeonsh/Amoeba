/*	@(#)sp_mktrans.c	1.3	94/04/07 11:09:50 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "sp_stubs.h"

#ifdef OWNDIR
/*
 * In this case, sp_trans() first looks if the directory is managed by
 * ourselves.  If so, it calls the routine performing the operation requested
 * directly. If not, it does a real transaction with the external server.
 */
extern int sp_trans _ARGS((header *hdr,
			   char *inbuf,  int insize,
			   char *outbuf, int outsize));

#define sp_dotrans(hdr, inbuf, insize, outbuf, outsize) \
	  sp_trans(hdr, inbuf, (int)insize, outbuf, (int)outsize)

#else

#define sp_dotrans(hdr, inbuf, insize, outbuf, outsize) \
	     trans(hdr, inbuf, insize, hdr, outbuf, outsize)

#endif

#ifdef __STDC__
errstat
sp_mktrans(int ntries, capset *dir, header *hdr, int cmd,
	   const char *inbuf, bufsize insize, char *outbuf, bufsize outsize)
#else
errstat
sp_mktrans(ntries, dir, hdr, cmd, inbuf, insize, outbuf, outsize)
int      ntries;
capset  *dir;
header  *hdr;
int      cmd;
char    *inbuf;
bufsize  insize;
char    *outbuf;
bufsize  outsize;
#endif
{
    header  myhdr;
    int     try;
    bufsize n;
    errstat err;
    errstat failed = SP_UNAVAIL; /* ntries RPC_NOTFOUNDs in a row */
        
    for (try = 0; try < ntries; try++) {
	int i;

	for (i = 0; i < dir->cs_final; i++) {
	    suite *s;

	    s = &dir->cs_suite[i];
	    if (!s->s_current) {
		continue;
	    }

	    if (hdr != NULL) {	/* set parameters */
		myhdr = *hdr;
	    }
	    myhdr.h_command = cmd;
	    myhdr.h_port = s->s_object.cap_port;
	    myhdr.h_priv = s->s_object.cap_priv;
	    if (inbuf != NULL) {
		myhdr.h_size = insize;
	    } else {
		myhdr.h_size = outsize;
	    }

	    n = sp_dotrans(&myhdr, (char *)inbuf, insize, outbuf, outsize);
	    if (ERR_STATUS(n)) {
		err = ERR_CONVERT(n);
	    } else {
		err = ERR_CONVERT(myhdr.h_status);
	    }

	    if (err == RPC_FAILURE && ntries > 1) {
		/* Retry idempotent operation, but return RPC_FAILURE
		 * in case we don't succeed in a following iteration.
		 */
		failed = RPC_FAILURE;
	    } else if (err != RPC_NOTFOUND) {
		if (err == STD_OK) {
		    if (n > 0 && myhdr.h_size != n) {
			/* Work around bug in old soap svr: it didn't always
			 * set hdr.h_size (e.g. in sp_lookup).
			 */
			myhdr.h_size = n;
		    }
		    if (hdr != NULL) {
			*hdr = myhdr;
		    }
		}
		return err;
	    }
	}
    }
    
    return failed;
}
