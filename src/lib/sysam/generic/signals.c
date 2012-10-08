/*	@(#)signals.c	1.4	96/02/27 11:22:30 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* New (light-weight-, or thread-) signal interface */
/* XXX This version only saves one pending catcher call */

#include <amoeba.h>
#include <cmdreg.h> /* Needed by stderr.h */
#include <stderr.h> /* For STD_OK and STD_NOMEM */
#include <thread.h> /* For thread_(re)alloc */
#include <exception.h> /* For sig_vector */
#include <fault.h> /* For thread_ustate ; from h/machdep/arch/<ARCH> */
#include <module/signals.h>
#include <module/mutex.h>

#include <stdlib.h>
#include <string.h>


/* Types */

typedef void (*catcher) _ARGS((signum sig, thread_ustate *fp, void *extra));

#ifdef __STDC__
typedef void *closure;
#else
typedef char *closure;
#endif


/* Per-thread global data structure */

struct sig_data {
	int vecused;		/* Number of entries used in vector */
	int vecsize;		/* Number of entries allocated in vector */
	sig_vector *vector;	/* Signal parameters vector */
	int blocking;		/* Block level, > 0 if signals are blocked */
	/* Pending catcher call */
	catcher pnd_func;
	signum pnd_sig;
	closure pnd_extra;
};


/* File-global variables */

static int data_id, vec_id; /* Glocal data module identifiers */


/* Macros to get pointers to glocal data */
#define SIG_DATA(p) {			\
	char *np;			\
	THREAD_DO_ALLOC(np, &data_id, sizeof(struct sig_data)); \
	p = (struct sig_data *) np;	\
    }
#define SIG_VECTOR(p, n) {		\
	char *np;			\
	THREAD_DO_ALLOC(np, &vec_id, (n)*sizeof(sig_vector));	\
	p = (sig_vector *) np;		\
    }
#define SIG_REVECTOR(n) \
    ((sig_vector *) thread_realloc(&vec_id, (n)*sizeof(sig_vector)))


/* The real catcher.  This calls the user's catcher unless blocking.
   If the user's catcher returns, call sys_sigret to return from
   interrupted system calls. */

/* XXX could be static but it's so nice to see this in stack traces */
/* XXX should be void but cc doesn't like casting its address to long */
_sys_catcher(sig, fp, func, extra)
	signum sig;
	thread_ustate *fp;
	catcher func;
	closure extra;
{
	struct sig_data *p;
	
	_thread_local = (struct thread_data *) (-1); /* thread.c will set it */
	
	if (sig < 0) { /* Exceptions cannot be blocked */
		(*func)(sig, fp, extra);
	}
	else {
		SIG_DATA(p);
		if (p != 0) {
			if (p->blocking == 0) {
				(*func)(sig, fp, extra);
			}
			else {
				p->pnd_func = func;
				p->pnd_sig = sig;
				p->pnd_extra = extra;
				/* The fp isn't saved, would be invalid */
			}
		}
	}
	sys_sigret(fp);
}


/* Specify a new catcher function and extra argument for a signal.
   A nil function pointer means the signal is to be ignored.
   Don't call this from within a catcher. */

errstat
sig_catch(sig, func, extra)
	signum	sig;
	void (*func)();
	_VOIDSTAR extra;
{
	struct sig_data *p;
	sig_vector *v, *oldv;
	int i;
	
	if (sig == 0)
		return STD_ARGBAD;
	
	SIG_DATA(p);
	if (p == 0)
		return STD_NOMEM;
	v = p->vector;
	
	/* Block signal catchers while we are manipulating the vector */
	sig_block();
	
	/* Look for existing slot */
	for (i = 0; i < p->vecused; ++i) {
		if (v[i].sv_type == sig) {
			v[i].sv_pc = 0;
#ifdef QPT
			v[i].sv_arg3 = *(int *) func;
#else
			v[i].sv_arg3 = (long) func;
#endif
			v[i].sv_arg4 = (long) extra;
			if (func != 0)
#ifdef QPT
				v[i].sv_pc = *(int *) _sys_catcher;
#else
				v[i].sv_pc = (long) _sys_catcher;
#endif
			sig_unblock();
			return STD_OK;
		}
	}
	
	/* No slot yet -- add to end */
	oldv = 0;
	i = p->vecused;
	if (i + 1 > p->vecsize) {
		/* Allocate or extend allocated vector space.
		   The vector is allocated through thread_(re)alloc
		   so we don't need to clean up at thread exit. */
		oldv = v;
		if (v == 0) {
			SIG_VECTOR(v, i + 1);
		}
		else
			v = SIG_REVECTOR(i + 1);
		if (v == 0) {
			sig_unblock();
			return STD_NOMEM;
		}
		p->vector = v;
		p->vecsize = i + 1;
	}
	v[i].sv_type = sig;
	v[i].sv_pc = 0;
#ifdef QPT
	v[i].sv_arg3 = *(int *) func;
#else
	v[i].sv_arg3 = (long) func;
#endif
	v[i].sv_arg4 = (long) extra;
	if (func != 0)
#ifdef QPT
		v[i].sv_pc = *(int *) _sys_catcher;
#else
		v[i].sv_pc = (long) _sys_catcher;
#endif
	p->vecused = i+1;
	sys_setvec(v, p->vecused);
	if (oldv != 0)
		free((char *) oldv);
	sig_unblock();
	return STD_OK;
}


/* Return a new unique signal number.
   This may be used to create broadcast groups, etc. */

signum
sig_uniq()
{
	static signum seed = 0x10000;
	
	return seed++;
}


/* Block all signals.
   This avoids the call to the user's catcher function;
   the 'real catcher' is still called and the user's catcher will be
   called when signals are unblocked.
   Pairs of calls to sig_block and sig_unblock may be nested;
   only the outer (last) call to sig_unblock will really unblock. */

void
sig_block()
{
	struct sig_data *p;
	
	SIG_DATA(p);
	if (p == 0)
		return;
	p->blocking++;
}


/* Unblock all signals.
   This will call the user's catchers for signals that arrived while
   signals were blocked. */

void
sig_unblock()
{
	struct sig_data *p;
	catcher func;
	
	SIG_DATA(p);
	if (p == 0)
		return;
	p->blocking--;
	if (p->blocking > 0)
		return;
	if (p->blocking != 0)
		abort(); /* Unmatched call */
	if ((func = p->pnd_func) != 0) {
		p->pnd_func = 0;
		(*func)(p->pnd_sig, (thread_ustate *) 0, p->pnd_extra);
	}
}


/* Atomically lock a mutex and block signals. */
/* XXX must be done differently when scheduling changes. */

void
sig_mu_lock(mu)
	mutex *mu;
{
	for (;;) {
		sig_block();
		if (mu_trylock(mu, (interval)(-1)) == 0)
			break;
		sig_unblock();
	}
}


/* Ditto with timeout. */

int
sig_mu_trylock(mu, maxdelay)
	mutex *mu;
	interval maxdelay;
{
	sig_block();
	if (mu_trylock(mu, maxdelay) == 0)
		return 0;
	sig_unblock();
	return -1;
}


/* Unlock a mutex and unblock signals. */

void
sig_mu_unlock(mu)
	mutex *mu;
{
	mu_unlock(mu);
	sig_unblock();
}
