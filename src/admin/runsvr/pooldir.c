/*	@(#)pooldir.c	1.5	96/02/27 10:18:10 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Pooldir data structure management */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "amoeba.h"
#include "ampolicy.h"
#include "capset.h"
#include "server/soap/soap.h"
#include "sp_dir.h"

#include "runsvr.h"
#include "objects.h"
#include "runrights.h"
#include "findbest.h"
#include "pool_dir.h"

extern char *strdup();

/*
 * We take a 5 second timeout while scanning a pooldir,  to avoid hanging
 * a long period on an unavailable directory during initialisation
 * (or on an invalid directory in case someone tries to add a bad one).
 * Note that for real security we also need service timeouts.
 */
#define SCAN_TIMEOUT	((interval)5000)

/* Forward: */
static errstat fillarchdir();
static errstat scan_archdir();

static struct pooldir *pools = NULL;	/* List of all managed pooldirs */

/* Create a new pooldir object */

static struct pooldir *
newpooldir()
{
	return (struct pooldir *) calloc(1, sizeof(struct pooldir));
}


/* Create a new archdir object */

static struct archdir *
newarchdir(name)
	char *name;
{
	struct archdir *a =
		(struct archdir *) calloc(1, sizeof(struct archdir));

	if (a == NULL) {
	    return NULL;
	}
	if ((a->ad_name = strdup(name)) == NULL) {
	    free((_VOIDSTAR) a);
	    return NULL;
	}
	return a;
}


/* Create a new hostcap object */

static struct hostcap *
newhostcap(name)
	char *name;
{
	struct hostcap *h =
		(struct hostcap *) calloc(1, sizeof(struct hostcap));
	if (h == NULL)
		return NULL;
	if ((h->hc_name = strdup(name)) == NULL) {
		free((_VOIDSTAR) h);
		return NULL;
	}
	return h;
}


/* Destroy a hostcap object */

static void
delhostcap(h)
	struct hostcap *h;
{
	if (h->hc_name != NULL)
		free((_VOIDSTAR) h->hc_name);
	free((_VOIDSTAR) h);
}


/* Destroy the list of hostcaps of a given archdir */

static void
purgearchdir(a)
	struct archdir *a;
{
	struct hostcap *h;
	
	while ((h = a->ad_host) != NULL) {
		a->ad_host = h->hc_next;
		delhostcap(h);
	}
}


/* Destroy an archdir object */

static void
delarchdir(a)
	struct archdir *a;
{
	purgearchdir(a);
	if (a->ad_name != NULL)
		free((_VOIDSTAR) a->ad_name);
	free((_VOIDSTAR) a);
}


/* Destroy the list of archdirs of a given pooldir */

static void
purgepooldir(p)
	struct pooldir *p;
{
	struct archdir *a;
	
	while ((a = p->pd_arch) != NULL) {
		p->pd_arch = a->ad_next;
		delarchdir(a);
	}
}


/* Destroy a pooldir object */

void
del_pooldir(p)
	struct pooldir *p;
{
	purgepooldir(p);
	free((_VOIDSTAR) p);
}


/* Create a new hostinfo object */

static struct hostinfo *
newhostinfo(name, cap)
	char *name;
	capability *cap;
{
	struct hostinfo *p =
		(struct hostinfo *)malloc(sizeof(struct hostinfo));
	
	if (p == NULL)
		return NULL;
	
	mu_init(&p->hi_mu);
	p->hi_up = 0;
	p->hi_hostcap = *cap;
	(void) strncpy(p->hi_name, name, PD_HOSTSIZE);
	p->hi_name[PD_HOSTSIZE-1] = '\0'; /* Ensure name is null-terminated */
	p->hi_lastflip = sys_milli();
	p->hi_lastpoll = 0;
	p->hi_lastrepl = 0;
	p->hi_npolls = 0;
	p->hi_nbest = 0;
	p->hi_nconsidered = 0;
	return p;
}

/* Add a directory to the list of managed pool directories */

static errstat
pool_rescan(p)
struct pooldir *p;
{
    capset 	      dir_cs;
    SP_DIR	     *dd;
    struct sp_direct *d;
    errstat 	      err;
    struct archdir   *new_archlist = NULL;

    if (cs_singleton(&dir_cs, &p->pd_dir) != 1) {
	fprintf(stderr, "cs_singleton failure for pool directory\n");
	return STD_NOSPACE;
    }

    if ((err = sp_list(&dir_cs, &dd)) != STD_OK) {
	fprintf(stderr, "cannot list pooldir (%s)\n", err_why(err));
	cs_free(&dir_cs);
	return err;
    }

