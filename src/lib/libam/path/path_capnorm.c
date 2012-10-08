/*	@(#)path_capnorm.c	1.5	96/02/27 11:02:10 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* Part of the path-processing module */

#include <amoeba.h>
#include <stderr.h>
#include <capset.h>
#include <module/path.h>
#include <module/direct.h>
#include <module/cap.h>
#include <string.h>
#include <stdlib.h>
#include <soap/soap.h>

#ifndef NULL
#define NULL 0
#endif

extern capability *cap_root, *cap_work;		/* defined in dir_origin.c */

extern capset _sp_rootdir;
extern capset _sp_workdir;

/*
 * Similar to path_rnorm(), except that it uses a capability pointer instead
 * of a cwd string.  See path.h for details.
 */
int
path_capnorm(dircap_p, path, buf, len)
	capability **dircap_p;
	char *path;
	char *buf;
	int len;
{
	capability *dircap = *dircap_p;
	int err;
	char *workpath = NULL;

	if (!cap_work)
		(void)dir_origin("");

	if (*path != '/' && (!dircap || cap_work == dircap ||
				      cap_work && cap_cmp(dircap, cap_work))) {
		if (!workpath)
			workpath = getenv("_WORK");
		if ((err = path_rnorm(workpath, path, buf, len)) < 0)
			/* If error, return unnormalized path */
			strncpy(buf, path, len);
	} else {
		if ((err = path_norm((char *)0, path, buf, len)) < 0)
			/* If error, return unnormalized path */
			strncpy(buf, path, len);
	}

	/* SOAP doesn't understand "" as a dir entry name, so if we
	 * have that, we have to expand it, if possible: */
	if (!*buf && (!dircap || dircap == cap_work)) {
		if (!workpath)
			workpath = getenv("_WORK");
		if (workpath)
			strncpy(buf, workpath, len);
	}

	if (*buf == '/') {
		/* We've now have (or always had) an absolute path,
		 * so change the dircap cap to the root cap: */
		if (!cap_root)
			(void)dir_origin("/");
		dircap = cap_root;
	}

	*dircap_p = dircap;	/* Set dircap output */
	return err;
}


/*
 * Similar to path_rnorm(), except that it uses a pointer to a capability
 * set (capset) instead of a cwd string.  See path.h for details.
 */
path_cs_norm(dircapset_p, path, buf, len)
capset **dircapset_p;
char *path;
char *buf;
int len;
{
	capset *dircapset = *dircapset_p;
	int err;
	char *workpath = NULL;
	static int sp_init_called = 0;

	/* Initialize: */
	if (!cap_work)
		(void)dir_origin("");
	if (!sp_init_called) {
		if (sp_init() != STD_OK) {
			/* If error, return unnormalized path */
			strncpy(buf, path, len);
			return -1;
		}
		sp_init_called = 1;
	}

	if (*path != '/' && (!dircapset || dircapset == &_sp_workdir ||
			       (cap_work && cs_member(dircapset, cap_work)))) {
		if ((err = path_rnorm(workpath = getenv("_WORK"), path, buf,
								     len)) < 0)
			/* If error, return unnormalized path */
			strncpy(buf, path, len);
	} else {
		if ((err = path_norm((char *)0, path, buf, len)) < 0)
			/* If error, return unnormalized path */
			strncpy(buf, path, len);
	}

	/* SOAP doesn't understand "" as a dir entry name, so if we
	 * have that, we have to expand it, if possible: */
	if (!*buf && (!dircapset || dircapset == &_sp_workdir ||
			       (cap_work && cs_member(dircapset, cap_work)))) {
		if (!workpath)
			workpath = getenv("_WORK");
		if (workpath)
			strncpy(buf, workpath, len);
	}

	if (*buf == '/') {
		/* We've now have (or always had) an absolute path,
		 * so change the dircapset to the root capset: */
		dircapset = &_sp_rootdir;
	}

	*dircapset_p = dircapset;	/* Set dircapset output */
	return err;
}
