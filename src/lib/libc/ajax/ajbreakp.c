/*	@(#)ajbreakp.c	1.3	94/04/07 10:22:17 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Break path, ajax variant (special default dir) */

#include "ajax.h"

char *
_ajax_breakpath(dir, path, pcap)
	capability *dir;
	const char *path;
	capability *pcap;
{
	char *name;
	capset csdir, csret;
	int err;
	
	if (_ajax_csorigin(dir, path, &csdir) != STD_OK)
		return NULL;
	name = (char *) path;
	err = sp_traverse(&csdir, (const char **) &name, &csret);
	cs_free(&csdir);
	if (err != STD_OK)
		return NULL;
	(void)cs_to_cap(&csret, pcap);
	cs_free(&csret);
	return name;
}
