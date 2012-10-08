/*	@(#)name_create.c	1.2	94/04/07 11:05:28 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/direct.h"
#include "module/name.h"
#include "module/stdcmd.h"

errstat
name_create(name)
char *name;
{
	capability cap, *origin;
	register n;

	if ((origin = dir_origin(name)) == 0)
		return STD_SYSERR;
	if ((n = dir_create(origin, &cap)) != STD_OK)
		return n;
	if ((n = dir_append(origin, name, &cap)) != STD_OK)
		(void) std_destroy(&cap);
	return n;
}
