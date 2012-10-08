/*	@(#)name_replace.c	1.2	94/04/07 11:05:54 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "module/direct.h"
#include "module/name.h"

errstat
name_replace(name, object)
char *name;
capability *object;
{
	return dir_replace((capability *)0, name, object);
}
