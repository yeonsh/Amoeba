/*	@(#)thread.c	1.8	96/02/27 11:22:36 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * Threads Package
 *
 *	This file contains the implementation of the Amoeba user-mode
 *	threads.
 */

#include "amoeba.h"
#include "thread.h"
#include "module/mutex.h"
#include "module/proc.h"
#include "stdlib.h"
#include "string.h"
#include "cmdreg.h"
#include "stderr.h"


#ifdef lint
struct thread_data *_thread_local;
#endif

#ifdef DEBUG
static mutex print_crit;
#define debug(lev, message_args) \
    if (DEBUG >= lev) {		\
	mu_lock(&print_crit);	\
	printf message_args;	\
	mu_unlock(&print_crit);	\
    } else 0
#else
#define debug(lev, message_args)
#endif

#undef MIN
#define MIN(x,y) ((x)<(y)?(x):(y))

#define THREAD_INIT()	(thread_initialized ? 1 : thread_doinit())

struct thread_data *_main_local;


/* The index table is a bitmap to keep track of free thread slots.  Each
 * thread acquires its own index using either thread_index or thread_alloc.
 * Using this index it can get to its thread local data using either
 * thread_alloc or directly using the structure pointed to by
 * ``_thread_local.''
 */
static char indextab[MAX_THREAD_ALLOC];

/* Per thread there is an array of MAX_THREAD_ALLOC thread slots.  Each thread
 * uses the same `_thread_local' variable to access its local data, but the
 * contents of this variable are different for each thread.  Using thread_index
 * a thread can get to its local data, that is, the data global to the thread.
 */

/* This variable is used for certain critical regions.
 */
static mutex thread_crit;

/*
 * Stack information kept per thread:
 */
struct stack_info {
    char  *st_addr;		/* start address of the stack		*/
    long   st_size;		/* size of the stack			*/
    segid  st_segid;		/* the segment id for the stack segment */
    long   st_exited;		/* set by the kernel after exitthread	*/
    struct stack_info *st_next;	/* next on the list			*/
};

static struct stack_info *stacks_living = NULL;  /* for living threads */
static struct stack_info *stacks_leaving = NULL; /* for leaving threads */

/* Before any of these routines can be used, the current thread needs to be
 * initialized.  Its `_thread_local' pointer points to nothing (as far as we
 * are concerned).
 */
static int thread_initialized;


static int
thread_doinit()
{
    struct thread_data *td;

    if (thread_initialized)
	return 1;
    mu_lock(&thread_crit);
    if (!thread_initialized)
    {
	td = (struct thread_data *) calloc(sizeof(struct thread_data), 1);
	if (td == NULL) {
	    mu_unlock(&thread_crit);
	    return 0;
	}
	td->td_tab[THREAD_STACK].m_data = (char *) td;
	td->td_tab[THREAD_STACK].m_size = sizeof(struct thread_data);
	_main_local = td;
	thread_initialized = 1;
    }
    mu_unlock(&thread_crit);
    return 1;
}


/*
 * The process server started a new thread.  Call the right procedure for
 * that thread.
 */
static void
thread_entry()
{
    struct thread *param;
    struct thread_data *local;

    _thread_local = (struct thread_data *)(-1);	/* _thread_local isn't valid */
    GETLOCAL(local);
    _thread_local = local;
    param = &local->td_tab[THREAD_PARAM];
    (*local->td_entry)(param->m_data, param->m_size);
    thread_exit();
    /*NOTREACHED*/
}

#ifdef DEBUG
static void
stack_dump_free_list()
{
    struct stack_info *st;

    debug(0, ("stack free_list (_thread_local=%x, sp=%x):\n",
	      _thread_local, &st));

    for (st = stacks_living; st != NULL; st = st->st_next) {
	debug(0, ("current: %x+%d\n", st->st_addr, st->st_size));
    }
    for (st = stacks_leaving; st != NULL; st = st->st_next) {
	debug(0, ("leaving: %x+%d (exited: %d)\n",
		  st->st_addr, st->st_size, st->st_exited));
    }
}
#endif /* DEBUG */

static struct stack_info *
stack_reuse(size)
int size;
{
    /* Go through the list of leaving threads and see if one of
     * them has exited whose stack we may reuse.
     */
    struct stack_info *st, *st_prev;

    st_prev = NULL;
    for (st = stacks_leaving, st_prev = NULL;
	 st != NULL;
	 st_prev = st, st = st->st_next)
    {
	if (st->st_exited && st->st_size == size) {
	    /* Found an available stack with the correct size.
	     * Unlink it from the stacks_leaving list.
	     */
	    debug(0, ("stack_reuse: %x+%d\n", st->st_addr, st->st_size));
	    if (st_prev == NULL) {
		stacks_leaving = st->st_next;
	    } else {
		st_prev->st_next = st->st_next;
	    }
	    break;
	}
    }

    return st;
}

