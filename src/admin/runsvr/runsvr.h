/*	@(#)runsvr.h	1.4	96/02/27 10:18:29 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Common definitions for run server */


/* Include files */

#include <amtools.h>
#include <semaphore.h>
#include <module/proc.h>
#include <module/direct.h>
#include <ajax/mymonitor.h>
#include "exec_fndhost.h"
#include "_ARGS.h"


/* Segment caching data structures */

struct segcache {
	struct segcache	*sc_next;
	capability	sc_cap;
};

#define SEGCACHESIZE 50


/* Host info data structure.
   This is normally linked into a queue of hosts that are up or down.
   If we always know in which queue we are, we don't need an
   'up-or-down' flag here. */

struct hostinfo {
	mutex		hi_mu;		/* protects agains concurrent use */
	struct hostinfo *hi_next;	/* next in list of all hosts */
	int		hi_up;		/* nonzero when up */
	char		hi_name[PD_HOSTSIZE]; /* host name */
	capability	hi_hostcap;	/* host capability */
	capability	hi_proccap;	/* process server cap, when up */
	
	/* Load info from pro_getload */
	long		hi_cpuspeed;	/* nominal CPU speed, instr/sec. */
	long		hi_runnable;	/* average runnable threads, *1024 */
	long		hi_freebytes;	/* free memory, in bytes */
	capability	hi_execcap;	/* cap to start processes with */
	
	/* Segment cache info */
	struct segcache *hi_schead;	/* Most recently inserted entry */
	struct segcache hi_sc[SEGCACHESIZE];
	
	/* Statistics */
	unsigned long	hi_lastflip;	/* last up<-->down transition */
	unsigned long	hi_lastpoll;	/* time when last poll started */
	unsigned long	hi_lastrepl;	/* time when last poll returned
					   (ok or not) */
	long		hi_npolls;	/* number of polls in current state */
	long		hi_nbest;	/* number of times chosen as best */
	long		hi_nconsidered; /* number of times considered at all */
};


/* Pooldir and archdir data structures */

/* A *pooldir* is a directory like /profile/pool, with subdirectories
   for one or more architectures.  There is a pooldir struct for each
   pool managed by the run server. */

struct pooldir {
	struct pooldir *pd_next;	/* Next managed pooldir */
	capability	pd_dir;		/* Corresponding directory object */
	struct archdir *pd_arch;	/* List of archdirs found here */
	int		pd_enabled;	/* 1 if pool may be used, 0 otherwise */

	/* per pool evaluation parameters: */
	long		pd_equivperc;
	long		pd_memtrunc;
	long		pd_cachetime;
	long		pd_cachemem;
};

/* An *archdir* is a directory like /profile/pool/vax, containing
   capabilities for some hosts with a given architecture.  There is
   an archdir struct for each architecture subdirectory of each pooldir
   structure (duplicates are not recognized as such -- there probably
   shouldn't be any). */

struct archdir {
	struct archdir	*ad_next;	/* Next archdir in this pooldir */
	char		*ad_name;	/* Architecture name */
	struct hostcap	*ad_host;	/* List of hostcaps found here */

	struct archparams {
	    long	 ad_multiplier;	/* Per architecture evaluation param*/
	} ad_params;

	/* VM parameters for pro_getdef: */
	int		 ad_clickshift;	/* Zero if not initialized */
	int		 ad_vmlo;
	int		 ad_vmhi;
};

/* A *hostcap* is a host capability like /profile/pool/vax/bcc.
   There is a hostcap structure for each occurrence of a host in
   an archdir.  Hostcap structures that refer to the same host
   from different archdirs point to the same hostinfo structure. */

struct hostcap {
	struct hostcap	*hc_next;	/* Next hostcap in this archdir */
	char		*hc_name;	/* Host name */
	struct hostinfo	*hc_info;	/* Host info structure */
};


/* Functions defined by the run server */

void refresh_adminfile	_ARGS((void));
void adddownpoll	_ARGS((struct hostinfo *p, long delay));
void error		_ARGS((char *format, ...));
void fatal		_ARGS((char *format, ...));

void addjob();

/* Globals */

extern int debugging;			/* Debugging level */
extern char *progname;			/* Program name (set in main) */
extern struct hostinfo *First_host;	/* List of all hostinfo structs */
extern struct tjobset *upjobs, *downjobs; /* Separate job queues for up/down */
