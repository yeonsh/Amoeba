/*	@(#)fifo_impl.c	1.3	94/04/06 11:44:32 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* Created:
 *   apr 20 93  Ron Visser
 * Modified:
 *   june 23 93 Kees Verstoep
 *		Rewritten.  Now works with Ail, and implements protection.
 */

#include "fifosvr.h"
#include <sys/types.h>
#include <time.h>
#include <module/buffers.h>
#include <module/rnd.h>
#include <server/fifo/fifo.h>
#define  _POSIX_SOURCE
#include <limits.h>
#include <ajax/fdstuff.h>
#include <errno.h>
#include <ampolicy.h>
#include "fifo_impl.h"
#include "fifo_main.h"

static char Start_time[40];

/* State_Mu locks the statefile capability and check fields */
static mutex State_Mu;

/*
 * We have three kinds of capabilities:
 * - object 0:
 *	Supercap for administrative requests and new fifos
 * - object 1..Num_Fifo:
 *	The "stable" fifo objects whose checkfield is saved in a statefile.
 * - object Num_Fifo+1..Num_Fifo+Num_fp
 *	The "runtime" fifo references that are created when a fifo object
 *	is opened.  These are not saved in the statefile.
 */

static int Num_Fifo;
static int Num_Fp;

#define FIRST_VALID_FIFO	(1)
#define LAST_VALID_FIFO		(Num_Fifo)
#define FIRST_VALID_FP		(Num_Fifo + 1)
#define LAST_VALID_FP		(Num_Fifo + Num_Fp)

struct fp     *Fp_Tab = NULL;
struct fifo_t *Fifo_Tab = NULL;

static mutex Fp_Mu;
static mutex Fifo_Mu;

struct fp *
fp_entry(obj)
objnum obj;
{
    struct fp *fp;

    if (obj < FIRST_VALID_FP || obj > LAST_VALID_FP) {
	MON_NUMEVENT("fp_entry: bad object", obj);
	return NIL_FP;
    }

    fp = &Fp_Tab[obj - FIRST_VALID_FP];
    if (fp->fp_count == 0 || fp->fp_fifo == NULL) {
	MON_NUMEVENT("fp_entry: bad entry", obj);
	return NIL_FP;
    }

    fp->fp_lifetime = MAX_LIFETIME;
    return fp;
}

static struct fp *
fp_new(f)
struct fifo_t *f;
{
    /* Tries to find a free entry in Fp_Tab.
     * If there is, the entry is locked, and fp_new returns a pointer to it.
     * Otherwise NIL_FP is returned.
     */
    struct fp *fp, *retfp;

    retfp = NIL_FP;
    mu_lock(&Fp_Mu);
    for (fp = &Fp_Tab[0]; fp < &Fp_Tab[Num_Fp]; fp++) {
	if (fp->fp_count == 0) {
	    /* mark this entry as used and initialize it */
	    fp->fp_count = 1;
	    fp->fp_fifo = f;
	    uniqport(&fp->fp_check);
	    /* Currently we use the standard aging mechanism for both
	     * fifo and fp structs.  Maybe the latter should be garbage
	     * collected sooner, using a watchdog thread.
	     */
	    fp->fp_lifetime = MAX_LIFETIME;
	    retfp = fp;
	    break;
	}
    }
    mu_unlock(&Fp_Mu);

    return retfp;
}

static void
fp_unlink(fp)
struct fp *fp;
{
    /* Unlink the fifo from the fp entry */
    MON_EVENT("freeing fp slot");
    fp->fp_fifo = NIL_FIFO;
    fp->fp_count = 0;
}

struct fifo_t *
fifo_entry(obj)
objnum obj;
{
    struct fifo_t *f;

    if (obj < FIRST_VALID_FIFO || obj > LAST_VALID_FIFO) {
	return NIL_FIFO;
    }

    mu_lock(&Fifo_Mu);
    f = &Fifo_Tab[obj - FIRST_VALID_FIFO]; 
    if (f->f_status != F_INUSE) {
	MON_NUMEVENT("fifo_entry: unused entry", obj);
	f = NULL;
    } else {
	f->f_lifetime = MAX_LIFETIME;
    }
    mu_unlock(&Fifo_Mu);

    return f;
}

