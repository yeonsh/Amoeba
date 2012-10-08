/*	@(#)copy.h	1.2	94/04/06 17:22:38 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __COPY_H__
#define __COPY_H__

/*
**	valid types of copy command
*/

#define	SCREEN		((capability *) 0)
#define	NULLTEXT	((Texture *) 0)

#define	VIDEO_TO_VIDEO	0
#define	VIDEO_TO_USER	1
#define	USER_TO_VIDEO	2

/*
**	Bitblt operators
*/

#define	D_ZEROS		0
#define	S_AND_D		1
#define	S_AND_NOT_D	2
#define	SOURCE		3
#define	NOT_S_AND_D	4
#define	DEST		5	/* noop ? */
#define	S_XOR_D		6
#define	S_OR_D		7
#define	NOT_S_AND_NOT_D	8
#define	NOT_S_XOR_D	9
#define	NOT_D		10
#define	S_OR_NOT_D	11
#define	NOT_S		12
#define	NOT_S_OR_D	13
#define	NOT_S_OR_NOT_D	14
#define	D_ONES		15

#endif /* __COPY_H__ */