    /* rescan arch dirs in this pooldir */
    while ((d = sp_readdir(dd)) != NULL) {
	if (pd_known_arch(d->d_name)) {
	    struct archdir *arch, *old_arch;

	    err = scan_archdir(&dir_cs, d->d_name, &arch);
	    if (err == STD_OK) {
		old_arch = find_pool_arch(p, d->d_name);
		if (old_arch != NULL) { /* use the previous arch params */
		    arch->ad_params = old_arch->ad_params;
		} else {		/* initialise default arch params */
		    initarchparams(arch);
		}

		/* add it to new archdir list */
		arch->ad_next = new_archlist;
		new_archlist = arch;
	    }
	}
    }
    sp_closedir(dd);
    cs_free(&dir_cs);

    /* switch to new archdir list */
    purgepooldir(p);
    p->pd_arch = new_archlist;

    if (p->pd_arch == NULL) {
	if (debugging) {
	    fprintf(stderr, "%s: empty pooldir.\n", progname);
	}
    }

    return STD_OK;
}

errstat
add_pooldir(dir_cap, pp)
capability *dir_cap;
struct pooldir **pp;
{
    struct pooldir *p;
    interval old_tout;
    errstat err;

    if ((p = newpooldir()) == NULL) {
	return STD_NOSPACE;
    }

    p->pd_dir = *dir_cap;

    old_tout = timeout(SCAN_TIMEOUT);
    err = pool_rescan(p);
    (void) timeout(old_tout);

    if (err != STD_OK) {
	/* e.g. because dir_cap is not a directory */
	del_pooldir(p);
	return err;
    }

    initpoolparams(p);

    /* success: add it to the pools list */
    p->pd_next = pools;
    pools = p;

    *pp = p;
    return STD_OK;
}


errstat
impl_run_create(h, dir_cap, poolcap_ret)
header *h;
capability *dir_cap;
capability *poolcap_ret;
{
    object	   *super_obj = NULL;
    errstat	    err;
    capability	    poolcap;
    struct pooldir *pool = NULL;

    err = obj_find(&h->h_priv, OBJ_SUPER, RUN_RGT_CREATE, &super_obj);
    if (err != STD_OK) {
	return err;
    }

    /* we have the super object locked, so there's no need for extra mutexes */
    err = add_pooldir(dir_cap, &pool);

    /* release it before we atomically create a new object */
    obj_release(super_obj);

    if (err != STD_OK) {
	return err;
    }

    if ((err = obj_new(&poolcap, OBJ_POOL, pool)) != STD_OK) {
	del_pooldir(pool);
	return err;
    } 

    /* All's well; save the new configuration to disk */
    refresh_adminfile();

    *poolcap_ret = poolcap;    /* client has to install this in a directory. */
    return STD_OK;
}

/* (re)scan an architecture directory */

static errstat
scan_archdir(csp, arch_name, archp)
capset *csp;
char *arch_name;
struct archdir **archp;
{
    capset cs;
    struct archdir *a;
    errstat err;
	
    if ((err = sp_lookup(csp, arch_name, &cs)) != STD_OK) {
	error("lookup failed for archdir '%s' (%s)", arch_name, err_why(err));
	return err;
    }

    if (debugging)
	fprintf(stderr, "%s: processing archdir '%s' ...\n",
		progname, arch_name);
	
    if ((a = newarchdir(arch_name)) == NULL) {
	return STD_NOSPACE;
    }

    err = fillarchdir(a, &cs);
    cs_free(&cs);
    if (err != STD_OK) {
	error("could not add hosts of archdir '%s' (%s)",
	      arch_name, err_why(err));
	delarchdir(a);
	return err;
    }

    if (a->ad_host == NULL) {
	if (debugging) {
	    fprintf(stderr, "%s: empty archdir '%s' (that's ok).\n",
		    progname, arch_name);
	}
    }

    *archp = a;
    return STD_OK;
}

/* Forward: */
static errstat addhost();

/* Read the hosts of an archdir into the archdir data structure */
	        
static errstat
fillarchdir(a, cs)
	struct archdir *a;
	capset *cs;
{
	errstat err;
	SP_DIR *dd;
	struct sp_direct *d;
	
	if ((err = sp_list(cs, &dd)) != STD_OK)
		return err;

	err = STD_OK;	/* set in loop when some host cannot be added */
	while ((d = sp_readdir(dd)) != NULL) {
		/* skip old run server capabilities (they were published
		 * in arch dirs):
		 */
		if (strchr(d->d_name, '.') != NULL)
			continue;
		if ((err = addhost(a, cs, d->d_name)) != STD_OK)
			break;
	}
	sp_closedir(dd);
	cs_free(cs);
	return err;
}


