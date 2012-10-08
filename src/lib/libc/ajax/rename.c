/*	@(#)rename.c	1.6	96/02/27 11:06:06 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Posix rename() function using SOAP primitives */

#include "ajax.h"
#include "sp_dir.h"
#include "module/cap.h"

/* Flags to sp_rename, or-ed together */
#define SP_MOVECHECK (1<<0)
#define SP_DESTPURGE (1<<1)

/* Forward */
static int setent();
static int movecheck();
static int sp_rename();


int
rename(src, dst)
	char *src, *dst;
{
	int err;
	
	err = sp_rename(src, dst, SP_MOVECHECK);
	if (err != STD_OK)
		ERR(_ajax_error(err), "rename failed");
	return 0;
}

static int
sp_rename(src, dst, flags)
	char *src;
	char *dst;
	int flags;
{
	int err, i;
	sp_entry ent[2];
	sp_result res[2];
	capset cs[2];
	capability * cp[2];
	capability cap[2];
	char * s;
	char * d;
	
	if ((err = setent(&ent[0], src)) != STD_OK)
		goto cleanup0;
	if ((err = setent(&ent[1], dst)) != STD_OK)
		goto cleanup1;
	if ((err = sp_setlookup(2, ent, res)) != STD_OK)
		goto cleanup2;
	if (res[0].sr_status != STD_OK) {
		err = res[0].sr_status;
		goto cleanup3;
	}

	/*
	 * Prevent moving dir to subdir of itself.  In fact in Amoeba this
	 * is ok as long as there is another path to the dir. POSIX
	 * actually says "The dst pathname shall not contain a path prefix
	 * that contains src".  That is exactly right for Amoeba.
	 */
	s = src;
	d = dst;
	while (*d == *s && *d != '\0' && *s != '\0') {
	    d++;
	    s++;
	}
	if (*s == '\0' && (*d == '\0' || *d == '/')) {
	    err = STD_ARGBAD;
	    goto cleanup3;
	}
	
	if (res[1].sr_status == STD_OK) { /* Destination exists */
		static capability nullcap;
		
		if (flags & SP_MOVECHECK)
			if ((err = movecheck(res)) != STD_OK)
				goto cleanup3;
		
		if (!cs_singleton(&cs[0], &nullcap)) {
			err = STD_NOMEM;
			goto cleanup3;
		}
		if (!cs_copy(&cs[1], &res[0].sr_capset)) {
			cs_free(&cs[0]);
			goto cleanup3;
		}
		for (i = 0; i < 2; i++) {
			cp[i] = &cap[i];
			(void)cs_to_cap(&res[i].sr_capset, cp[i]);
		}
		/*
		 * POSIX says that if src and dest refer to links to the
		 * same file we do nothing.  Even if the links don't have
		 * the same name!  Who are we to argue??
		 */
		if (cap_cmp(cp[0], cp[1]) == 1)
		    return STD_OK;

		err = sp_install(2, ent, cs, cp);
		cs_free(&cs[0]);
		cs_free(&cs[1]);
		if (err == STD_OK) {
			/* The sp_delete() below can't be denied
			   (otherwise the sp_install() above would have
			   failed); other errors we don't know how to
			   handle (probably network or server failures). */
			(void) sp_delete(&ent[0].se_capset, ent[0].se_name);
			/* The cs_purge() is optional; if not all copies
			   of the object can't be destroyed, garbage
			   collection will take care of the rest. */
			if (flags & SP_DESTPURGE)
				(void)cs_purge(&res[1].sr_capset);
		}
	}
	else { /* Destination doesn't exist yet */
		long cols[SP_MAXCOLUMNS];

		/* destination should get same protection as source */
		err = sp_getmasks(SP_DEFAULT, src, SP_MAXCOLUMNS, cols);
		if (err == STD_OK) {
		    err = sp_append(&ent[1].se_capset, ent[1].se_name,
				    &res[0].sr_capset, SP_MAXCOLUMNS, cols);
		    if (err == STD_OK) {
			err = sp_delete(&ent[0].se_capset, ent[0].se_name);
			if (err != STD_OK && err != RPC_FAILURE) {
				/* Undo the sp_append() if possible */
				(void) sp_delete(&ent[1].se_capset,
							ent[1].se_name);
			}
		    }
		}
	}
	
cleanup3:
	if (res[1].sr_status == STD_OK)
		cs_free(&res[1].sr_capset);
	if (res[0].sr_status == STD_OK)
		cs_free(&res[0].sr_capset);
cleanup2:
	cs_free(&ent[1].se_capset);
cleanup1:
	cs_free(&ent[0].se_capset);
cleanup0:
	return err;
}

/* Fill an entry with info that is palatable to sp_setlookup */

static int
setent(ent, name)
	sp_entry *ent;
	char *name;
{
	ent->se_name = name;
	return sp_traverse(SP_DEFAULT, (const char **) &ent->se_name,
			   &ent->se_capset);
}

/* Enforce the (BSD, Posix) rule that rename() to an existing target is
   only allowed if the types (file or directory) are the same, and if
   the destination is a directory, it must be empty. */

static int
movecheck(res)
	sp_result res[];
{
	capset cs;
	int srcisdir;
	SP_DIR *dd;
	int err;
	
	/* Is src a directory? */
	err = sp_lookup(&res[0].sr_capset, "x", &cs);
	srcisdir = (err == STD_OK || err == STD_NOTFOUND);
	if (err == STD_OK)
		cs_free(&cs);
	
	if (sp_list(&res[1].sr_capset, &dd) != STD_OK) {
		if (srcisdir)
			return STD_DENIED;
		else
			return STD_OK;
	}
	else {
		if (!srcisdir)
			err = STD_DENIED;
		else if (sp_readdir(dd) != NULL)
			err = SP_NOTEMPTY;
		else
			err = STD_OK;
		sp_closedir(dd);
		return err;
	}
}
