/*	@(#)stderr.h	1.6	96/02/27 10:26:33 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __STDERR_H__
#define __STDERR_H__

#include <_ARGS.h>
#include <cmdreg.h>

/*
** stderr.h
**
**	This file contains the standard errors that all servers/user programs
**	should understand.  This includes the RPC errors.
**	There is a special fucntion which can be called to get an
**	explanation of the standard error from the server.
**
**	Requests for registration of standard errors should be mailed to:
**		amoeba-support@cs.vu.nl
**
**  Author:
**	Greg Sharp
*/

/* The successful return status (no error) */
#define	STD_OK			((errstat) 0)

/* RPC failures and errors */
#define	RPC_FAILURE		(-1)
#define	RPC_NOTFOUND		(-2)
#define	RPC_BADADDRESS		(-3)
#define	RPC_ABORTED		(-4)
#define	RPC_TRYAGAIN		(-5)
/* attempt to send error code as a command */
#define	RPC_COMBAD		(-6)
/* server returned RPC error in hdr.h_status field */
#define	RPC_REMOTERPC		(-7)
/* transaction on null port */
#define RPC_BADPORT		(-8)
/* server is only reachable through a untrusted path */
#define RPC_UNSAFE		(-9)

/* Group return values: */
#define BC_FAIL         	(-20)
#define BC_ABORT        	(-21)
#define BC_TOOBIG       	(-22)
#define BC_BADPORT      	(-23)
#define BC_NOEXIST      	(-24)
#define BC_TOOMANY      	(-25)
#define BC_ILLRESET     	(-28)


/* standard errors */
#define	STD_CAPBAD		((errstat) (STD_FIRST_ERR))
#define	STD_COMBAD		((errstat) (STD_FIRST_ERR - 1))
#define	STD_ARGBAD		((errstat) (STD_FIRST_ERR - 2))
#define	STD_NOTNOW		((errstat) (STD_FIRST_ERR - 3))
#define STD_NOSPACE		((errstat) (STD_FIRST_ERR - 4))
#define STD_DENIED		((errstat) (STD_FIRST_ERR - 5))
#define	STD_NOMEM		((errstat) (STD_FIRST_ERR - 6))
#define	STD_EXISTS		((errstat) (STD_FIRST_ERR - 7))
#define	STD_NOTFOUND		((errstat) (STD_FIRST_ERR - 8))
#define	STD_SYSERR		((errstat) (STD_FIRST_ERR - 9))
#define	STD_INTR		((errstat) (STD_FIRST_ERR - 10))
#define	STD_OVERFLOW		((errstat) (STD_FIRST_ERR - 11))
#define	STD_WRITEPROT		((errstat) (STD_FIRST_ERR - 12))
#define	STD_NOMEDIUM		((errstat) (STD_FIRST_ERR - 13))

/* The routine for interpretting the above */
#define	err_why	_err_why

char *	err_why _ARGS((errstat));

#endif /* __STDERR_H__ */
