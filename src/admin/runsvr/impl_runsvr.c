/*	@(#)impl_runsvr.c	1.6	96/02/27 10:17:58 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
Implementation functions for operations supported by the run server.
These functions are called by the AIL-generated server main loop.
Their signature is defined by the AIL class definitions (in
~amoeba/h/class/*.cls), and the translation process from AIL
operations to C functions.  The first parameter is always a header.

NB: Some implementation functions are in findbest.c and pooldir.c.
*/

#include <stdio.h>
#include <stdlib.h>

#include "runsvr.h"
#include "module/buffers.h"
#include "class/stdinfo.h"
#include "objects.h"
#include "runrights.h"
#include "findbest.h"


/* The "std_info" request must be supported by all servers.
   It returns an indentification string for the server.
   (There are some proposals to make this string machine-parsable,
   but for now, it is intended only for the human looking at the output
   of "dir -l".) */

static char *
buf_add_string(buf, end, str)
char *buf, *end, *str;
{
    buf = buf_put_string(buf, end, str);
    if (buf != NULL) buf--;
    return buf;
}

static int
hosts_up(arch)
struct archdir *arch;
/* Return 1 if at least one host in the archdir is up, else 0. */
{
    register struct hostcap *host;

    for (host = arch->ad_host; host != NULL; host = host->hc_next) {
	if (host->hc_info->hi_up) {
	    return 1;
	}
    }
    return 0;
}

/*ARGSUSED*/
errstat
impl_std_info(h, str, max_str, n, len)
header *h;
char *str;
int max_str;
int n;
int *len;
{
    char *bufp, *end;
    object *obj;
    errstat err;

    if (n < 0) {
	return STD_ARGBAD;
    }
    if (n > max_str) {
	/* we cannot pass more than max_str bytes */
	n = max_str;
    }
    err = obj_find(&h->h_priv, OBJ_POOL|OBJ_SUPER, RUN_RGT_STATUS, &obj);
    if (err != STD_OK) {
	return err;
    }
    
    bufp = str;
    end = &str[n];
    if (obj->obj_type == OBJ_POOL) {
	struct archdir *a;

	/* return string of the form "! arch_1 .. arch_n", telling
	 * for which architecure hosts are currently available.
	 */
	bufp = buf_add_string(bufp, end, "!");
	for (a = obj->obj_pool->pd_arch; a != NULL; a = a->ad_next) {
	    if (hosts_up(a)) {
		bufp = buf_add_string(bufp, end, " ");
		bufp = buf_add_string(bufp, end, a->ad_name);
	    }
	}
    } else {
	bufp = buf_add_string(bufp, end, "run server");
    }

    if (bufp == NULL) {
	*len = n;	/* totally filled up */
    } else {
	*len = bufp - str;
    }
    obj_release(obj);
    return STD_OK;
}

errstat
impl_std_restrict(h, mask, newcap)
header 	   *h;
rights_bits mask;
capability *newcap;
{
    errstat err, retval = STD_OK;
    rights_bits rights;
    object *obj;

    /* Find & validate object: */
    err = obj_find(&h->h_priv, OBJ_POOL|OBJ_SUPER, RUN_RGT_NONE, &obj);
    if (err != STD_OK) {
	return err;
    }

    /* retrieve and mask off rights */
    if (prv_decode(&h->h_priv, &rights, &obj->obj_check) < 0) {
	/* should not happen because we have just validated it */
	obj_release(obj);
	return STD_CAPBAD;
    }
    rights &= mask;

    /* build new cap */
    newcap->cap_port = h->h_port;
    if (prv_encode(&newcap->cap_priv, prv_number(&h->h_priv),
		   rights, &obj->obj_check) != 0) {
        retval = STD_SYSERR;      /* Shouldn't happen */
    }

    obj_release(obj);
    return retval;
}


/* The "std_status" request returns an elaborate status message,
 * ready to be written to stdout (including all newlines).
 */

extern char *status_header();
extern char *status_pool();
extern char *status_all();

/*ARGSUSED*/
errstat
impl_std_status(h, buf, max_buf, len, len_ret)
header *h;
char *buf;
int max_buf;
int len;
int *len_ret;
{
    char       *bufp, *end;
    object     *obj;
    errstat	err;

    if (len < 0) {
	return STD_ARGBAD;
    }
    if (len > max_buf) {
	len = max_buf;
    }
    err = obj_find(&h->h_priv, OBJ_POOL|OBJ_SUPER, RUN_RGT_STATUS, &obj);
    if (err != STD_OK) {
	return err;
    }

    memset((_VOIDSTAR) buf, '\0', (size_t)len);
    end = &buf[len-1];
    bufp = buf;

    if (obj->obj_type == OBJ_POOL) {
	/* status of hosts in a particular pool */
	bufp = status_header(bufp, end);
	bufp = status_pool(bufp, end, obj->obj_pool);
    } else {
	/* status of all hosts */
	bufp = status_header(bufp, end);
	bufp = status_all(bufp, end);
    }

    if (bufp == NULL) {
	bufp = strchr(buf, '\0');
    }
    *len_ret = bufp - buf;

    obj_release(obj);
    return STD_OK;
}


/* The "exec_findhost" request returns the capability for the process server
   of a suitable pool processor, and that processor's name (which is
   ignored by the client except for debugging purposes). */


#define MAX_PD_IN_LIST	10 /* 10 architectures should be enough, for a while */