void
fifo_lock(f)
struct fifo_t *f;
{
    mu_lock(&f->f_mu);
}

void
fifo_unlock(f)
struct fifo_t *f;
{
    mu_unlock(&f->f_mu);
}

struct fifo_t *
fifo_nth_entry(obj)
int obj;
{
    struct fifo_t *f;

    if (obj < 0 || obj >= (LAST_VALID_FIFO - FIRST_VALID_FIFO)) {
	return NIL_FIFO;
    }

    f = &Fifo_Tab[obj]; 
    if (f->f_status != F_INUSE) {
	return NIL_FIFO;
    }

    return f;
}

static errstat
fifo_do_open(f, side)
struct fifo_t *f;
int    side;
{
    /* If the corresponding fifo is not currently open it is opened first. */

    if (f->f_cbclosed) {
	/* Previously closed, but not yet freed.  Do it now.
	 * TODO: we must be sure the readers have gone already.
	 */
	MON_EVENT("cbclosed in fifo_open");
	cb_free(f->f_cb);
	f->f_cb = NULL;
    }
    if (f->f_cb == NULL) {
	f->f_cb = cb_alloc(FIFO_BUF_SIZE);
	if (f->f_cb == NULL) {
	    MON_EVENT("fifo_open: out of memory");
	    return STD_NOSPACE;
	}
    }

    f->f_users[side]++;
    return STD_OK;
}

static errstat
fp_do_close(fp, side, freeit)
struct fp *fp;
int side;
int freeit;
{
    if (fp->fp_count <= 0) {
	return STD_ARGBAD;
    }

    if (--fp->fp_count == 0) {
	struct fifo_t *f = fp->fp_fifo;
	
	f->f_users[side]--;
	if (freeit && (f->f_users[R_SIDE] == 0 && f->f_users[W_SIDE] == 0)) {
	    /* no more users of the fifo; free it */
	    if (f->f_cb != NULL) {
		MON_EVENT("free circbuf");
		cb_free(f->f_cb);
		f->f_cb = NULL;
		f->f_cbclosed = 0;
	    }
	} else if (f->f_users[W_SIDE] == 0) {
	    /* No more writers; mark it as closed.  This will also
	     * wake up the remaing readers.
	     * A new writer will free the circbuf and allocate a new one.
	     */
	    if (f->f_cb != NULL) {
		MON_EVENT("close circbuf");
		cb_close(f->f_cb);
		f->f_cbclosed = 1;
	    }
	}
	fp_unlink(fp);
    }

    return STD_OK;
}

/* If the last writer closes the fifo, all pending reads should return.
 * To accomplish this the circular buffer is closed.
 * It cannot be written again, so it has to be reopened on next use.
 */
/*ARGSUSED*/
errstat
impl_fifo_close(h, obj)
header *h;
objnum  obj;
{
    struct fp *fp;
    struct fifo_t *f;
    errstat err;
    int side;

    if ((fp = fp_entry(obj)) == NIL_FP) {
	return STD_CAPBAD;
    }

    f = fp->fp_fifo;
    fifo_lock(f);
    side = (fp->fp_flags & FWRITE) ? W_SIDE : R_SIDE;
    err = fp_do_close(fp, side, 1);
    fifo_unlock(f);

    return err;
}

static void
del_fp(fp)
struct fp *fp;
{
    int side = (fp->fp_flags & FWRITE) ? W_SIDE : R_SIDE;

    MON_EVENT("delete fp");
    while (fp->fp_count > 0) {
	if (fp_do_close(fp, side, 0) != STD_OK) {
	    return;
	}
    }
}

errstat
del_fifo(f)
struct fifo_t *f;
{
    struct fp *fp;
    int new_status = F_FREE;
    int users;

