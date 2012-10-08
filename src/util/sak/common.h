/*	@(#)common.h	1.3	96/02/27 13:11:53 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#define	FALSE		0
#define	TRUE		1

#define	LINESIZE	256

#define	DEF_PATH	"/bin:/profile/util"
#define	DEF_SHELL	"/bin/sh"
#define	DEF_ROOT	"/"
#define	DEF_WORK	DEF_ROOT

#define	DEFAULT_SESSION_SVR	"/profile/util/session"
#define	SESSION_ARGS		"-a"


char * memalloc _ARGS((size_t));
void addcapenv _ARGS((char *, capability *));
void add2capenv _ARGS((char *));
void addstrenv _ARGS((char *, char *));
void add2strenv _ARGS((char *));
void empty_strenv _ARGS((void));

#endif /* __COMMON_H__ */
