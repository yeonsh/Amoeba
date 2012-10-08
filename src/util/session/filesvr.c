/*	@(#)filesvr.c	1.8	96/02/27 13:14:03 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Tiny file server used to share bullet files between processes */

#include <amtools.h>
#define _POSIX_SOURCE
#include <limits.h>
#include <ajax/mymonitor.h>
#include <ajax/fdstuff.h> /* For flag defs, e.g., FREAD */
#include <ajax/ajax.h>
#include <bullet/bullet.h>
#include <unistd.h>
#include <module/rnd.h>

#include "share.h"
#include "pipelink.h"
#include "session.h"

#define NTHREADS	2
#define STACKSIZE	(BS_REQBUFSZ + 3000)
#define INCR_FILES	10   /* when out of slots, allocate INCR_FILES extra */

struct filedesc {
	long flags; /* Zero if closed */
	long pos;
	long size;
	int mode;
	long mtime;
	capability cap;
	capability dircap;
	char name[NAME_MAX+1];
	port check;
	struct object *shared;
	mutex mu;
};

static struct filedesc **ftab;	/* dynamic array of pointers to
				 * dynamically allocated filedesc structs
				 */
static int ftab_len = 0;	/* currently allocated # of filedesc slots */

static port svrport, cltport;

static void start_filesvr_watchdog();

static struct filedesc *
lock_file(priv, object)
private	*priv;
int	*object;
/* If priv designates a legal file object, lock and return its descriptor.
 * Otherwise return NULL.
 * The object number (whether OK or not) is put in *object.
 */
{
    struct filedesc *f;
    rights_bits rights;
    int obj;

    *object = obj = prv_number(priv);

    if (obj < 0 || obj >= ftab_len) {
	MON_NUMEVENT("lock_file: bad object number", obj);
	return(NULL);
    }

    f = ftab[obj];
    mu_lock(&f->mu);

    if (f->flags == 0 || prv_decode(priv, &rights, &f->check) < 0) {
	mu_unlock(&f->mu);
	MON_NUMEVENT("filesvr: bad object", obj);
	return(NULL);
    } else {
	/* return locked descriptor */
	return(f);
    }
}