    MON_EVENT("delete fifo");

    /* First remove any remaining references to this fifo */
    mu_lock(&Fp_Mu);
    for (fp = &Fp_Tab[0]; fp < &Fp_Tab[Num_Fp]; fp++) {
	if (fp->fp_fifo == f) {
	    fp_unlink(fp);
	}
    }
    mu_unlock(&Fp_Mu);

    users = f->f_users[R_SIDE] + f->f_users[W_SIDE];
    if (f->f_cb != NULL) {
	if (f->f_status == F_INUSE && users > 0) {
	    /* There are still users blocked on the fifo, so close it, rather
	     * than free it.  It will be freed during the next aging round.
	     */
	    MON_EVENT("del_fifo: close circbuf");
	    cb_close(f->f_cb);
	    f->f_cbclosed = 1;
	    new_status = F_DELETE;
	} else {
	    MON_EVENT("del_fifo: free circbuf");
	    cb_free(f->f_cb);
	    f->f_cb = NULL;
	    f->f_cbclosed = 0;
	}
    }

    f->f_status = new_status;

    /* If there are threads blocked on opening the fifo, unblock them now. */
    sema_mup(&f->f_wait, NUM_THREADS);
    return STD_OK;
}

/*ARGSUSED*/
errstat
impl_std_age(h, obj)
header *h;
objnum  obj;
{
    int removed = 0;
    struct fifo_t *f;
    struct fp *fp;
    rights_bits rights;

    if (obj != 0) {
	return STD_ARGBAD;
    }
    if (prv_decode(&h->h_priv, &rights, &Super_check) != 0) {
	return STD_CAPBAD;
    }
    if (rights != ALL_RIGHTS) {
	return STD_DENIED;
    }

    /* Decrease the time-to-live field of all existing FPs */
    for (fp = &Fp_Tab[0]; fp < &Fp_Tab[Num_Fp]; fp++) {
	if (fp->fp_count > 0) {
	    f = fp->fp_fifo;
	    fifo_lock(f);
	    if (fp->fp_lifetime > 0) {
		if (--fp->fp_lifetime == 0) {
		    del_fp(fp);
		}
	    }
	    fifo_unlock(f);
	}
    }

    /* Decrease the time-to-live field of all existing FIFOs */
    for (f = &Fifo_Tab[0]; f < &Fifo_Tab[Num_Fifo]; f++) {
	fifo_lock(f);
	if ((f->f_status == F_INUSE && --f->f_lifetime == 0) ||
	    (f->f_status == F_DELETE))
	{
	    if (del_fifo(f) == STD_OK) {
		removed++;
	    }
	}
	fifo_unlock(f);
    }

    if (removed > 0) {
	update_statefile();
    }

    return STD_OK;
}

/*ARGSUSED*/
errstat 
impl_fsq_read(h, obj, offset, bufsiz, buf, max_buf, moredata, retsize)
header *h;
objnum  obj;
long    offset;	/* ignored */
int     bufsiz;
char   *buf;
int	max_buf;
int    *moredata;
int    *retsize;
{
    struct fp *fp;
    struct fifo_t *f;

    if ((fp = fp_entry(obj)) == NULL) {
	return STD_ARGBAD;
    }
    if (bufsiz <= 0) {
	return STD_ARGBAD;
    }
    if (bufsiz > max_buf) {
	bufsiz = max_buf;
    }

    f = fp->fp_fifo;
    fifo_lock(f);

    if (cb_full(f->f_cb) <= 0) {
	/* no data currently available */
	if (f->f_users[W_SIDE] == 0) {
	    MON_EVENT("read hits EOF right away");
	    *retsize = 0;
	    fifo_unlock(f);
	    return STD_OK;
	} else if (fp->fp_flags & FNONBLOCK){
	    MON_EVENT("read with O_NONBLOCK on empty fifo");
	    fifo_unlock(f);
	    return FIFO_AGAIN;
	}
    }

    fifo_unlock(f); /* don't keep the fifo locked while blocked in the read */
    *retsize = cb_gets(f->f_cb, buf, 1, bufsiz);

    if (f->f_status == F_INUSE && cb_full(f->f_cb) >= 0) {
	*moredata = FSE_MOREDATA;
    } else {
	*moredata = FSE_NOMOREDATA;
    }
    return STD_OK;
}

