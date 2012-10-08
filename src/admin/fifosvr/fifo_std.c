/*	@(#)fifo_std.c	1.3	94/04/06 11:45:06 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* 
 * Created:
 *	apr 20 93   Ron Visser
 * Modified:
 *	june 23 93  Kees Verstoep: rewritten; interface made suitable for Ail.
 */

#include "fifosvr.h"
#include <module/buffers.h>
#include <stdobjtypes.h>
#include <bullet/bullet.h>
#include "fifo_std.h"
#include "fifo_impl.h"
#include "fifo_main.h"

/*ARGSUSED*/
errstat
impl_std_restrict(h, obj, mask, newcap)
header     *h;
objnum      obj;
long        mask;
capability *newcap;
{
    struct fifo_t *f;
    struct fp     *fp;
    rights_bits    rights;
    port           check;

    if (obj == 0) {
	check = Super_check;
    } else if ((f = fifo_entry(obj)) != NULL) {
	check = f->f_check;
    } else if ((fp = fp_entry(obj)) != NULL) {
	check = fp->fp_check;
    } else {
	return STD_CAPBAD;
    }

    if (prv_decode(&h->h_priv, &rights, &check) == 0) {
	if (prv_encode(&newcap->cap_priv, obj,
		       rights & (rights_bits) mask, &check) == 0) {
	    newcap->cap_port = h->h_port;
	    return STD_OK;
	} else {
	    return STD_CAPBAD;
	}
    } else {
	return STD_CAPBAD;
    }
}

/*ARGSUSED*/
errstat
impl_std_info(h, obj, reply, max_reply, size, retlen)
header *h;
objnum  obj;
char   *reply;
int	max_reply;
int	size;
int    *retlen;
{
    struct fifo_t *f;
    int actual_size;
    char mybuf[80];
    char *buf;

    if (size < 0) {
	return STD_ARGBAD;
    }
    if (size > max_reply) {
	size = max_reply;
    }
    if (obj == 0) {
	buf = "fifo server";
    } else if ((f = fifo_entry(obj)) != NULL) {
	fifo_lock(f);
	sprintf(mybuf, "%s r = %d; w = %d",
		OBJSYM_PIPE, f->f_users[R_SIDE], f->f_users[W_SIDE]);
	fifo_unlock(f);
	buf = mybuf;
    } else {
	return STD_ARGBAD;
    }

    /* return the full size; the client will check for overflow */
    actual_size = strlen(buf);
    strncpy(reply, buf, (size_t) ((actual_size > size) ? size : actual_size));
    *retlen = actual_size;

    return STD_OK;
}

/*ARGSUSED*/
errstat
impl_std_status(h, obj, reply, max_reply, size, retlen)
header *h;
objnum  obj;
char   *reply;
int	max_reply;
int     size;
int    *retlen;
{
    struct fifo_t *f;

    if (size < 0) {
	return STD_ARGBAD;
    }
    if (size > max_reply) { /* truncate */
	size = max_reply;
    }
    if (obj == 0) {
	*retlen = fifo_global_stat(reply, size);
    } else if ((f = fifo_entry(obj)) != NULL) {
	*retlen = fifo_stat(f, reply, size);
    } else {
	return STD_ARGBAD;
    }

    return STD_OK;
}

/*ARGSUSED*/
errstat
impl_std_touch(h, obj)
header *h;
objnum  obj;
{
    /* check that it's the super cap or a fifo entry */
    if (obj != 0 && fifo_entry(obj) == NIL_FIFO) {
	return STD_CAPBAD;
    }

    /* looking up a fifo has already resets its time-to-live field */
    return STD_OK;
}

errstat
impl_std_destroy(h, obj)
header *h;
objnum  obj;
{
    rights_bits rights;
    errstat err;
    struct fifo_t *f;

    /* check if the fifo is curently open */
    if ((f = fifo_entry(obj)) == NIL_FIFO) {
	return STD_CAPBAD;
    }
    /* check the cap has the DELETE right */
    if (prv_decode(&h->h_priv, &rights, &f->f_check) != 0) {
	return STD_CAPBAD;
    }
    if ((rights & BS_RGT_DESTROY) == 0) {
	return STD_DENIED;
    }
    
    if ((err = del_fifo(f)) == STD_OK) {
	update_statefile();
    }

    return err;
}

void 
init_std()
{
}
