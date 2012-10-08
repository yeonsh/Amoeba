/*	@(#)findbest.c	1.6	96/02/27 10:17:50 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
The decision which processor to use is a complicated one.  We might
want to take the following in account: available memory; memory
fragmentation (especially since the current version of  Amoeba
can't move segments); whether the text segment is cached in the
processor.  The most interesting bit of information is unfortunately
unavailable: what is the process going to do...  Since load balancing
is an art by itself, (in fact the subject of doctoral dissertations),
we use a simple algorithm initially but make sure that it is easy to
change the algorithm without having to change the whole program
structure.
*/

#include <stdlib.h>
#include <string.h>

#include "_ARGS.h"
#include "runsvr.h"
#include "server/bullet/bullet.h"
#include "module/buffers.h"
#include "runrights.h"
#include "segcache.h"
#include "pool_dir.h"
#include "objects.h"

/*
When asked to select a host for a new process, the run server evaluates
the following formula for each host having an appropriate architecture "arch",
and chooses a host which evaluates within EQUIVPERC percent from the highest
value:

	PREF-arch *
	(MIN(memleft, MEMTRUNC) + textsize*CACHEMEM) *
	speed * (1 + textsize/CACHETIME)

The following dynamic values are input to the formula:

memleft:	The free memory in the processor minus the size of the
		new program.

textsize:	The size of the program's text segment, IF it is
		believed to be cached; zero otherwise.

speed:		The expected speed at which the program will run;
		computed from the cpu speed and the number of threads
		already running.

The following parameters, settable via the "std_params" interface,
influence the outcome:

MEMTRUNC:	If memory left is more than MEMTRUNC K bytes, pretend only
		MEMTRUNC is left.  This avoids the effect that a host
		with gobs of memory attracts all the small jobs.

CACHETIME:	We expect a speedup of 2 by avoiding having to load
		the text segment for a program with a text segment of
		CACHETIME K bytes.
		XXX Big programs win big here, unfortunately they also
		tend to run longer, which means they get more cradit
		than they deserve.

CACHEMEM:	We expect an average savings of CACHEMEM percent of the
		text size if the text segment is found cached.  CACHEMEM
		represents the expected chance that the text segment is
		already in use by another copy of the same program.

EQUIVPERC:	If two hosts evaluate to values within EQUIVPERC
		percent, they are considered equivalent candidate.  The
		choice between equivalent candidates is made by a random
		generator.

PREF-arch:	For every architecture "arch" in a pool this parameter
		this parameter can be used to promote or discourage its
		use (e.g. to account for slow rpc or process startup time).
		The default value for each architecure is 100%.
ENABLE:		For each pool it can be turned off or on.  ENABLE can be
		1 (on) or 0 (off).
*/

/* architecture specific param values */
#define MIN_PREFARCH	0L
#define MAX_PREFARCH	1000L
#define DEF_PREFARCH	100L
#define NAME_PREFARCH	"PREF-%s"

/* Minimum, maximum and default parameter values for any architecture */
#define MIN_EQUIVPERC	0L
#define MAX_EQUIVPERC	50L
#define DEF_EQUIVPERC	5L
#define NAME_EQUIVPERC	"EQUIVPERC"

#define MIN_MEMTRUNC	1L
#define MAX_MEMTRUNC	64000L
#define DEF_MEMTRUNC	2048L
#define NAME_MEMTRUNC	"MEMTRUNC"

#define MIN_CACHETIME	1L
#define MAX_CACHETIME	10000L
#define DEF_CACHETIME	500L
#define NAME_CACHETIME	"CACHETIME"

#define MIN_CACHEMEM	1L
#define MAX_CACHEMEM	200L
#define DEF_CACHEMEM	10L
#define NAME_CACHEMEM	"CACHEMEM"

#define	ENABLE_ON	1
#define	ENABLE_OFF	0
#define	DEF_ENABLE	ENABLE_ON
#define	NAME_ENABLE	"ENABLE"