/*ARGSUSED*/
errstat 
impl_fsq_write(h, obj, offset, size, buf, max_buf, written)
header *h;
objnum  obj;
long    offset;
int     size;
char   *buf;
int	max_buf;
int    *written;
{
    int max_size;
    struct fifo_t *f;
    struct fp *fp;

    if ((fp = fp_entry(obj)) == NULL) {
	return STD_ARGBAD;
    }
    f = fp->fp_fifo;
    fifo_lock(f);

    if (f->f_users[R_SIDE] == 0) {
	/* Return STD_NOTNOW to signal on a broken pipe */
	MON_EVENT("write gets broken pipe");
	fifo_unlock(f);
	return STD_NOTNOW;
    }

    if (fp->fp_flags & FNONBLOCK) {
	if (size > max_buf) {
	    if ((max_size = cb_empty(f->f_cb)) > 0) {
		size = max_size;
		MON_NUMEVENT("write: truncated size", size);
	    } else {
		MON_EVENT("write > BUF_SIZE with O_NONBLOCK");
		fifo_unlock(f);
		return FIFO_AGAIN;
	    }
	} else {
	    if (cb_empty(f->f_cb) < size) {
		MON_EVENT("write > avail with O_NONBLOCK");
		fifo_unlock(f);
		return FIFO_AGAIN;
	    }
	}
    }

    fifo_unlock(f); /* don't keep fifo locked while locked in write */
    if (cb_puts(f->f_cb, buf, size) < 0) {
	MON_EVENT("cb_puts failed");
	return FIFO_BADF;
    }

    *written = size;
    return STD_OK;
}

/*ARGSUSED*/
errstat
impl_fifo_open(h, obj, flags, fifo_ref)
header *h;
objnum  obj;
int     flags;
capability *fifo_ref;
{
    struct fp *fp;
    struct fifo_t *f;
    capability newcap;
    int     side;
    int     n_this_side, n_other_side, n_read_side;
    errstat err;

    if ((f = fifo_entry(obj)) == NIL_FIFO) {
	return STD_CAPBAD;
    }

    /* exactly one of FREAD, FWRITE should be specified */
    if (flags & FWRITE) {
	if (flags & FREAD) {
	    MON_EVENT("both reading and writing");
	    return STD_ARGBAD;
	}
	side = W_SIDE;
    } else if (flags & FREAD) {
	side = R_SIDE;
    } else {
	MON_EVENT("requires reading or writing");
	return STD_ARGBAD;
    }

    fifo_lock(f);

    if ((fp = fp_new(f)) == NULL) {
	MON_EVENT("cannot allocate fp slot");
	fifo_unlock(f);
	return STD_NOSPACE;
    }

    if ((err = fifo_do_open(f, side)) != STD_OK) {
	fp->fp_count = 0;
	fifo_unlock(f);
	return err;
    }

    fp->fp_flags = flags & (FREAD|FWRITE|FNONBLOCK);
    if (prv_encode(&newcap.cap_priv, fp->fp_objnum,
		   ALL_RIGHTS, &fp->fp_check) != 0) {
	fatal("prv_encode failed!");
    }
    newcap.cap_port = Pub_cap.cap_port;

    n_this_side = f->f_users[side];
    n_other_side = f->f_users[OTHER_SIDE(side)];
    n_read_side = f->f_users[R_SIDE];
    fifo_unlock(f);

    if ((flags & FNONBLOCK) == 0) {
	/* Blocking fifo_open() */
	/* TODO: put a limit on the number of threads blocked in fifo_open? */
	if (n_other_side == 0) {
	    /* Block until opened on the other side */
	    MON_EVENT("waiting for other side to open");
	    sema_down(&f->f_wait);

	    /* The fifo must be not have been deleted while we blocked! */
	    if (f->f_status != F_INUSE) {
		(void) fp_do_close(fp, side, 1);
		return FIFO_BADF;
	    }
	} else {
	    if (n_this_side == 1) {
		/* First one on this side; unblock all waiting threads
		 * on other side.
		 */
		MON_EVENT("unblock other side");
		sema_mup(&f->f_wait, n_other_side);
	    }
	}
    } else {
	/* Non-blocking fifo_open() */
	if (side == W_SIDE && n_read_side == 0) {
	    MON_EVENT("open: no readers");
	    (void) fp_do_close(fp, side, 1);
	    return FIFO_ENXIO;
	}
    }

    *fifo_ref = newcap;
    return STD_OK;
}

