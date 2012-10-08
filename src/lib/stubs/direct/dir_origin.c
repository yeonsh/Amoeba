/*	@(#)dir_origin.c	1.4	96/02/27 11:14:35 */
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

capability *cap_root, *cap_work;

capability *
dir_origin(name)
#ifdef __STDC__
const char *name;
#else
char *name;
#endif
{
	static capability local_root, local_work;

	if (*name == '/') {
		if (cap_root == 0 && (cap_root = getcap("ROOT")) == 0)
			if (dir_root(&local_root) != STD_OK)
				return 0;
			else
				cap_root = &local_root;
		return cap_root;
	}
	else {
		if (cap_work == 0 && (cap_work = getcap("WORK")) == 0)
			if (dir_root(&local_work) != STD_OK)
				return 0;
			else
				cap_work = &local_work;
		return cap_work;
	}
}
