/*	@(#)Xamoeba.h	1.3	96/02/27 10:38:47 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "cmdreg.h"
#include "_ARGS.h"

#define		MAXAMBUFSIZE	30000		/* limit on amoeba packets */

/*
**	Command codes for transactions
*/
#define		AX_CONNECT	(X_FIRST_COM+1)	/* Open new connection */
#define		AX_REINIT	(X_FIRST_COM+2)	/* terminate session */
#define		AX_SHUTDOWN	(X_FIRST_COM+3)	/* Terminate X server */

/*
**	Return codes for connection request
*/
#define		AX_OK		0
#define		AX_NOCONN	(X_FIRST_ERR)
#define		AX_BADAUTH	(X_FIRST_ERR-1)
#define		AX_FULLHOUSE	(X_FIRST_ERR-2)

#define	x11_reinit	_x11_reinit
#define	x11_shutdown	_x11_shutdown

errstat x11_reinit _ARGS(( capability *cap ));
errstat x11_shutdown _ARGS(( capability *cap ));
