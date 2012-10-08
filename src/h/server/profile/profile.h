/*	@(#)profile.h	1.2	94/04/06 17:05:10 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __SERVER__PROFILE__PROFILE_H__
#define __SERVER__PROFILE__PROFILE_H__

/*
 *	Commands specific to the profile server
 */

#define	PROFILE_CMD		(PROFILE_FIRST_COM + 0)
#if RPC_PROFILE
#define	PROFILE_RPC		(PROFILE_FIRST_COM + 1)
#define	PROFILE_RPC_START	(PROFILE_FIRST_COM + 2)
#define	PROFILE_RPC_STOP	(PROFILE_FIRST_COM + 3)
#endif /* RPC_PROFILE */

/*
 *	Rights bits for the profile server
 */

#define	PROFILE_RGT_ALL		0xFF
#define PROFILE_RGT		0x1

#endif /* __SERVER__PROFILE__PROFILE_H__ */
