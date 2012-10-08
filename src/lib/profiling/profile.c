/*	@(#)profile.c	1.3	94/04/07 10:58:50 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Simple user level profiling package based on the ACK procentry()/procexit()
 * feature.  It maintains a per-thread call graph and does timings using
 * sys_milli().
 *
 * TODO:
 * - syscall stubs don't call procentry/procexit yet.
 * - longjmp() can modify the stack in unexpected ways,
 *   we can at least make sure our package handles the simple case
 *   (i.e., assume we jump back to last function calling setjmp)
 */

#include "amoeba.h"
#include "thread.h"
#include "module/mutex.h"
#include "module/syscall.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

/* Define COUNT_OVERHEAD when you want take the overhead introduced by
 * this module into account.  Unfortunately, this causes more sys_milli()
 * overhead, but the timing figures presented should become more accurate.
 */
#define COUNT_OVERHEAD

/* A static maximum number of threads makes it all much simpler .. */
#define MAXTHREADS	20

#ifdef COUNT_OVERHEAD
static unsigned long Total_overhead = 0;
#endif

struct func_call {
    struct func_info *fc_caller;	/* parent function descriptor       */
    int		      fc_ntimes;	/* number of times called by it     */
    struct func_call *fc_next;		/* next parent function             */
};

struct func_info {
    char             *fi_name;		/* the function's name		    */
    struct func_call *fi_called;	/* list of functions that called it */
    int		      fi_ntimes;	/* number of times called	    */
    unsigned long     fi_t_total;	/* total time spent in it	    */
    unsigned long     fi_t_min;		/* minimum time spent in it         */
    unsigned long     fi_t_max;		/* maximum time spent in it         */
    struct func_info *fi_next;		/* next func 			    */
};

struct func_stack {
    struct func_info  *fs_func;		/* the function 		     */
    struct func_stack *fs_next;		/* next function on the stack        */
    unsigned long      fs_t_start;	/* starting time for this invocation */
    unsigned long      fs_t_overhead;	/* accum. overhead of this module    */
};

struct thd_info {
    int    	       thd_here;	/* thread in this module?            */
    struct func_info  *thd_funcs;	/* functions called in this thread   */
    struct func_stack *thd_stack;	/* current func stack in this thread */
    mutex	       thd_mu;		/* disallow concurrent access        */
} thread_info[MAXTHREADS];

static mutex Freelist_mu;

/* A few `generic' structure allocation routines.  In order to allow
 * timings on Amoeba's malloc library as well, we allocate the first
 * bunch of structures from static arrays.  Otherwise this module would
 * calling malloc() with another malloc() call pending, leading to deadlock.
 * A really fullproof version probably should probably refrain from using
 * the (standard) malloc library altogether.
 */

#define ALLOC_CHUNCK	1000

#define DEF_free(FREEFUNC, TYPE, FREELIST, NEXT)\
						\
static struct TYPE *FREELIST = NULL;		\
						\
static void					\
FREEFUNC(fs, n)					\
register struct TYPE *fs;			\
int n;						\
{						\
    register int i;				\
						\
    mu_lock(&Freelist_mu);			\
    for (i = 0; i < n; i++) {			\
	fs->NEXT = FREELIST;			\
	FREELIST = fs;				\
	fs++;					\
    }						\
    mu_unlock(&Freelist_mu);			\
}

#define DEF_new(NEWFUNC, TYPE, FREEFUNC, FREELIST, NEXT) \
						\
static struct TYPE *				\
NEWFUNC()					\
{						\
    struct TYPE *ret = NULL;			\
						\
    mu_lock(&Freelist_mu);			\
    if (FREELIST != NULL) {			\
	ret = FREELIST;				\
	FREELIST = FREELIST->NEXT;		\
    }						\
    mu_unlock(&Freelist_mu);			\
    if (ret == NULL) {				\
	ret = (struct TYPE *) calloc(ALLOC_CHUNCK, sizeof(struct TYPE)); \
	if (ret != NULL) {			\
	    FREEFUNC(ret + 1, ALLOC_CHUNCK - 1);\
	} else {				\
	    abort();				\
	}					\
    }						\
    return ret;					\
}

#define DEF_init(INITFUNC, TYPE, FREEFUNC)	\
						\
static void					\
INITFUNC()					\
{						\
    static struct TYPE def_alloc[ALLOC_CHUNCK];	\
						\
    FREEFUNC(&def_alloc[0], ALLOC_CHUNCK);	\
}

DEF_free(free_func_stack, func_stack, stack_freelist,  fs_next)
DEF_new ( new_func_stack, func_stack, free_func_stack, stack_freelist, fs_next)
DEF_init(init_func_stack, func_stack, free_func_stack)

