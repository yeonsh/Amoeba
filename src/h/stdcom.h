/*	@(#)stdcom.h	1.4	94/04/06 15:46:03 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __STDCOM_H__
#define __STDCOM_H__

/*
** stdcom.h	(include cmdreg.h)
**
**	This file contains the standard commands that all servers should
**	implement (where they are relevant).
**	NB: the maximum command number is STD_LAST_COM.
**	If you wish to register a new standard command please mail your
**	request to:
**		amoeba-support@cs.vu.nl
**
** Author:
**	Greg Sharp
*/

#define STD_MONITOR	(STD_FIRST_COM)		/* monitoring		  */
#define	STD_AGE		(STD_FIRST_COM+1)	/* age object for g.c.	  */
#define	STD_COPY	(STD_FIRST_COM+2)	/* replicate object	  */
#define STD_DESTROY	(STD_FIRST_COM+3)	/* destroy object	  */
#define STD_INFO	(STD_FIRST_COM+4)	/* for ``ls -l''	  */
#define STD_RESTRICT	(STD_FIRST_COM+5)	/* restrict rights	  */
#define STD_STATUS	(STD_FIRST_COM+6)	/* get server status info */
#define STD_TOUCH	(STD_FIRST_COM+7)	/* object is used	  */
#define STD_GETPARAMS	(STD_FIRST_COM+8)	/* get server parameters  */
#define STD_SETPARAMS	(STD_FIRST_COM+9)	/* set server parameters  */
#define STD_NTOUCH	(STD_FIRST_COM+10)	/* object is used	  */

#endif /* __STDCOM_H__ */