/* Forward */
static long calcmem();
static long evaluate();
static int close_enough();

void
initpoolparams(pool)
struct pooldir *pool;
{
    pool->pd_equivperc = DEF_EQUIVPERC;
    pool->pd_memtrunc = DEF_MEMTRUNC;
    pool->pd_cachetime = DEF_CACHETIME;
    pool->pd_cachemem = DEF_CACHEMEM;
    pool->pd_enabled = DEF_ENABLE;
}

void
initarchparams(arch)
struct archdir *arch;
{
    arch->ad_params.ad_multiplier = DEF_PREFARCH;
}

/* Find the best host to execute one of the given set of process descriptors.
 * Returns standard error codes.
 */
errstat
findbest(priv, npd, pd_list, ret_host, ret_index)
private		 *priv;		/* private part of the client's request */
int		  npd;		/* length of pd_list */
process_d	 *pd_list[];	/* array of pd's, one per architecture */
struct hostinfo **ret_host;	/* host chosen */
int 		 *ret_index;	/* index of pd chosen */
{
    struct hostinfo *p, *best;
    struct pooldir  *pool;

    int	        nbest, best_index, pd_index;
    segment_d  *sd;
    long	memneed;
    long	besteval, thiseval;
    errstat	err;
    object     *pool_obj = NULL;
	
    best = NULL;
    besteval = 0;
    nbest = 0;

    err = obj_find(priv, OBJ_POOL, RUN_RGT_FINDHOST, &pool_obj);
    if (err != STD_OK) {
	return err;
    }
    pool = pool_obj->obj_pool;
    if (!pool->pd_enabled) {
	return STD_NOTNOW;
    }
    
    for (pd_index = 0; pd_index < npd; pd_index++) {
	struct archdir *a;
	struct hostcap *hc;
	process_d *pd = pd_list[pd_index];

	if ((a = find_pool_arch(pool, pd_arch(pd))) == NULL) {
	    /* Not fatal, because we might have a few other pd's
	     * for which we *do* have a corresponding arch dir.
	     */
	    continue;
	}
	
	memneed = calcmem(pd);
	sd = whichseg(pd);
	
	for (hc = a->ad_host; hc != NULL; hc = hc->hc_next) {
	    p = hc->hc_info;
	    if (!p->hi_up) {
		continue;	/* skip down hosts */
	    }
	    if (p->hi_lastrepl < p->hi_lastpoll) {
		/* No reply has been received yet from the last poll of
		 * this host.  This is suspect if the poll was started
		 * long ago -- a poll may be hanging for quite a while
		 * when the host just went down while its port was still
		 * in the port cache.
		 * So, if the last poll was more than a second ago we
		 * don't consider this host.  This will reduce the
		 * chance of returning a host that just crashed (but of
		 * course it is still possible...).
		 */
		if (sys_milli() - p->hi_lastpoll >= 1000L) {
		    MON_EVENT("findbest skip suspect host");
		    continue;
		}
	    }
	    thiseval = evaluate(p, memneed, sd, pool, a);
	    if (thiseval > 0)
		p->hi_nconsidered++;

	    if (nbest > 0 && besteval > 0 &&
		    close_enough(thiseval, besteval, pool->pd_equivperc)){
		nbest++;
		if (debugging >= 2) {
		    printf("findbest find equivalent host\n");
		}
		/* With a chance of 1/nbest, choose this */
		if (rand() % nbest == 0) {
		    if (debugging >= 2) {
			printf("new host wins\n");
		    }
		    best = p;
		    best_index = pd_index;
		    besteval = thiseval;
		}
	    }
	    else if (thiseval > besteval) {
		if (debugging >= 2) {
		    printf("found better host %s\n", p->hi_name);
		}
		nbest = 1;
		best = p;
		besteval = thiseval;
		best_index = pd_index;
	    }
	}
    }
	
    if (best == NULL) {
	MON_EVENT("findbest found no good host");
	obj_release(pool_obj);
	return STD_NOTNOW;
    }

    if (debugging) {
	printf("-> %s\n", best->hi_name);
    }

    /*
     * Now we try a little heuristic to stop this machine being reused in
     * a hurry if the new program runs it short of memory with lots of
     * mallocs.
     */
    best->hi_freebytes -= memneed * 2;
    if (best->hi_freebytes < 0)
	best->hi_freebytes = 0;
    best->hi_runnable += 1024;
    best->hi_nbest++;

    if (sd != NULL)
	useseg(best, sd);

    *ret_host = best;
    *ret_index = best_index;

    obj_release(pool_obj);
    return STD_OK;
}