static void
fifo_init(f)
struct fifo_t *f;
{
    f->f_lifetime = MAX_LIFETIME;
    f->f_cb = NULL; /* allocated when the fifo is actually opened */
    f->f_cbclosed = 0;
    f->f_users[R_SIDE] = 0;
    f->f_users[W_SIDE] = 0;
    mu_init(&f->f_mu);
    sema_init(&f->f_wait, 0);
    f->f_status = F_INUSE;
}

static struct fifo_t *
fifo_new()
{
    struct fifo_t *f;
    int i;

    mu_lock(&Fifo_Mu);
    for (i = 0; i < Num_Fifo; i++) {
	f = &Fifo_Tab[i];
	if (f->f_status == F_FREE) {
	    fifo_init(f);
	    uniqport(&f->f_check);
	    break;
	}
    }
    mu_unlock(&Fifo_Mu);

    return (i < Num_Fifo) ? f : NIL_FIFO;
}

/*ARGSUSED*/
errstat
impl_fifo_create(h, obj, fifo_cap)
header     *h;
objnum      obj;
capability *fifo_cap;
{
    /* this routine returns a capability for a new fifo */
    struct fifo_t *f;

    if (obj != 0) {
	/* needs to be called with the super cap as arg */
	return STD_ARGBAD;
    }

    mu_lock(&State_Mu);
    if ((f = fifo_new()) != NULL) {
	if (prv_encode(&fifo_cap->cap_priv, f->f_objnum,
		       ALL_RIGHTS, &f->f_check) != 0) {
	    fatal("prv_encode failed!!");
	}
	fifo_cap->cap_port = Pub_cap.cap_port;
	update_statefile();
    }
    mu_unlock(&State_Mu);

    return (f != NULL) ? STD_OK : STD_NOSPACE;
}

/*ARGSUSED*/
errstat
impl_fifo_share(h, obj)
header *h;
objnum  obj;
{
    /* this routine is called after a fork, to increase the usage count */
    struct fp *fp;
  
    if ((fp = fp_entry(obj)) == NIL_FP) {
	return STD_CAPBAD;
    }

    fifo_lock(fp->fp_fifo);
    fp->fp_count++;
    fifo_unlock(fp->fp_fifo);
    return STD_OK;
}

int
fifo_stat(f, buf, size)
struct fifo_t *f;
char   *buf;
int     size;
{
    char mybuf[200]; /* big enough */
    size_t len;

    strcpy(mybuf, "fifonum readers writers bytes lifetime\n");
    strcat(mybuf, "------- ------- ------- ----- --------\n");

    fifo_lock(f);
    sprintf(mybuf + strlen(mybuf), "%7d %7d %7d %5d %8d\n",
	    f->f_objnum, f->f_users[R_SIDE], f->f_users[W_SIDE],
	    ((f->f_cb != NULL) ? cb_full(f->f_cb) : 0), f->f_lifetime);
    fifo_unlock(f);

    len = strlen(mybuf);
    strncpy(buf, mybuf, (size > len) ? len : size);
    return len;
}

