/*	@(#)ajerror.c	1.3	94/04/07 10:22:42 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Convert Amoeba error status to POSIX errno value 
 *
 * XXX Not all Amoeba errors can be translated 1-1 to POSIX errors.
 * XXX Certain errors deserve new error codes (but I'm too lazy).
 * XXX The interpretation of others is context-dependent.
 *
 * last modified apr 22 93 Ron Visser
 */

#include "ajax.h"
#include "am_exerrors.h"
#include "fifo/fifo.h"

static struct {
	errstat amerror;
	int errno;
} translate[] = {
	{STD_OK,		0},
	
	{RPC_FAILURE,		EIO},
	{RPC_NOTFOUND,		EIO},
	{RPC_BADADDRESS,	EFAULT},
	{RPC_ABORTED,		EIO},
	{RPC_TRYAGAIN,		EAGAIN},
	{RPC_BADPORT,		EIO},
	
	{STD_CAPBAD,		EBADF},
	{STD_COMBAD,		ENOSYS},
	{STD_ARGBAD,		EINVAL},
	{STD_NOTNOW,		EBUSY},
	{STD_NOSPACE,		ENOSPC},
	{STD_DENIED,		EACCES},
	{STD_NOMEM,		ENOMEM},
	{STD_EXISTS,		EEXIST},
	{STD_NOTFOUND,		ENOENT},
	{STD_SYSERR,		EIO},
	{STD_INTR,		EINTR},
	
	{AMEX_NOPROG,		EINVAL},
	{AMEX_PDLOOKUP,		ENOENT},
	{AMEX_PDREAD,		ENOEXEC},
	{AMEX_PDSHORT,		EIO},
	{AMEX_PDSIZE,		ENOEXEC},
	{AMEX_NOCAPS,		EINVAL},
	{AMEX_NOHOST,		ENOEXEC},
	{AMEX_GETDEF,		EIO},
	{AMEX_MALLOC,		ENOMEM},
	{AMEX_STACKOV,		ENOMEM},
	{AMEX_SEGCREATE,	ENOMEM},
	
	{SP_UNAVAIL,		EIO},
	{SP_NOTEMPTY,		ENOTEMPTY},
	{SP_UNREACH,            ENOENT},
	{SP_CLASH,              EEXIST},

	{FIFO_AGAIN,		EAGAIN},
	{FIFO_BADF,		EBADF},
	{FIFO_ENXIO,		ENXIO},
};

int
_ajax_error(err)
	errstat err;
{
	int i;
	for (i = 0; i < sizeof(translate) / sizeof(translate[0]); i++) {
		if (translate[i].amerror == err)
			return translate[i].errno;
	}
	return EIO;
}