/* Return true of a and b are within equivperc percent of each other */

static int
close_enough(a, b, equivperc)
	long a, b;
	long equivperc;
{
	long diff = a - b;
	if (diff < 0)
		diff = -diff;
	if (a < 0)
		a = -a;
	if (b < 0)
		b = -b;
	return diff < (a+b) * equivperc / 200;
}

/* Calculate the amount of memory needed for a given process, in bytes. */

static long
calcmem(pd)
	process_d *pd;
{
	int i;
	struct segment_d *sd;
	long memneed;
	
	memneed = 0;
	for (i = pd->pd_nseg, sd = PD_SD(pd); --i >= 0; ++sd) {
		if (sd->sd_type & MAP_TYPEMASK) {
			memneed += sd->sd_len;
			/* Add 16K for zero size stack segment: */
			if (sd->sd_len == 0 && (sd->sd_type & MAP_SYSTEM)) {
				if (strcmp("sparc", pd_arch(pd)) == 0) {
				    memneed += 16*1024;
				} else {
				    memneed += 64*1024;
				}
			}
		}
	}
	
#ifndef NDEBUG
	if (debugging >= 2) {
		printf("calculated memory need is %d\n", memneed);
	}
#endif

	return memneed;
}

/* Return a number giving a measure of the suitability of a host for the
   given process.  A negative return value means totally unsuitable. */

static long
evaluate(p, memneed, sd, pool, arch)
	struct hostinfo *p;
	long memneed;
	segment_d *sd;
	struct pooldir *pool;	/* for per-pool evaluation params */
	struct archdir *arch;	/* for per-arch evaluation params */
{
	long kbleft;
	long kipsavail;
	long oldret, ret;
	long textsize;
	
	kbleft = (p->hi_freebytes - memneed) / 1024;
	if (kbleft > pool->pd_memtrunc)
		kbleft = pool->pd_memtrunc;
	kipsavail = p->hi_cpuspeed / (p->hi_runnable + 1024);
	oldret = ret = kbleft * kipsavail;
	textsize = 0;
	if (ret >= 0 && sd != NULL) {
		textsize = checksegbonus(p, sd);
			/* Text seg size (bytes) or 0 */
		if (textsize > 0) {
			ret = (kbleft + textsize*pool->pd_cachemem/100 / 1024)
				* kipsavail;
			ret = ret/1024 * (1024 + textsize/pool->pd_cachetime);
			/* NB This expression is in danger of overflow! */
		}
	}
	ret = ret/100 * arch->ad_params.ad_multiplier;
	
#ifndef NDEBUG
	if (debugging >= 2) {
	  if (textsize > 0)
	    printf(
	      "eval %s: left=%ldk, kips=%ld, ret %d, textsize=%ldk, ret %d\n",
	      p->hi_name, kbleft, kipsavail, oldret, textsize/1024, ret);
	  else
	    printf("eval %s: left=%ldk, kips=%ld, ret %d\n",
	  			p->hi_name, kbleft, kipsavail, ret);
	}
#endif
	return ret;
}

/*
 * std_getparams implementation
 */

