/*	@(#)profile.h	1.2	94/04/06 17:18:47 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
profile.h

Created:	Aug 5, 1992 by Philip Homburg
*/

#ifndef _SYS__PROFILE_H
#define _SYS__PROFILE_H

void profile_rpc _ARGS(( int prc, int thr, port *reqport, objnum reqobj, command reqcmd, 
	bufsize reqsiz, bufsize replsiz, unsigned long reqtim, unsigned long repltim ));

#endif /* _SYS__PROFILE_H */