DEF_free(free_func_call,  func_call,  call_freelist,   fc_next)
DEF_new ( new_func_call,  func_call,  free_func_call,  call_freelist,  fc_next)
DEF_init(init_func_call,  func_call,  free_func_call)

DEF_free(free_func_info,  func_info,  info_freelist,   fi_next)
DEF_new ( new_func_info,  func_info,  free_func_info,  info_freelist,  fi_next)
DEF_init(init_func_info,  func_info,  free_func_info)

/*
 * We try to recognise the situations where we are calling a library function
 * from this module.  Because we want to be able to handle multiple threads
 * as well, we maintain all adminstration in a per-thread datastructure.
 * The index of the calling thread is found by means of a glocal variable.
 *
 * Note: the threads library should not be compiled with the profiling
 * option; otherwise we would quickly run into an infinite recursion.
 */

static int Prof_nthreads = 0;
static int Prof_initialized = 0;

static void
prof_init()
{
    mu_init(&Freelist_mu);
    init_func_stack();
    init_func_call();
    init_func_info();
    Prof_initialized = 1;
}

static struct thd_info *
enter()
{
    /* Flag to avoid recursion while getting the thread id: */
    static int Prof_getting_id = 0;
    static int ident;
    register int *thread_id;
    register struct thd_info *thdp;

    if (!Prof_initialized) {
	prof_init();
    }

    if (Prof_getting_id) {
	return NULL;
    }
    Prof_getting_id = 1;
    thread_id = (int *) thread_alloc(&ident, sizeof(int));
    Prof_getting_id = 0;

    /* When initialized, the glocal variable referred to by `ident'
     * contains the thread number plus one.
     */
    if (*thread_id == 0) {
	/* first time called within this thread */
	if (Prof_nthreads < MAXTHREADS) {
	    *thread_id = ++Prof_nthreads;

	    /* initialize thread info structure */
	    thdp = &thread_info[*thread_id - 1];
	    thdp->thd_here = 0;
	    thdp->thd_funcs = NULL;
	    thdp->thd_stack = NULL;
	    mu_init(&thdp->thd_mu);
	} else {
	    thdp = NULL;
	}
    } else {
	thdp = &thread_info[*thread_id - 1];
    }

    if (thdp != NULL) {
	if (!thdp->thd_here) {
	    thdp->thd_here = 1;
	    mu_lock(&thdp->thd_mu);
	} else {
	    thdp = NULL;
	}
    }

    return thdp;
}

static void
leave(thdp)
struct thd_info *thdp;
{
    thdp->thd_here = 0;
    mu_unlock(&thdp->thd_mu);
}

static void
func_enter(start0, ti, func)
unsigned long    start0;
struct thd_info *ti;
char            *func;
{
    /* for now, keep it simple (& thus slow): sequential lookup */
    register unsigned long start1;
    register struct func_info  *fi;
    register struct func_stack *fs;

    for (fi = ti->thd_funcs; fi != NULL; fi = fi->fi_next) {
	/* no need to strcmp; the function names are constant */
	if (fi->fi_name == func) {
	    break;
	}
    }

    if (fi == NULL) {
	/* not yet present; add it */
	fi = new_func_info();
	fi->fi_name = func;
	fi->fi_called = NULL; /* no one called it, until now */
	fi->fi_t_total = fi->fi_t_min = fi->fi_t_max = 0;
	fi->fi_ntimes = 0;
	fi->fi_next = ti->thd_funcs;
	ti->thd_funcs = fi;
    }

    /* update the stack */
    fs = new_func_stack();
    fs->fs_func = fi;
    fs->fs_next = ti->thd_stack;
    ti->thd_stack = fs;

    /* add it to the call structure */
    if (fs->fs_next != NULL) {
	struct func_call *fc;

	for (fc = fi->fi_called; fc != NULL; fc = fc->fc_next) {
	    if (fc->fc_caller == fs->fs_next->fs_func) {
		/* already called this parent before */
		break;
	    }
	}

	if (fc == NULL) {
	    /* first time we are called by this parent */
	    fc = new_func_call();
	    fc->fc_caller = fs->fs_next->fs_func;
	    fc->fc_ntimes = 0;
	    fc->fc_next = fi->fi_called;
	    fi->fi_called = fc;
	}

	fc->fc_ntimes++;
    }

    /* start the timer */
    start1 = sys_milli();
    fs->fs_t_start = start1;

#ifdef COUNT_OVERHEAD
    fs->fs_t_overhead = (start1 > start0) ? (start1 - start0) : 0;
    Total_overhead += fs->fs_t_overhead;
#endif
}