int
fifo_global_stat(buf, size)
char *buf;
int   size;
{
    char mybuf[200]; /* big enough */
    int i;
    int n_free = 0, n_inuse = 0, n_open = 0, n_fresh = 0;
    size_t len;

    /* gather some global statistics */
    mu_lock(&Fifo_Mu);
    for (i = 0; i < Num_Fifo; i++) {
	struct fifo_t *f = &Fifo_Tab[i];

	fifo_lock(f);
	if (f->f_status == F_FREE) {
	    n_free++;
	} else if (f->f_status == F_INUSE) {
	    n_inuse++;
	    if (f->f_users[R_SIDE] + f->f_users[W_SIDE] > 0) {
		n_open++;
	    }
	    if (f->f_lifetime == MAX_LIFETIME) {
		n_fresh++;
	    }
	}
	fifo_unlock(f);
    }
    mu_unlock(&Fifo_Mu);

    sprintf(mybuf, "fifo server version %s; started %s\n",
	    VERSION, Start_time);
    sprintf(mybuf + strlen(mybuf),
	    "fifos in use: %d; free: %d; open: %d; fresh: %d\n",
	    n_inuse, n_free, n_open, n_fresh);

    len = strlen(mybuf);
    strncpy(buf, mybuf, (size > len) ? len : size);
    return len;
}

/*ARGSUSED*/
errstat
impl_fifo_bufsize(h, obj, size)
header *h;
objnum  obj;
long   *size;
{
    /* I wish everything was as simple as this.. */
    *size = FIFO_BUF_SIZE;
    return STD_OK;
}


errstat
fifo_act(priv, objret)
private *priv;
objnum  *objret;
{
    /* Called by Ail-generated main loop to find and check object after
     * receiving a request.  TODO: getting the appropriate lock right away
     * may become necessary when preemptive threads are introduced.
     */
    struct fifo_t *f;
    struct fp     *fp;
    rights_bits    rights;
    port           check;
    objnum         obj;

    obj = prv_number(priv);
    /* check which of the three kinds this request is about */
    if (obj == 0) {
	check = Super_check;
    } else if ((f = fifo_entry(obj)) != NULL) {
	check = f->f_check;
    } else if ((fp = fp_entry(obj)) != NULL) {
	check = fp->fp_check;
    } else {
	return STD_CAPBAD;
    }

    if (prv_decode(priv, &rights, &check) == 0) {
	*objret = obj;
	return STD_OK;
    } else {
	return STD_CAPBAD;
    }
}

/*ARGSUSED*/
void
fifo_deact(obj)
objnum obj;
{
    /* Nothing to do; (un)locking is done by the implementation functions. */
}


void
init_fifo(numfifo, checkfields)
int  numfifo;
port checkfields[];
{
    int i;
    time_t tim;

    time(&tim);
    strncpy(Start_time, ctime(&tim), sizeof(Start_time));

    mu_init(&State_Mu);
    mu_init(&Fp_Mu);
    mu_init(&Fifo_Mu);
    
    Num_Fifo = numfifo;
    Num_Fp = 2 * Num_Fifo;
    Fifo_Tab = (struct fifo_t *)
			malloc((size_t) (Num_Fifo * sizeof(struct fifo_t)));
    Fp_Tab = (struct fp *) malloc((size_t) (Num_Fp * sizeof(struct fp)));
    if (Fifo_Tab == NULL || Fp_Tab == NULL) {
	fatal("cannot allocate fifo data");
    }

    for (i = 0; i < Num_Fifo; i++) {
	struct fifo_t *f = &Fifo_Tab[i];

	if (NULLPORT(&checkfields[i])) {
	    f->f_status = F_FREE;
	} else {
	    fifo_init(f);
	    f->f_check = checkfields[i];
	}
	f->f_objnum = (objnum) (FIRST_VALID_FIFO + i);
    }

    for (i = 0; i < Num_Fp; i++) {
	struct fp *fp = &Fp_Tab[i];

	fp->fp_fifo = NIL_FIFO;
	fp->fp_count = 0;
	fp->fp_objnum = (objnum) (FIRST_VALID_FP + i);
    }
}