static void thread_cleanup();

/*
 * stack_alloc
 *     Create a segment for the stack of a new thread unless we can
 *     reuse the stack of an exited thread.
 */
static char *
stack_alloc(size)
    int size;
{
    struct stack_info *st_info;

    debug(1, ("stack_alloc(%d): _thread_local=%x, sp=%x\n",
					    size, _thread_local, &st_info));

    /* Cannot have more than one thread operating on free list at a time: */
    mu_lock(&thread_crit);

    /* See if we can reuse the stack for a thread that has exited recently */
    st_info = stack_reuse(size);
    
    /* Remove stacks for other threads that have exited */
    thread_cleanup();

    if (st_info == NULL) {
	/* No free segment of correct size, so map a new one */

	char *stack_p;
	segid new_seg;

	st_info = (struct stack_info *) calloc(sizeof(struct stack_info), 1);
	if (st_info == NULL) {
	    mu_unlock(&thread_crit);
	    debug(1, ("stack_alloc(%d): can't alloc stack_info\n", size));
	    return NULL;
	}

	if ((stack_p = findhole((long) size)) == NULL)
	{
	    mu_unlock(&thread_crit);
	    debug(1, ("stack_alloc(%d): can't find hole\n", size));
	    free((_VOIDSTAR) st_info);
	    return NULL;
	}

	/* We will use the segid returned by seg_map to unmap the
	 * segment in stack_free, below:
	 */
	new_seg = seg_map((capability *) NULL, (vir_bytes) stack_p,
			  (vir_bytes) size,
			  (long)(MAP_GROWDOWN | MAP_TYPEDATA | MAP_READWRITE));
	if (new_seg < 0) {
	    mu_unlock(&thread_crit);
	    debug(1, ("stack_alloc(%d): seg_map failed\n", size));
	    free((_VOIDSTAR) st_info);
	    return NULL;
	}

	debug(2, ("\tgot stack @ %x, id = %d, from seg_map\n",
							    stack_p, new_seg));
	st_info->st_segid = new_seg;
	st_info->st_addr = stack_p;
	st_info->st_size = size;
    }

    st_info->st_exited = 0;
    st_info->st_next = stacks_living;
    stacks_living = st_info;

    mu_unlock(&thread_crit);
    debug(1, ("stack_alloc(%d): succeeded\n", size));
    return st_info->st_addr;
}

static void
thread_cleanup()
{
    /* Go through the list of leaving threads and free the stacks and the
     * corresponding administration for those who have exited now.
     */
    struct stack_info *st, *st_next, *still_leaving;

    still_leaving = NULL;
    st_next = NULL;
    for (st = stacks_leaving; st != NULL; st = st_next) {
	st_next = st->st_next;

	if (st->st_exited) {
	    (void) seg_unmap(st->st_segid, (capability *) NULL);
	    free((_VOIDSTAR) st);
	} else {
	    st->st_next = still_leaving;
	    still_leaving = st;
	}
    }

    stacks_leaving = still_leaving;
}


/*
 * stack_free
 *	Mark the stack indicated as free.  The actual freeing by seg_unmap
 *	will happen as soon as we are sure the current owner thread has left,
 *	and no new thread got it in the meantime.
 */
static long *
stack_free(start)
    char *start;
{
    struct stack_info *st, *st_prev;

    /* We don't know how to free main's stack and the data for the main
     * thread's stack is actually malloced in thread_doinit so we just
     * waste it.
     */
    if ((struct thread_data *) start == _main_local) {
	debug(1, ("stack_free(%x,%d): main's stack not freed\n", start, size));
	return NULL;
    }

    mu_lock(&thread_crit);

    debug(1, ("stack_free(%x, %d): _thread_local=%x, sp=%x\n",
	      start, _thread_local, &st));

    /* see if some threads have exited meanwhile */
    thread_cleanup();

    for (st = stacks_living, st_prev = NULL;
	 st != NULL;
	 st_prev = st, st = st->st_next)
    {
	if (st->st_addr == start) {
	    break;
	}
    }
    if (st == NULL) {
	debug(0, ("stack_free: error - stack at %x not found.\n", start));
	mu_unlock(&thread_crit);
	return NULL;
    }

    /* move st from stacks_living to stacks_leaving */
    if (st_prev == NULL) {
	stacks_living = st->st_next;
    } else {
	st_prev->st_next = st->st_next;
    }
    st->st_next = stacks_leaving;
    stacks_leaving = st;

#ifdef DEBUG
    debug(1, ("after stack_free:\n"));
    stack_dump_free_list();
#endif

    mu_unlock(&thread_crit);

    return &st->st_exited;
}