static char *
put_long_var(buf, end, name, min, max, type, descr, value)
char *buf, *end;	/* buffer start and end */
char *name;		/* name of the variable */
long min, max;		/* minimum and maximum value (to create type string) */
char *type;		/* the "currency" of the type */
char *descr;		/* a short description telling what the param does */
long value;		/* the current value */
/*
 * Put the string representation of an integer variable in a buffer,
 * in the format required by std_getparams().
 */
{
    register char *bufp = buf;

    /* name\0 */
    bufp = buf_put_string(bufp, end, name);

    /* type\0 */
    if ((bufp != NULL) && (end - bufp >= 20 + strlen(type))) {
	/* type is actually a subrange: */
	sprintf(bufp, "%ld..%ld %s", min, max, type);

	/* skip value until after nul byte */
	bufp = strchr(bufp, '\0');
	bufp++;
    }

    /* description\0 */
    bufp = buf_put_string(bufp, end, descr);

    /* value\0 */
    if ((bufp != NULL) && (end - bufp >= 10)) {
	sprintf(bufp, "%ld", value);

	/* skip value until after nul byte */
	bufp = strchr(bufp, '\0');
	bufp++;
    }

    return bufp;
}


errstat
impl_std_getparams(h, buf, max_buf, n, len, nparams)
header *h;
char *buf;	/* to put params in */
int max_buf;
int n;		/* length of client buffer */
int *len;	/* length of buffer returned */
int *nparams;	/* number of parameters returned */
{
    struct archdir *arch;
    struct pooldir *pool;

    int		np;
    char       *bufp, *end;
    object     *pool_obj;
    errstat	err;

    if (n < 0) {
	return STD_ARGBAD;
    }
    if (n > max_buf) {
	n = max_buf;
    }
    bufp = buf;
    end = &buf[n];

    /* std_getparams only works on a pool object, with adminstrative rights: */
    err = obj_find(&h->h_priv, OBJ_POOL, RUN_RGT_ADMIN, &pool_obj);
    if (err != STD_OK) {
	return err;
    }
    pool = pool_obj->obj_pool;

    np = 0;
    bufp = put_long_var(bufp, end, NAME_EQUIVPERC,
			MIN_EQUIVPERC, MAX_EQUIVPERC, "%",
			"hosts in this range considered equivalent",
			pool->pd_equivperc);
    np++;
    bufp = put_long_var(bufp, end, NAME_MEMTRUNC,
			MIN_MEMTRUNC, MAX_MEMTRUNC, "Kb",
			"ignore free memory exceeding it", pool->pd_memtrunc);
    np++;
    bufp = put_long_var(bufp, end, NAME_CACHETIME,
			MIN_CACHETIME, MAX_CACHETIME, "Kb",
			"twofold speedup at this text size",
			pool->pd_cachetime);
    np++;
    bufp = put_long_var(bufp, end, NAME_CACHEMEM,
			MIN_CACHEMEM, MAX_CACHEMEM, "%",
			"text segment sharing advantage", pool->pd_cachemem);
    np++;

    /* put the per-architecture parameters in the buffer */
    for (arch = pool->pd_arch; arch != NULL; arch = arch->ad_next) {
	char pref_arch[40];

	sprintf(pref_arch, NAME_PREFARCH, arch->ad_name);
	bufp = put_long_var(bufp, end, pref_arch, MIN_PREFARCH, MAX_PREFARCH,
			    "%", "architecture preference multiplier",
			    arch->ad_params.ad_multiplier);
	np++;
    }
    bufp = put_long_var(bufp, end, NAME_ENABLE,
			ENABLE_OFF, ENABLE_ON, "%",
			"enable/disable pool", pool->pd_enabled);
    np++;

    obj_release(pool_obj);

    if (bufp == NULL) {
	return STD_OVERFLOW;
    }

    *nparams = np;
    *len = bufp - buf;
    return STD_OK;
}

/*
 * std_setparams implementation
 */