static void
func_leave(stop0, ti, func)
unsigned long    stop0;
struct thd_info *ti;
char            *func;
{
    register struct func_stack *fs;
    register struct func_info  *fi;
    unsigned long stop1, taken;

    fs = ti->thd_stack;
    assert(fs != NULL);
    fi = fs->fs_func;

    if (fi->fi_name != func) {
	printf("thread %d: procexit(\"%s\") does not match stack:\n",
	       (ti - thread_info), func);
	for (; fs != NULL; fs = fs->fs_next) {
	    printf("%s\n", fs->fs_func->fi_name);
	}
	abort();
    }

    /* update timing statistics */
    taken = (stop0 > fs->fs_t_start) ? (stop0 - fs->fs_t_start) : 0;

#ifdef COUNT_OVERHEAD
    taken = (taken > fs->fs_t_overhead) ? (taken - fs->fs_t_overhead) : 0;
#endif

    if (fi->fi_ntimes == 0) {
	/* first time; set min and max to time taken */
	fi->fi_t_min = fi->fi_t_max = taken;
    } else {
	if (taken < fi->fi_t_min) {
	    fi->fi_t_min = taken;
	}
	if (taken > fi->fi_t_max) {
	    fi->fi_t_max = taken;
	}
    }
    fi->fi_t_total += taken;
    fi->fi_ntimes++;

    /* unwind the stack */
    ti->thd_stack = fs->fs_next;

#ifdef COUNT_OVERHEAD
    /* add overhead for terminated invocation to parent */
    if (ti->thd_stack != NULL) {
	ti->thd_stack->fs_t_overhead += fs->fs_t_overhead;

	stop1 = sys_milli();
	if (stop1 > stop0) {
	    ti->thd_stack->fs_t_overhead += (stop1 - stop0);
	    Total_overhead += (stop1 - stop0);
	}
    }
#endif

    free_func_stack(fs, 1);
}

void
procentry(func)
char *func;
{
    unsigned long start0 = sys_milli();
    struct thd_info *thdp;

    if ((thdp = enter()) == NULL) {
	return;
    }

    /* update administration */
    func_enter(start0, thdp, func);

    leave(thdp);
}

void
procexit(func)
char *func;
{
    unsigned long stop0 = sys_milli();
    struct thd_info *thdp;

    if ((thdp = enter()) == NULL) {
	return;
    }

    /* update administration */
    func_leave(stop0, thdp, func);

    leave(thdp);
}

void
prof_dump(fp)
FILE *fp;
/* For ordinary programs this function is typically called when exiting,
 * but servers might call it at appropriate other times (e.g., when a
 * std_status request is received).
 */
{
    static mutex print_mu;
    static int print_inited = 0;
    struct thd_info *thdp;
    int i;

    if (!print_inited) {
	mu_init(&print_mu);
	print_inited = 1;
    }

    /* avoid deadlock when doing two simulaneous prof_dump()s: */
    mu_lock(&print_mu);

    if ((thdp = enter()) == NULL) {
	mu_unlock(&print_mu);
	return;
    }

    if (fp == NULL) { /* by default write to standard error */
	fp = stderr;
    }

    fprintf(fp, "Dumping Profile Statistics\n");
    fprintf(fp, "Calling thread: %d\n", (thdp - thread_info));

#ifdef COUNT_OVERHEAD
    fprintf(fp, "Profiling Overhead: %ld msec\n", Total_overhead);
#endif

    for (i = 0; i < Prof_nthreads; i++) {
	struct thd_info  *ti;
	struct func_info *fi;
	struct func_call *fc;

	fprintf(fp, "\n------ thread %d -------\n", i);

	ti = &thread_info[i];
	/* lock the thread if it's not myself */
	if (ti != thdp) {
	    mu_lock(&ti->thd_mu);
	}

	for (fi = ti->thd_funcs; fi != NULL; fi = fi->fi_next) {
	    fprintf(fp, "%-20s: %3d { %3ld, %3ld, %3ld } [",
		    fi->fi_name, fi->fi_ntimes, fi->fi_t_min,
		    (fi->fi_ntimes != 0)  ? (fi->fi_t_total/fi->fi_ntimes) : 0,
		    fi->fi_t_max);

	    /* print info about its callers: */
	    for (fc = fi->fi_called; fc != NULL; fc = fc->fc_next) {
		fprintf(fp, " %s(%d)", fc->fc_caller->fi_name, fc->fc_ntimes);
	    }
	    fprintf(fp, " ]\n");
	}

	if (ti != thdp) {
	    mu_unlock(&ti->thd_mu);
	}
    }
    
    leave(thdp);
    mu_unlock(&print_mu);
}
