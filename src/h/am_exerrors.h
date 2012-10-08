/*	@(#)am_exerrors.h	1.2	94/04/06 15:38:02 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __AM_EXERRORS_H__
#define __AM_EXERRORS_H__

/* Errors that exec_file() and exec_pd() can return besides STD errors */

/* NB: AM_EXEC_FIRST_ERR happens to be -1899 so these start at -1900... */

#define AMEX_NOPROG	(AM_EXEC_FIRST_ERR - 1)
#define AMEX_PDLOOKUP	(AM_EXEC_FIRST_ERR - 2)
#define AMEX_PDREAD	(AM_EXEC_FIRST_ERR - 3)
#define AMEX_PDSHORT	(AM_EXEC_FIRST_ERR - 4)
#define AMEX_PDSIZE	(AM_EXEC_FIRST_ERR - 5)
#define AMEX_NOCAPS	(AM_EXEC_FIRST_ERR - 6)
#define AMEX_NOHOST	(AM_EXEC_FIRST_ERR - 7)
#define AMEX_GETDEF	(AM_EXEC_FIRST_ERR - 8)
#define AMEX_MALLOC	(AM_EXEC_FIRST_ERR - 9)
#define AMEX_STACKOV	(AM_EXEC_FIRST_ERR - 10)
#define AMEX_SEGCREATE	(AM_EXEC_FIRST_ERR - 11)
#define AMEX_SEGWRITE	(AM_EXEC_FIRST_ERR - 12)

#endif /* __AM_EXERRORS_H__ */
