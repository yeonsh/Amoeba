/*	@(#)chown.c	1.2	94/04/07 09:42:40 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Posix chown() function */

#include "ajax.h"

int
chown(path, owner, group)
	char *path;
	int owner, group;
{
	capability tmp;
	
	if (name_lookup(path, &tmp) != 0)
		ERR(ENOENT, "chown: path not found");
	if (owner != FAKE_UID || group != FAKE_GID)
		ERR(EPERM, "chown: no permission");
	return 0; /* No-op */
}