errstat
impl_run_multi_findhost(h, pd_num, pd_buf, max_pd_buf, pd_len,
			pd_which, hostname_ret,	max_hostname_ret, hostcap_ret)
header	   *h;
int         pd_num;
char       *pd_buf;
int	    max_pd_buf;
int         pd_len;
int        *pd_which;
char       *hostname_ret;
int	    max_hostname_ret;
capability *hostcap_ret;
{
    struct hostinfo *best_host;
    errstat 	     err;
    int		     pd_index;
    char	    *pdbufp = pd_buf;
    process_d 	    *pd_list[MAX_PD_IN_LIST];
    int		     pd_in_list = 0;

    if (pd_num < 1 || pd_num > MAX_PD_IN_LIST) {
	return STD_ARGBAD;
    }
    if (pd_len < 0) {
	return STD_ARGBAD;
    }
    if (pd_len > max_pd_buf) {
	/* was truncated */
	pd_len = max_pd_buf;
    }

    err = STD_OK;

    /* get the pd list from the buffer */
    for (pd_index = 0; pd_index < pd_num; pd_index++) {
	process_d *pd;

	pd = pdmalloc(pdbufp, &pd_buf[pd_len]);
	if (pd == NULL) {
	    fprintf(stderr, "could not allocate pd\n");
	    err = STD_NOSPACE;
	    break;
	}

	if ((pdbufp = buf_get_pd(pdbufp, &pd_buf[pd_len], pd)) == NULL) {
	    fprintf(stderr, "could not get pd from buffer\n");
	    free((_VOIDSTAR)pd);
	    err = STD_ARGBAD;
	    break;
	}

	pd_list[pd_in_list++] = pd;
    }

    if (err == STD_OK) {
	/* find best host for one of these pd's */
	err = findbest(&h->h_priv, pd_in_list, pd_list, &best_host, pd_which);
    }

    /* free the pd list */
    for (pd_index = 0; pd_index < pd_in_list; pd_index++) {
	free((_VOIDSTAR)pd_list[pd_index]);
    }

    if (err == STD_OK) {	/* set result values */
	*hostcap_ret = best_host->hi_execcap;
	strncpy(hostname_ret, best_host->hi_name, (size_t) max_hostname_ret);
	upreschedule(best_host);
    }

    return err;
}


errstat
impl_run_get_exec_cap(hdr, proccap, execcap, arch, archsz)
header *	hdr;
capability *	proccap; /* in */
capability *	execcap; /* out */
char *		arch; /* out */
int		archsz; /* in */
{
    errstat		err;
    object *		obj;
    struct pooldir *	pool;
    struct hostcap *	hc;
    struct hostinfo *	h;
    struct archdir *	a;

    if (prv_number(&proccap->cap_priv) != 0) {
	return STD_ARGBAD; /* It isn't a suitable proc svr cap */
    }

    /*
     * 1. make sure that hdr->h_priv refers to a valid pool
     * 2. make sure that proccap is in that pool
     * 3. return the current exec_cap for that proccap
     */

    err = obj_find(&hdr->h_priv, OBJ_POOL, RUN_RGT_FINDHOST, &obj);
    if (err != STD_OK) {
	return err;
    }
#define	RETURN(a) { err = a; goto Done; }

    pool = obj->obj_pool;
    if (!pool->pd_enabled) {
	RETURN(STD_NOTNOW);
    }

    /*
     * FOR each architecture
     *  FOR each host in that architecture
     *   IF (host->proccap == proccap)
     *    return host->execcap
     */
    for (a = pool->pd_arch; a != NULL; a = a->ad_next) {
	for (hc = a->ad_host; hc != NULL; hc = hc->hc_next) {
	    h = hc->hc_info;
	    if (PORTCMP(&h->hi_proccap.cap_port, &proccap->cap_port)) {
		*execcap = h->hi_execcap;
		(void) memcpy(arch, a->ad_name, archsz);
		RETURN(STD_OK);
	    }
	}
    }

    RETURN(STD_ARGBAD);

#undef RETURN

Done:
    obj_release(obj);
    return err;
}


/* Get the load, available cycles and bytes.
   Simply add the load, cycles and bytes of all processors in the given
   pool that are up. */

errstat
impl_pro_1getload(h, kips, loadav, mfree)
	header *h;
	int *kips;
	int *loadav;
	long *mfree;
{
	struct archdir *a;
	struct hostcap *hc;
	struct hostinfo *p;
	long cpuspeed, runnable, freebytes;
	object *pool_obj;
	errstat err;

	err = obj_find(&h->h_priv, OBJ_POOL, RUN_RGT_STATUS, &pool_obj);
	if (err != STD_OK) {
		MON_ERROR("pro_getload bad capability");
		return err;
	}

	cpuspeed = runnable = freebytes = 0;
	for (a = pool_obj->obj_pool->pd_arch; a != NULL; a = a->ad_next) {
	    for (hc = a->ad_host; hc != NULL; hc = hc->hc_next) {
		p = hc->hc_info;
		if (p->hi_up) {
			cpuspeed += p->hi_cpuspeed;
			runnable += p->hi_runnable;
			freebytes += p->hi_freebytes;
		}
	    }
	}
	*kips = cpuspeed / 1024;
	*loadav = runnable;
	*mfree = freebytes;

	obj_release(pool_obj);
	return STD_OK;
}