/*ARGSUSED*/
static void
filesvr(arg, len)
	char *arg;
	int len;
{
	char buf[BS_REQBUFSZ];
	header h;
	bufsize n, nreply;
	int i;
	struct filedesc *f;
	errstat err;
	b_fsize nread;
	int pid;
	struct object *shared;
	
	MON_EVENT("filesvr: starting");
	
	for (;;) {
		h.h_port = svrport;
		n = getreq(&h, buf, sizeof buf);
		if (ERR_STATUS(n)) {
			err = ERR_CONVERT(n);
			if (err == RPC_ABORTED) {
				MON_EVENT("filesvr: restart");
			} else {
				warning("filesvr: getreq failed (%s)",
					err_why(err));
			}
			continue;
		}
		MON_NUMEVENT("filesvr: got command", h.h_command);

		if ((f = lock_file(&h.h_priv, &i)) == NULL) {
			MON_NUMEVENT("filesvr: bad object", i);
			h.h_status = STD_CAPBAD;
			putrep(&h, NILBUF, 0);
			continue;
		}
		MON_NUMEVENT("filesvr: command applies to valid object", i);
		nreply = 0;
		
		/* NB: each case must unlock the mutex as soon as possible */
		switch (h.h_command) {
		
		case FSQ_READ:
			if (!(f->flags & FREAD)) {
				mu_unlock(&f->mu);
				MON_EVENT("filesvr: read request denied");
				h.h_status = STD_DENIED;
				break;
			}
			if (h.h_size > sizeof buf)
				h.h_size = sizeof buf;
			MON_NUMEVENT("filesvr: read request, size", h.h_size);
			if (f->flags & FNONCOMMITTED) {
			    h.h_status = err = b_modify(&f->cap, 0L, (char *)0,
							0L, BS_COMMIT,&f->cap);
			    if (err != STD_OK) {
				mu_unlock(&f->mu);
				MON_NUMEVENT("b_read: commit failed", err);
				break;
			    }
			    f->flags &= ~(FNONCOMMITTED|FSTORED);
			}
			h.h_status = err = b_read(&f->cap, (b_fsize)f->pos,
				buf, (b_fsize)h.h_size, &nread);
			if (err == STD_OK) {
				if (h.h_size > nread)
					h.h_extra = FSE_NOMOREDATA;
				else
					h.h_extra = FSE_MOREDATA;
				h.h_size = nreply = nread;
				f->pos += nread;
				MON_NUMEVENT("filesvr: b_read nread", nread);
			}
			else
				MON_NUMEVENT("filesvr: b_read err", err);
			mu_unlock(&f->mu);
			break;
		
		case FSQ_WRITE:
			if (!(f->flags & FWRITE)) {
				mu_unlock(&f->mu);
				MON_EVENT("filesvr: write reqest denied");
				h.h_status = STD_DENIED;
				break;
			}
			MON_NUMEVENT("filesvr: write request, size", h.h_size);
			h.h_status = err = b_modify(&f->cap, (b_fsize)f->pos,
				buf, (b_fsize)n, 0, &f->cap);
			if (err == STD_OK) {
				f->flags |= FNONCOMMITTED;
				h.h_size = n;
				f->pos += n;
				if (f->pos > f->size)
					f->size = f->pos;
				MON_EVENT("filesvr: b_modify OK");
			}
			else
				MON_NUMEVENT("filesvr: b_modify err", err);
			mu_unlock(&f->mu);
			break;
		
		case PIPE_LINK:
			shared = f->shared;
			mu_unlock(&f->mu);
			pid = prv_number(&((capability *)buf)->cap_priv);
			MON_NUMEVENT("filesvr: pipe-link, pid", pid);
			ob_share(shared, pid);
			h.h_status = STD_OK;
			break;
		
		case PIPE_UNLINK:
			shared = f->shared;
			mu_unlock(&f->mu);
			pid = prv_number(&((capability *)buf)->cap_priv);
			MON_NUMEVENT("filesvr: pipe-unlink, pid", pid);
			ob_unshare(shared, pid);
			h.h_status = STD_OK;
			break;
		
		default:
			mu_unlock(&f->mu);
			MON_NUMEVENT("filesvr: bad command", h.h_command);
			h.h_status = STD_COMBAD;
			break;
		
		}
		putrep(&h, buf, nreply);
	}
}

static void
close_file(f)
	struct filedesc *f;
{
	errstat err;
	
	MON_EVENT("filesvr.close_file");
	mu_lock(&f->mu);
	if (f->flags == 0) {
		warning("filesvr: closing already closed file");
		mu_unlock(&f->mu);
		return;
	}
	if (f->flags & FWRITE) {
		MON_EVENT("filesvr.close_file: FWRITE set");
		if ((f->flags & FNONCOMMITTED)) {
			MON_EVENT("filesvr.close_file: committing");
			err = b_modify(&f->cap, (b_fsize)f->pos,
				NILBUF, (b_fsize)0, BS_COMMIT, &f->cap);
			if (err == STD_OK)
				f->flags &= ~(FNONCOMMITTED | FSTORED);
			else
				MON_NUMEVENT(
				    "filesvr.close_file: b_modify err", err);
		}
		if (!(f->flags & FSTORED)) {
			MON_EVENT("filesvr.close_file: appending/replacing");
			err = _ajax_install((f->flags & FREPLACE) != 0,
					    &f->dircap, f->name, &f->cap);
			if (err == STD_OK)
				f->flags |= FSTORED;
			else
				MON_NUMEVENT(
				    "filesvr.close_file: replace err", err);
		}
	}
	f->flags = 0;
	mu_unlock(&f->mu);
	MON_EVENT("filesvr.close_file: done");
}

