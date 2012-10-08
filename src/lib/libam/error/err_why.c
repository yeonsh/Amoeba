/*	@(#)err_why.c	1.6	94/04/07 10:01:23 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Return an error message for the commonest amoeba errors. */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "capset.h"
#include "am_exerrors.h"
#include "soap/soap.h"
#include "module/strmisc.h"

#ifndef KERNEL
#include <stdio.h>
#endif

static struct am_err {
	errstat error;
	char *message;
} am_errlist[] = {
	{RPC_FAILURE,	"rpc failure"},
	{RPC_NOTFOUND,	"server not found"},
	{RPC_BADADDRESS,"illegal address"},
	{RPC_ABORTED,	"rpc aborted"},
	{RPC_TRYAGAIN,	"retry rpc"},
	{RPC_BADPORT,	"null port"}, /* Meaning might change later */
	{RPC_UNSAFE,	"path to server is untrusted"},

	{BC_FAIL,	"group send failure"},
	{BC_ABORT,	"group abort"},
	{BC_TOOBIG,	"group message too big"},
	{BC_BADPORT,	"invalid port"},
	{BC_NOEXIST,	"does not exist"},
	{BC_TOOMANY,	"out of group resources"},
	{BC_ILLRESET,	"illegal group reset"},
	
	{STD_CAPBAD,	"invalid capability"},
	{STD_COMBAD,	"invalid command"},
	{STD_ARGBAD,	"invalid argument"},
	{STD_NOTNOW,	"not now, please"},
	{STD_NOSPACE,	"no space"},
	{STD_DENIED,	"operation denied"},
	{STD_NOMEM,	"out of memory"},
	{STD_EXISTS,	"exists already"},
	{STD_NOTFOUND,	"not found"},
	{STD_SYSERR,	"internal inconsistency"},
	{STD_INTR,	"interrupted"},
	{STD_OVERFLOW,	"buffer overflow"},
	{STD_WRITEPROT,	"medium write protected"},
	{STD_NOMEDIUM,	"no medium present in drive"},
	
	{SP_UNAVAIL,	"soap directory service unavailable"},
	{SP_NOTEMPTY,	"soap directory not empty"},
	{SP_UNREACH,	"soap object unreachable"},
	{SP_CLASH,	"soap serializability test failed"},
	
	{AMEX_NOPROG,	"no program given"},
	{AMEX_PDLOOKUP,	"can't find program"},
	{AMEX_PDREAD,	"can't read process descriptor"},
	{AMEX_PDSHORT,	"not all bytes read from process descriptor"},
	{AMEX_PDSIZE,	"inconsistent process size"},
	{AMEX_NOCAPS,	"no capability environment"},
	{AMEX_NOHOST,	"can't find suitable host processor"},
	{AMEX_GETDEF,	"can't get MMU parameters"},
	{AMEX_MALLOC,	"can't make local stack segment"},
	{AMEX_STACKOV,	"env+args too big for stack"},
	{AMEX_SEGCREATE,"can't create stack segment"},
	
	{0}
};


char *
err_why(err)
errstat	err;
{
    struct am_err *ep;
    static char buffer[sizeof("amoeba error 999999999")];
    
    for (ep = am_errlist; ep->error != 0; ++ep) {
	if (err == ep->error)
	    return ep->message;
    }
#ifdef KERNEL
    *bprintf(buffer, buffer + sizeof(buffer) - 1,
	"amoeba error %ld", (long) err) = '\0';
#else
    (void) sprintf(buffer, "amoeba error %ld", (long) err);
#endif /* KERNEL */
    return buffer;
}
