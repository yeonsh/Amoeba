/*	@(#)name_lookup.c	1.2	94/04/07 11:05:39 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "module/direct.h"
#include "module/name.h"

errstat
name_lookup(name, object)
char *name;
capability *object;
{
	return dir_lookup((capability *)0, name, object);
}