int
extend_file_table()
/*
 * Try to extend the file table with INCR_FILES entries, and initialise them.
 * Returns 0 on failure; 1 on success.  If it fails, ftab and ftab_len are
 * kept unchanged.
 */
{
	struct filedesc **new_ftab;
	struct filedesc *new_filedescs;
	int new_length;

	/* allocate INCR_FILES new slots */
	new_filedescs = (struct filedesc *)
	    malloc((size_t)(INCR_FILES * sizeof(struct filedesc)));
	if (new_filedescs == NULL) {
		/* allocation failure */
		return 0;
	}

	/* create/extend the index array */
	if (ftab_len == 0) {
		new_length = INCR_FILES;
		new_ftab = (struct filedesc **)
		    malloc((size_t)(new_length * sizeof(struct filedesc *)));
	} else {
		new_length = ftab_len + INCR_FILES;
		new_ftab = (struct filedesc **)realloc((_VOIDSTAR) ftab,
			     (size_t)(new_length * sizeof(struct filedesc *)));
	}

	if (new_ftab == NULL) {
		/* allocation failure, keep ftab/fteb_len unchanged */
		free((_VOIDSTAR) new_filedescs);
		return 0;
	} else {
		int i;

		/* link new slots to index table and initialise a few fields */
		for (i = 0; i < INCR_FILES; i++) {
			int ftab_index = ftab_len + i;

			new_ftab[ftab_index] = &new_filedescs[i];
			new_ftab[ftab_index]->flags = 0;
			mu_init(&new_ftab[ftab_index]->mu);
		}

		ftab = new_ftab;
		ftab_len = new_length;
		return 1;
	}
}	

static mutex ftab_mu; /* protect against file table race conditions */

int
startfilesvr(pid, flags, pos, size, mode, mtime, cap, dircap, name, cap_ret)
    /*in:*/
	int pid;
	long flags;
	long pos;
	long size;
	int mode;
	long mtime;
	capability *cap;
	capability *dircap;
	char name[NAME_MAX+1];
    /*out:*/
	capability *cap_ret;
{
	int i;
	static int nstarted;
	struct filedesc *f;
	
	if ((flags & (FREAD|FWRITE)) == 0) {
		MON_EVENT("startfilesvr: neither FREAD nor FWRITE");
		return STD_ARGBAD;
	}
	
	if (nstarted == 0) {
		uniqport(&svrport);
		priv2pub(&svrport, &cltport);
		mu_init(&ftab_mu);
		start_filesvr_watchdog();
	}
	for (i = nstarted; i < NTHREADS; ++i) {
		if (thread_newthread(filesvr, STACKSIZE, (char *)0, 0))
			++nstarted;
	}
	if (nstarted == 0) {
		MON_EVENT("startfilesvr: no server threads started");
		return STD_NOSPACE;
	}
	
	MON_EVENT("startfilesvr");
	mu_lock(&ftab_mu);

	/* Look for free slot in table */
	for (i = 0; i < ftab_len; ++i) {
		f = ftab[i];
		if (f->flags == 0) { /* Found a free slot */
			/* Try to lock it to see if we can grab it */
			if (mu_trylock(&f->mu, (interval) 0) < 0)
				continue;
			if (f->flags != 0) {
				mu_unlock(&f->mu);
				continue;
			}

			/* all checks passed, break out of the loop */
			break;
		}
	}

	if (i == ftab_len) {
		/* all current slots in use, allocate new ones */
		if (!extend_file_table()) {
			warning("filesvr: cannot allocate more file slots");
			mu_unlock(&ftab_mu);
			return STD_NOSPACE;
		}
		f = ftab[i];
		mu_lock(&f->mu);
	}

	mu_unlock(&ftab_mu);
	MON_NUMEVENT("startfilesvr: using file slot", i);

	f->flags = flags;
	f->pos = pos;
	f->size = size;
	f->mode = mode;
	f->mtime = mtime;
	f->cap = *cap;
	f->dircap = *dircap;
	strncpy(f->name, name, NAME_MAX);
	uniqport(&f->check);
	cap_ret->cap_port = cltport;
	(void) prv_encode(&cap_ret->cap_priv,
			  (objnum)i, PRV_ALL_RIGHTS, &f->check);
	f->shared = ob_new(pid, close_file, (char *)f);
	mu_unlock(&f->mu);
	return STD_OK;
}