static errstat
set_long_var(var, strvalue, min, max)
long *var;
char *strvalue;
long min, max;
{
    char *after_val;
    long value;

    /* first convert string to integer */
    value = strtol(strvalue, &after_val, 0);
    if (after_val == strvalue) { /* could not get integer out of strvalue */
	if (debugging) {
	    fprintf(stderr, "set_long_var: %s is not an integer\n", strvalue);
	}
	return STD_ARGBAD;
    }

    /* perform range check */
    if (value < min || value > max) {
	return STD_ARGBAD;
    }

    *var = value;
    return STD_OK;
}

errstat
set_pool_param(p, param, val)
struct pooldir *p;
char *param;
char *val;
{
    errstat err = STD_NOTFOUND;	/* unknown parameter name */

    if (strcmp(param, NAME_EQUIVPERC) == 0) {
	err = set_long_var(&p->pd_equivperc,val, MIN_EQUIVPERC, MAX_EQUIVPERC);
    } else if (strcmp(param, NAME_MEMTRUNC) == 0) {
	err = set_long_var(&p->pd_memtrunc, val, MIN_MEMTRUNC, MAX_MEMTRUNC);
    } else if (strcmp(param, NAME_CACHETIME) == 0) {
	err = set_long_var(&p->pd_cachetime,val, MIN_CACHETIME, MAX_CACHETIME);
    } else if (strcmp(param, NAME_CACHEMEM) == 0) {
	err = set_long_var(&p->pd_cachemem, val, MIN_CACHEMEM, MAX_CACHEMEM);
    } else if (strcmp(param, NAME_ENABLE) == 0) {
	err = set_long_var(&p->pd_enabled, val, ENABLE_OFF, ENABLE_ON);
    } else {
	struct archdir *arch;

	/* or maybe it's of the form PREF-arch */
	for (arch = p->pd_arch; arch != NULL; arch = arch->ad_next) {
	    char pref_arch[40];

	    sprintf(pref_arch, NAME_PREFARCH, arch->ad_name);
	    if (strcmp(param, pref_arch) == 0) {
		err = set_long_var(&arch->ad_params.ad_multiplier, val,
				   MIN_PREFARCH, MAX_PREFARCH);
		break;
	    }
	}
    }
	
    if (err != STD_OK) {
	fprintf(stderr, "refused to set parameter \"%s\" (%s)\n",
		param, err_why(err));
    }

    return err;
}

errstat
impl_std_setparams(h, param_buf, max_param_buf, buflen, nparams)
header *h;
char *param_buf;
int max_param_buf;
int buflen;
int nparams;
{
    struct pooldir *p;
    char       *bufp, *end;
    char       *par, *val;
    errstat	err, retval = STD_OK;
    object     *pool_obj;

    if (buflen < 0) {
	return STD_ARGBAD;
    }
    if (buflen > max_param_buf) {
	/* don't handle truncated request */
	return STD_OVERFLOW;
    }
    err = obj_find(&h->h_priv, OBJ_POOL, RUN_RGT_ADMIN, &pool_obj);
    if (err != STD_OK) {
	return err;
    }
    p = pool_obj->obj_pool;

    /* to avoid forgetting cleanup actions, call RETURN rather than return: */
#   define RETURN(e) { retval = e; goto Exit; }

    if (nparams != 1) {
	/* Only allow setting one parameter at a time; I'm lazy today.
	 * Note that this is no big restriction, as the std_params program
	 * currently sets multiple parameters one by one.
	 */
	RETURN(STD_ARGBAD);
    }

    bufp = param_buf;
    end = &param_buf[buflen];

    bufp = buf_get_string(bufp, end, &par);
    bufp = buf_get_string(bufp, end, &val);
    if (bufp == NULL) {
	RETURN(STD_ARGBAD);
    }

    if ((err = set_pool_param(p, par, val)) == STD_OK) {
	err = obj_set_attribute(pool_obj, par, val);
    }

    RETURN(err);

Exit:
    obj_release(pool_obj);
    if (err == STD_OK) {
	refresh_adminfile();	/* write it to disk */
    }
    return retval;
}
