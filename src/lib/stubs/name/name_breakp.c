/*	@(#)name_breakp.c	1.2	94/04/07 11:05:22 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* Break path at last component, returning pointer to last component and
 * capability for directory up to last component: */

#include "amoeba.h"
#include "module/direct.h"
#include "module/name.h"


char *
name_breakpath(path, pcap)
	char *path;
	capability *pcap;
{
	return dir_breakpath((capability *)0, path, pcap);
}
