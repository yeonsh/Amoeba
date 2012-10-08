/*	@(#)marsh.c	1.5	94/04/06 11:39:48 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "ailamoeba.h"
#include "server/boot/bsvd.h"
#include "svr.h"
#include "module/stdcmd.h"

/*
 *	Marshaling routines for what comes on vdisk:
 *	null terminated strings and capabilities.
 */

static bufptr
Put_str(bufp, end, str)
    bufptr bufp, end;
    char *str;
{
    if (bufp == NULL) return NULL;
    while (bufp < end)
	if ((*bufp++ = *str++) == '\0')
	    return bufp;
    return NULL;
} /* Put_str */

static bufptr
Put_cap(bufp, end, capp)
    bufptr bufp, end;
    capability *capp;
{
    if (bufp == NULL) return NULL;
    if (end - bufp >= sizeof(capability)) {
	*((capability *) bufp) = *capp;
	return bufp + sizeof(capability);
    }
    return NULL;
} /* Put_cap */

static bufptr
Get_str(bufp, end, s, max)
    bufptr bufp, end;
    char *s;
    int max;	/* #bytes reserved for the string */
{
    /* Fail if the name is empty - this is our usual way out */
    if (bufp == NULL || *bufp == '\0') return NULL;
    while (bufp < end && --max > 0)
	if ((*s++ = *bufp++) == '\0')
	    return bufp;
    return NULL;
} /* Get_str */

static bufptr
Get_cap(bufp, end, capp)
    bufptr bufp, end;
    capability *capp;
{
    if (bufp == NULL) return NULL;
    if (end - bufp < sizeof(capability)) return NULL;
    *capp = *(capability *)bufp;
    return bufp + sizeof(capability);
} /* Get_cap */

int
Size_data(objs, n)
    obj_rep *objs;
    int n;
{
    int size;
    size = 0;
    while (n-- > 0) {
	mu_lock(&objs->or_lock);
	if (obj_in_use(objs))
	    size += strlen(objs->or.or_data.name) + 1 + 2 * sizeof(capability);
	MU_CHECK(objs->or_lock);
	mu_unlock(&objs->or_lock);
	++objs;
    }
    return size;
} /* Size_data */

bool
Put_data(bufp, end, objs, n)
    bufptr bufp, end;
    obj_rep *objs;
    int n;		/* # entries in objs array */
{
    while (n-- > 0) {
	mu_lock(&objs->or_lock);
	if (obj_in_use(objs)) {
	    bufp = Put_str(bufp, end, objs->or.or_data.name);
	    bufp = Put_cap(bufp, end, &objs->or.or_objcap);
	    bufp = Put_cap(bufp, end, &objs->or.or_proccap);
	}
	MU_CHECK(objs->or_lock);
	mu_unlock(&objs->or_lock);
	++objs;
	if (bufp == NULL) {
	    if (debugging) prf("%nmarshaling error Put_data()\n");
	    return 0;	/* Fail */
	}
    }
    return 1;
} /* Put_data */

void
Get_data(bufp, end, objs, sz_objs)
    bufptr bufp, end;
    obj_rep objs[];
    int sz_objs;		/* # entries in objs array */
{
    ident name;
    capability obj, proc;
    errstat err;
    int ind;
    interval old_to;

    old_to = timeout((interval) 2000);
    while (bufp != NULL && bufp < end) {
	bufp = Get_str(bufp, end, name, sizeof(name));
	bufp = Get_cap(bufp, end, &obj);
	bufp = Get_cap(bufp, end, &proc);
	if (bufp != NULL) {
	    ind = FindName(name);
	    if (ind >= 0 && ind < sz_objs) {
		mu_lock(&objs[ind].or_lock);
		if (strcmp(name, objs[ind].or.or_data.name)) {
		    prf("%nGet_data(): name %s found incorrect at %d\n",
								name, ind);
		    abort();
		}
		/* See if the process capability is usable: */
		switch (err = std_touch(&proc)) {
		case STD_OK:
		case RPC_NOTFOUND:
		case RPC_FAILURE:
		case STD_COMBAD:
		    if (debugging)
			prf("Found proccap for %s (%s)\n", name, err_why(err));
		    objs[ind].or.or_proccap = proc;
		    objs[ind].or.or_status |= ST_GOTPROC;
		    break;
		default:
		    if (debugging)
			prf("touch(proccap of %s): %s\n", name, err_why(err));
		}
		objs[ind].or.or_objcap = obj;
		MU_CHECK(objs[ind].or_lock);
		mu_unlock(&objs[ind].or_lock);
	    }
	}
    }
    (void) timeout(old_to);
} /* Get_data */