/* Create a new thread, with entry point ``func'' and stacksize ``stsize.''  
 * ``param'' is an area of size ``psize'' allocated using calloc.  The
 * first entry in the thread table points to the stack, and the second
 * points to the parameter section.  ``param'' may be NULL, and ``psize''
 * doesn't necessarily have to correspond to the size of the parameter
 * area, although any other use is discouraged.
 */
int
thread_newthread(func, stsize, param, psize)
void (*func) _ARGS(( char *param, int psize ));
int   stsize;
char *param;
int   psize;
{
    char *data;
    int size;
    struct thread_data *td;

    if (THREAD_INIT() == 0)
	return 0;
    stsize = (stsize + sizeof(char *) - 1) & ~(sizeof(char *) - 1);
    size = stsize + sizeof(struct thread_data);
    if ((data = stack_alloc(size)) == 0)
	return 0;
    (void) memset((_VOIDSTAR) data, 0, (size_t) size);
    td = (struct thread_data *) &data[stsize];
    td->td_entry = func;
    td->td_tab[THREAD_STACK].m_data = data;
    td->td_tab[THREAD_STACK].m_size = size;
    td->td_tab[THREAD_PARAM].m_data = param;
    td->td_tab[THREAD_PARAM].m_size = psize;
    if (sys_newthread(thread_entry, td, td) != 0) {
	debug(1, ("sys_newthread(%x,%x,%x) failed!\n", thread_entry, td, td));
	stack_free(data);
	return 0;
    }
    return 1;
}


/*
 * thread_exit
 *   Die.  Release all the thread local data.
 *   The thread's stack is not yet thrown away because it still needed
 *   until the thread terminates itself by exitthread().
 *   The kernel will tell us when this has happened by setting a flag
 *   specified by the caller of exitthread.  The stack will be released
 *   in the next call to this thread module.
 */

thread_exit()
{
    register i;
    struct thread *thd;
    long *ready = NULL;

    if (THREAD_INIT() != 0) {
	/* Thread was initialised so lets get rid of it */
	struct thread_data *local;

	GETLOCAL(local);
	thd = local->td_tab;
	i = MAX_THREAD_ALLOC;
	while (--i >= 0) {	/* The stack is freed last */
	    if (i == THREAD_STACK) {
		/* The stack is a mapped segment, not malloced. */
		ready = stack_free(thd[i].m_data);
	    } else {
		if (thd[i].m_data != NULL)
		    free((_VOIDSTAR) thd[i].m_data);
	    }
	}
    }
    exitthread(ready);
    /*NOTREACHED*/
}


/* Allocate a free thread index.  The index is used to find the global data of
 * a thread.
 */
thread_index(index)
int *index;
{
    register int i;

    if (*index != 0)
	return 1;
    if (THREAD_INIT() == 0)
	return 0;
    for (i = 2; i < MAX_THREAD_ALLOC; i++)
	if (indextab[i] == 0) {
		indextab[i] = 1;
		*index = i;
		return 1;
	}
    return 0;
}

/* Allocate thread local memory, that is some data global to a
 * thread.  ``index'' is a null-initialized int variable, or one that is
 * initialized with thread_index().  If the data has been allocated already,
 * just return a pointer to it.
 */
char *
thread_alloc(index, size)
int *index;
{
    struct thread *thd;
    register char *data;
    struct thread_data *local;

    if (*index == 0 && !thread_index(index))
	return 0;
    GETLOCAL(local);
    thd = &local->td_tab[*index];
    if (thd->m_data == NULL) {
	if ((data = (char *) calloc((size_t) size, 1)) == NULL)
	    return NULL;
	thd->m_data = data;
	thd->m_size = size;
	return data;
    }
    else if (thd->m_size == size)
	return thd->m_data;
    else
	return NULL;
}


/* Change the size of the allocated thread local memory by copying it to a
 * new block.  This *always* copies the data and does *not* free the previous
 * data; freeing that is the caller's responsibility.  (This is needed by
 * sig_catch.)
 */

char *thread_realloc(index, size)
int *index;
{
    struct thread *thd;
    register char *data;
    struct thread_data *local;

    if (!thread_index(index))
	return NULL;
    GETLOCAL(local);
    thd = &local->td_tab[*index];
    if ((data = (char *) calloc((size_t) size, 1)) == NULL)
	return NULL;
    if (thd->m_data) {
	(void) memmove((_VOIDSTAR) data, (_VOIDSTAR) thd->m_data,
		       (size_t) MIN(thd->m_size, size));
    }
    thd->m_data = data;
    thd->m_size = size;
    return data;
}


/* Return the parameters to this thread.
 */
char *thread_param(size)
int *size;
{
    struct thread_data *local;

    if (THREAD_INIT() == 0)
	return NULL;
    GETLOCAL(local);
    if (size != 0)
	*size = local->td_tab[THREAD_PARAM].m_size;
    return local->td_tab[THREAD_PARAM].m_data;
}
