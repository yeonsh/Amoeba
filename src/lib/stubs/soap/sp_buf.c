/*	@(#)sp_buf.c	1.2	94/04/07 11:08:42 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "sp_stubs.h"
#include "sp_buf.h"
#include "module/mutex.h"

/* For big requests/results we maintain a pool of soap buffers.
 * As only a minor subset of the commands need them, they are allocated
 * on the fly, when needed.
 */

static union sp_buf *sp_buf_list; /* no "=NULL" for the sake of kernel in rom */
static mutex sp_buf_mu;

union sp_buf *
sp_getbuf()
{
    union sp_buf *buf;

    mu_lock(&sp_buf_mu);
    if (sp_buf_list == NULL) {	/* try to allocate new one */
	buf = (union sp_buf *) malloc(sizeof(union sp_buf));
    } else {			/* reuse previous one */
	buf = sp_buf_list;
	sp_buf_list = sp_buf_list->sp_buf_next;
    }
    mu_unlock(&sp_buf_mu);

    return buf;
}

void
sp_putbuf(buf)
union sp_buf *buf;
{
    mu_lock(&sp_buf_mu);
    buf->sp_buf_next = sp_buf_list;
    sp_buf_list = buf;
    mu_unlock(&sp_buf_mu);
}
