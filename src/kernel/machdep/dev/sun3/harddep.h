/*	@(#)harddep.h	1.2	94/04/06 09:26:08 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __HARDDEP_H__
#define __HARDDEP_H__

/*
**	the hardware dependent video stuff is in here.
*/

/*
** coords for on screen rectangle
** coordinates for off screen bitmap memory rectangle.
** if there is more than 1 off-screen rectangle then modify "balloc/init.c"
*/

#define	SCR_ORG_X	0
#define	SCR_ORG_Y	0
#define	SCR_COR_X	1152
#define	SCR_COR_Y	900

#ifdef VG150
#	define	OFF_ORG_X	0
#	define	OFF_ORG_Y	1100
#	define	OFF_COR_X	1408
#	define	OFF_COR_Y	1489
#	define	SCREENBASE	(Word *)0
#else
#	include "map.h"
#	define	SCREENBASE	(Word *)BMBASE
#endif

#endif /* __HARDDEP_H__ */
