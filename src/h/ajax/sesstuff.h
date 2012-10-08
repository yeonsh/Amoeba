/*	@(#)sesstuff.h	1.2	94/04/06 15:47:43 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __SESSTUFF_H__
#define __SESSTUFF_H__

#include "sessvr.h"

extern int _ajax_sesinited; /* True if session server known */
extern capability _ajax_session; /* Session server capability (see SESINIT) */

#define SESINIT(session) SESINIT2(session, -1)
#define SESINIT2(session, errval) { \
	if (!_ajax_sesinited && _ajax_sesinit() < 0) \
		ERR2(EIO, "SESINIT: sesinit failed", errval); \
	session = &_ajax_session; \
}

#endif /* __SESSTUFF_H__ */
