/*	@(#)cwd_set.c	1.4	96/02/27 11:15:53 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/path.h"
#include "module/direct.h"
#include "module/name.h"
#include "module/cap.h"
#include "capset.h"
#include "soap/soap.h"
#define _POSIX_SOURCE
#include <posix/limits.h>	/* For PATH_MAX */

#define ENV_WORK "_WORK"
extern capset _sp_workdir; /* See src/lib/libam/soap/sp_lib.c */

#ifndef UNIX
#include <stdlib.h>
extern int setenv();
extern void unsetenv();
#endif

#ifndef NULL
#define NULL 0
#endif

errstat
cwd_set(name)
char *name;
{
	capability cap, *workcap;
	errstat err;
#ifndef UNIX
	char *workpath, buf[PATH_MAX+1];
#endif
	
	if ((err = sp_init()) != STD_OK)
		return err;
	if ((workcap = dir_origin("")) == 0)
		return STD_SYSERR;

#ifndef UNIX
	/* Ensure that current working directory string is sensible: */
	if ((workpath = getenv(ENV_WORK)) == NULL || *workpath != '/')
		workpath = "/";
	if (name_lookup(workpath, &cap) != STD_OK || !cap_cmp(&cap, workcap))
		workpath = NULL;

	/* Expand "." and ".." and normalize name to an absolute path name: */
	if (path_norm(workpath, name, buf, sizeof buf) == 0)
		name = buf;
#endif

	if ((err = name_lookup(name, &cap)) != STD_OK)
		return err;
	*workcap = cap;
	cs_free(&_sp_workdir);
	(void) cs_singleton(&_sp_workdir, &cap);
#ifndef UNIX
	if (*name == '/')
		(void) setenv(ENV_WORK, name, 1);
	else
		unsetenv(ENV_WORK);
#endif
	return STD_OK;
}
