/*	@(#)sp_stubs.h	1.6	96/02/27 11:17:54 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdlib.h>
#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "stdcom.h"
#include "capset.h"
#include "soap/soap.h"
#include "soap/soapsvr.h"
#include "module/path.h"
#include "module/buffers.h"
#include <string.h>
#define  _POSIX_SOURCE
#include "posix/limits.h"	/* For PATH_MAX */

#define	alloc(s, n)	sp_alloc((unsigned)(s), (unsigned)(n))

#define SP_NOMOREROWS ((uint16)(-1))

/* Buffer size big enough to hold a capset of size MAXCAPSET.
 * See buf_get_capset() for an explanation of this formula.
 */
#define CAPSETBUFSIZE   (2*sizeof(short) + \
                         MAXCAPSET * (sizeof(capability) + sizeof(short)))

#ifndef __STDC__
#define const /* const */
#endif

#ifdef KERNEL
#define OWNDIR
#endif

#include "_ARGS.h"

#define	sp_mktrans	_sp_mktrans

errstat sp_mktrans     _ARGS((int         ntries, capset *dir,
			      header     *hdr,    int     cmd,
			      const char *inbuf,  bufsize insize,
			      char       *outbuf, bufsize outsize));