int
file_seek(filecap, pos, mode, new_off)
capability *filecap;
long	    pos;
int  	    mode;
long 	   *new_off;
/* perform lseek() on shared file */
{
    struct filedesc *f;
    int obj;

    if ((f = lock_file(&filecap->cap_priv, &obj)) == NULL) {
	MON_NUMEVENT("file_seek: invalid object", obj);
	return(-1);
    }

    switch (mode) {
    case SEEK_SET:
	break;
    case SEEK_CUR:
	pos += f->pos;
	break;
    case SEEK_END:
	if (f->size < 0) {
	    errstat err;

	    if ((err = b_size(&f->cap, &f->size)) != STD_OK) {
		MON_NUMEVENT("file_seek: b_size failed", err);
		mu_unlock(&f->mu);
		return(err);
	    }
	}
	pos += f->size;
	break;
    default:
	MON_NUMEVENT("file_seek: invalid seek mode", mode);
	mu_unlock(&f->mu);
	return(STD_DENIED);
    }
    if (pos < 0) {
	MON_NUMEVENT("file_seek: invalid seek position", pos);
	mu_unlock(&f->mu);
	return(STD_DENIED);
    }
    f->pos = pos;
    *new_off = pos;

    mu_unlock(&f->mu);
    return(0);
}

int
file_size(filecap, size)
capability *filecap;
long 	   *size;
/* return size of shared file (for fstat emulation) */
{
    struct filedesc *f;
    int obj;

    if ((f = lock_file(&filecap->cap_priv, &obj)) == NULL) {
	MON_NUMEVENT("file_size: invalid object", obj);
	return(-1);
    }

    if (f->size < 0) {	/* if it is >= 0, it is the current size */
	errstat err;

	if ((err = b_size(&f->cap, &f->size)) != STD_OK) {
	    mu_unlock(&f->mu);
	    return(err);
	}
    }
    *size = f->size;

    mu_unlock(&f->mu);
    return(0);
}


#if BS_CACHE_TIMEOUT < 4
 #error I do not believe this: uncommitted bullet files time out much too fast!
#else
/* we have to be a bit faster (say 3 minutes) than the bullet timeout: */
#define TIMEOUT_SECS    (60*(BS_CACHE_TIMEOUT - 3))
#endif

/*ARGSUSED*/
static void
filesvr_watchdog(p, s)
char * p;
int s;
{
/*
 * Regularly touches shared uncommitted bullet files to avoid losing them.
 */
    for (;;) {
        register int obj;

        /* Do what watchdogs do most of the time: */
        (void) sleep(TIMEOUT_SECS);

	mu_lock(&ftab_mu);

        /* Time for some action: touch uncommitted bullet files */
	for (obj = 0; obj < ftab_len; obj++) {
	    struct filedesc *f;

	    f = ftab[obj];
	    mu_lock(&f->mu);

	    if ((f->flags & FWRITE) != 0) {
		char buf[1];
		errstat err;

		/* touch the uncomitted file by writing 0 bytes at offset 0. */
		err = b_modify(&f->cap, (b_fsize) 0, buf, (b_fsize) 0,
			       0, &f->cap);
		if (err == STD_OK) {
		    f->flags |= FNONCOMMITTED;
                } else {
		    warning("filesvr watchdog: error touching \"%s\" (%s)",
			    f->name, err_why(err));
		}
            }

	    mu_unlock(&f->mu);
        }

	mu_unlock(&ftab_mu);
    }
}

static void
start_filesvr_watchdog()
/* starts the file descriptor watchdog in a new thread */
{
    static int started = 0;

    if (!started) {
        started = 1;
        (void) thread_newthread(filesvr_watchdog, 2000, (char *)NULL, 0);
    }
}