/* Add a host subdirectory to a pooldir struct */

static errstat
addhost(a, csp, name)
	struct archdir *a;
	capset *csp;
	char *name;
{
	struct hostcap *h;
	capset cs;
	capability cap;
	errstat err;
	struct hostinfo *p;
	
	if ((err = sp_lookup(csp, name, &cs)) != STD_OK) {
		return err;
	}
	err = cs_to_cap(&cs, &cap);
	cs_free(&cs);
	if (err != STD_OK) {
		return err;
	}
	
	for (p = First_host; p != NULL; p = p->hi_next) {
		if (strcmp(p->hi_name, name) == 0) {
			if (!PORTCMP(&p->hi_hostcap.cap_port, &cap.cap_port)){
				fprintf(stderr,
				  "%s: WARNING: multiple hosts named '%s'\n",
				  progname, name);
			}
			else
				break;
		}
		else if (PORTCMP(&p->hi_hostcap.cap_port, &cap.cap_port)) {
			fprintf(stderr,
			  "%s: WARNING: host '%s' already known as '%s'\n",
			  progname, name, p->hi_name);
			break;
		}
	}
	if (p == NULL) {
		if (debugging)
			fprintf(stderr, "%s: new host '%s'\n", progname, name);
		if ((p = newhostinfo(name, &cap)) == NULL)
			return STD_NOSPACE;
		p->hi_next = First_host;
		First_host = p;
		adddownpoll(p, (interval)0); /* Poll it a.s.a.p. */
	}
	else {
		rights_bits oldrights, newrights;

		if (debugging)
		    fprintf(stderr, "%s: known host '%s'\n", progname, name);

		/* Make sure we keep the host cap with the most rights! */
		oldrights = p->hi_hostcap.cap_priv.prv_rights & PRV_ALL_RIGHTS;
		newrights = cap.cap_priv.prv_rights & PRV_ALL_RIGHTS;
		if ((newrights & oldrights) == oldrights &&
		    (newrights & ~oldrights) != 0)
		{
		     /* It has more rights, but before switching over to it,
		      * check that it is valid.
		      */
		    char infobuf[16];
		    int len;

		    err = std_info(&cap, infobuf, sizeof(infobuf), &len);
		    if (err == STD_OK || err == STD_OVERFLOW) {
			p->hi_hostcap = cap;
			if (debugging) {
			    fprintf(stderr, "%s: more rights for %s (%s)\n",
				    progname, name, err_why(err));
			}
			/* Cause runsvr to refetch the proc cap since it
			 * may now also have more rights.
			 */
			updownflip(p);
		    } else {
			if (debugging) {
			    fprintf(stderr, "%s: ignore new cap for %s (%s)\n",
				    progname, name, err_why(err));
			}
		    }
		}
	}
	if ((h = newhostcap(name)) == NULL)
		return STD_NOSPACE;
	h->hc_info = p;
	h->hc_next = a->ad_host;
	a->ad_host = h;
	return STD_OK;
}


struct archdir *
find_pool_arch(pool, arch)
struct pooldir *pool;
char *arch;
/* find an archdir within a pooldir */
{
    struct archdir *a;
	
    for (a = pool->pd_arch; a != NULL; a = a->ad_next) {
	if (strcmp(a->ad_name, arch) == 0) {
	    return a;
	}
    }

    /* not found */
    return NULL;
}

/* The "std_touch" request rescans the contents of the given
 * pooldir/archdir.
 */

errstat
impl_std_touch(h)
header *h;
{
    object   *pool_obj;
    interval  old_tout;
    errstat   err;
	
    err = obj_find(&h->h_priv, OBJ_POOL, RUN_RGT_ADMIN, &pool_obj);
    if (err != STD_OK) {
	return err;
    }

    /* recan all archdirs in this pool dir */
    old_tout = timeout(SCAN_TIMEOUT);
    err = pool_rescan(pool_obj->obj_pool);
    (void) timeout(old_tout);

    obj_release(pool_obj);
    return err;
}

/* The "std_destroy" request invalidates & deletes an existing run object. */
errstat
impl_std_destroy(h)
header *h;
{
    object *pool_obj;
    errstat err;

    obj_disallow_mods();	/* get global object lock */
    err = obj_find(&h->h_priv, OBJ_POOL, RUN_RGT_DESTROY, &pool_obj);

    if (err != STD_OK) {
	obj_allow_mods();
	return err;
    }

    del_pooldir(pool_obj->obj_pool);
    obj_delete(&pool_obj);

    /* other modifications/global operations allowed again: */
    obj_allow_mods();

    refresh_adminfile();
    return STD_OK;
}
