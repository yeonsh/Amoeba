/*	@(#)fifosvr.h	1.4	96/02/27 10:13:19 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* Created:  apr 20 93   Ron Visser
 * Modified: june 28 93  Kees Verstoep
 */

#include <amtools.h>
#include <semaphore.h>
#include <circbuf.h>
#include <ajax/mymonitor.h>
#include <thread.h>
#include <server/bullet/bullet.h>

#define VERSION		"1.1"

#define FIFO_STACK	8000	/* enough since trans buf is malloced */
#define FIFO_BUF_SIZE	30000
#define NUM_THREADS	8

#define NIL_FP		((struct fp *) 0)
#define NIL_FIFO	((struct fifo_t *) 0)

#define R_SIDE		0
#define W_SIDE		1
#define OTHER_SIDE(s)	(((s) == W_SIDE) ? R_SIDE : W_SIDE)

#define NUM_FIFO	100
#define MIN_NUM_FIFO	10
#define MAX_NUM_FIFO	3000
#define NUM_FP		(2 * NUM_FIFO)
#define MAX_LIFETIME	6	/* hours */

#define ALL_RIGHTS	BS_RGT_ALL
#define USER_RIGHTS	((rights_bits) (BS_RGT_CREATE | BS_RGT_READ | \
					BS_RGT_MODIFY | BS_RGT_DESTROY))


/* Every call to open creates an entry in the fp ("fifo pointer") table.
 * All entries belonging to the same fifo point to a common entry in
 * the fifo table.  A slot is free if fp_fifo is a nil pointer.
 * When accessing a fp structure, the lock on the corresponding fifo
 * structure should be gotten first.
 */
struct fp {
    long   fp_flags;	/* flags from fifo_open */
    int    fp_count;	/* how often is this slot shared */
    objnum fp_objnum;	/* object number of this entry */
    int    fp_lifetime;	/* for garbage collection */
    port   fp_check;	/* for protection */
    struct fifo_t *fp_fifo; /* pointer to fifo table */
};

/* The semaphore is used in case of blocking fifo_open() */
struct fifo_t {
    semaphore f_wait;	/* to wait when not opened with O_NONBLOCK */
    mutex  f_mu;	/* mutex for this fifo and related fps */
    int    f_status;	/* is this entry currently used? */
    int    f_users[2];	/* nr of slots(not users) using each side */
    int    f_cbclosed;	/* closed, need to be reopened */
    objnum f_objnum;	/* object number of this entry */
    int    f_lifetime;	/* for garbage collection */
    port   f_check;	/* for protection */
    struct circbuf *f_cb; /* Circular buffer */
};

/* values for f_status: */
#define F_FREE		0
#define F_INUSE		1
#define F_DELETE	2

extern port Super_check;
extern capability Pub_cap;
