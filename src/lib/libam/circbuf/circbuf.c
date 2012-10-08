/*	@(#)circbuf.c	1.5	94/04/07 09:57:20 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** Circular buffer package.
**
** Circular buffers are used to transfer a stream of bytes between a
** reader and a writer, usually in different threads.  The stream is
** ended after the writer closes the stream; when the reader has read
** the last byte, the next read call returns an end indicator.  Flow
** control is simple: the reader will block when no data is immediately
** available, and the writer will block when no buffer space is
** immediately available.
**
** This package directly supports concurrent access by multiple readers
** and/or writers.
*/

#include "amoeba.h"
#include "semaphore.h"
#include "module/mutex.h"
#include "stdlib.h"
#include "string.h"
#include "circbuf.h"

#if DEBUG
#include <server/ip/debug.h>
#endif

struct circbuf {
    mutex	full;		/* locked iff no data in buffer */
    mutex	empty;		/* locked iff no free space in buffer */
    mutex	lock;		/* serializes access to other members */
    /*
    ** Mutex full is used to block until at least one byte of data
    ** becomes available.  Similarly, you can block on empty until at
    ** least one free byte becomes available in the buffer.
    ** To prevent deadlock, always acquire full or empty before lock,
    ** and never acquire full and empty together.
    ** When closed, full and empty should always be unlocked.
    */
    char	*first, *in, *out, *last;
    int		nbytes;
    /*
    ** Data is between out and in, and wraps from last to first.
    ** When in == out, use nbytes to see if the buffer is full or empty.
    */
    int		getpcount;	/* getp counter */
    int		putpcount;	/* putp counter */
    int		closed;	/* when set, no new data is accepted */
    semaphore	*usema;		/* Pointer to optional external semaphore */
    /*
    ** When usema is used, it will be 'upped' by N each time N more
    ** bytes become available for reading.  When the cb is closed, usema
    ** will be 'upped' by one more so a reader can detect the closing.
    ** (This only works on the reader side -- there is no equivalent
    ** function for writers.)
    **
    ** NOTE: It requires considerable care to get all the data out.
    ** XXX Explain how to do it.
    */
};

/*
** cb_alloc -- allocate a new circular buffer of a given size.
** Return a nil pointer if not enough memory is available.
** The buffer sits immediately following the control structure,
** in the same malloc'ed block.
*/
struct circbuf *
cb_alloc(size)
    int size;
{
    register struct circbuf *cb;

    if (size <= 0)
    	abort(); /* We have no mercy for illegal calls */
    cb = (struct circbuf *)
    	malloc(sizeof(struct circbuf) + size);
    if (cb == 0)
    	return 0; /* Not enough memory */
    cb->first = cb->in = cb->out = (char *)&cb[1];
    cb->last  = cb->first + size;
    mu_init(&cb->full);
    mu_init(&cb->empty);
    mu_init(&cb->lock);
    mu_lock(&cb->full);
    cb->nbytes = 0;
    cb->getpcount = cb->putpcount = 0;
    cb->closed = 0;
    cb->usema = 0;
    return cb;
}

/*
** cb_free -- free a circular buffer structure.
** This should only be called if all readers and writers agree not to
** use the cb anymore.  If there is any doubt, use an extra semaphore.
*/
void
cb_free(cb)
    struct circbuf *cb;
{
    free((char *)cb);
}

/*
** cb_setsema -- set external semaphore pointer.
** See comments in the struct definition for an explanation.
** Call this at most once per allocated circular buffer.
*/
void
cb_setsema(cb, sema)
    struct circbuf *cb;
    semaphore *sema;
{
    mu_lock(&cb->lock);
    if (sema == 0 || cb->usema != 0)
    	abort(); /* Illegal call -- no mercy */
    cb->usema = sema;
    sema_mup(sema, cb->nbytes + (cb->closed != 0));
   mu_unlock(&cb->lock);
}

/*
** cb_close -- set closed flag.
** May be called as often as you want, by readers and writers.
** Once closed, no new data can be pushed into the buffer,
** but data already in it is still available to readers.
*/
void
cb_close(cb)
    register struct circbuf *cb;
{
    mu_lock(&cb->lock);
    if (cb->closed) {
    	mu_unlock(&cb->lock);
    	return;
    }
    if (cb->nbytes == 0)
	mu_unlock(&cb->full);
    else if (cb->in == cb->out)
	mu_unlock(&cb->empty);
    cb->closed = 1;
    if (cb->usema)
    	sema_up(cb->usema);
    mu_unlock(&cb->lock);
}

/*
** cb_full -- return number of available data bytes.
** When closed and there are no bytes available, return -1.
*/
int
cb_full(cb)
    register struct circbuf *cb;
{
    register int n;

    mu_lock(&cb->lock);
    n = (cb->nbytes == 0 && cb->closed)? -1: cb->nbytes;
    mu_unlock(&cb->lock);
    return n;
}

/*
** cb_empty -- return number of available free bytes.
** Return -1 if closed (this can be used as a test for closedness).
*/
int
cb_empty(cb)
    register struct circbuf *cb;
{
    register int n;

    mu_lock(&cb->lock);
    if (cb->closed)
    	n = -1;
    else
	n = (cb->last - cb->first) - cb->nbytes;
    mu_unlock(&cb->lock);
    return n;
}

/*
** cb_putc -- put char into cb.
** Return 0 if OK, -1 if closed.
*/
int
cb_putc(cb, c)
    register struct circbuf *cb;
    int c;
{
    mu_lock(&cb->empty);
    mu_lock(&cb->lock);
    if (cb->closed) {
    	mu_unlock(&cb->lock);
    	mu_unlock(&cb->empty);
    	return -1;
    }
    if (cb->nbytes == 0)
    	mu_unlock(&cb->full); /* First byte being added to empty buffer */
    *cb->in++ = c;
    if (cb->in == cb->last)
    	cb->in = cb->first;
    cb->nbytes++;
    if (cb->in != cb->out)
        mu_unlock(&cb->empty); /* Buffer is not yet full */
    if (cb->usema)
    	sema_up(cb->usema);
    mu_unlock(&cb->lock);
    return 0;
}

static int cb_intgetc();

/*
** cb_getc -- get next char from cb.
** Return char (always >= 0) if OK, -1 if closed and no more data.
*/
int
cb_getc(cb)
    register struct circbuf *cb;
{
    mu_lock(&cb->full);
    return cb_intgetc(cb);
}

/*
** cb_trygetc -- get char from cb with timeout as for mu_trylock.
** Return -1 if closed or no char appears within time.
*/
int
cb_trygetc(cb, maxtimeout)
    register struct circbuf *cb;
    int maxtimeout;
{
    if (mu_trylock(&cb->full, (interval)maxtimeout) < 0)
    	return -1;
    return cb_intgetc(cb);
}

/*
** cb_intgetc -- internal routine to complete cb_getc and cb_trygetc.
*/
static int
cb_intgetc(cb)
    register struct circbuf *cb;
{
    unsigned char c;

    mu_lock(&cb->lock);
    if (cb->nbytes == 0) {
    	if (!cb->closed)
    	    abort(); /* Got past full, not closed, no bytes??? */
    	mu_unlock(&cb->lock);
    	mu_unlock(&cb->full);
    	return -1;
    }
    if (cb->in == cb->out)
    	mu_unlock(&cb->empty); /* Buffer no longer full */
    c = *cb->out++;
    if (cb->out == cb->last)
    	cb->out = cb->first;
    cb->nbytes--;
    if (cb->nbytes != 0 || cb->closed)
        mu_unlock(&cb->full); /* Buffer is not yet empty */
    mu_unlock(&cb->lock);
    return c;
}

/*
** cb_puts -- put n bytes into cb.
*/
int
cb_puts(cb, s, n)
    register struct circbuf *cb;
    register char *s;
    register int n;
{
    while (n > 0) {
        auto char *ptr;
	register int k = cb_putp(cb, &ptr, 1);
	if (k < 0)
		return -1;
	if (k > n)
		k = n;
	memmove(ptr, s, k);
	cb_putpdone(cb, k);
	s += k;
	n -= k;
    }
    return 0;
}

/*
** cb_gets -- get between minlen and maxlen bytes from circbuf.
*/
int
cb_gets(cb, s, minlen, maxlen)
    register struct circbuf *cb;
    register char *s;
    int minlen, maxlen;
{
    register int count = 0;

    while( count < maxlen) {
        auto char *ptr;
        register int k = cb_getp(cb, &ptr, /*Block if*/ count < minlen);
	if (k <= 0)
		break;
	if (k > maxlen - count)
	    k = maxlen - count;
	memmove(s + count, ptr, k);
	cb_getpdone(cb, k);
	count += k;
    }
    return count;
}

/*
** cb_getp -- get pointer to next data bytes.
** The 'blocking' parameter is interpreted as follows:
**	-1	block interruptible
**	 0	don't block
**	+1	block non-interruptible.
** An interruptible block may return an error when a signal was received.
** Return count of available bytes (0 if none).
** If nonzero return, a call to cb_getpdone must follow to announce how
** many bytes were actually consumed.
*/
int
cb_getp(cb, ps, blocking)
    register struct circbuf *cb;
    char **ps;
    int blocking;
{
    if (cb->getpcount != 0)
    	abort(); /* Previous cb_getp still active */
    if (blocking > 0) {
	mu_lock(&cb->full);
    }
    else {
	if (mu_trylock(&cb->full, (interval)blocking) < 0)
	    return 0;
    }
    mu_lock(&cb->lock);
    if (cb->nbytes == 0) {
    	if (!cb->closed)
    	    abort(); /* Got past full, not closed, no bytes??? */
    	mu_unlock(&cb->lock);
    	mu_unlock(&cb->full);
    	return 0;
    }
    *ps = cb->out;
    cb->getpcount = cb->in > cb->out ? cb->nbytes : cb->last - cb->out;
    mu_unlock(&cb->lock);
    return cb->getpcount;
}

void
cb_getpdone(cb, len)
    register struct circbuf *cb;
    int len;
{
    if (cb->getpcount == 0 || len < 0 || len > cb->getpcount)
    	abort(); /* Require 0 <= len <= getpcount, and previous cb_getp call */
    /* We get here with cb->full locked */
    mu_lock(&cb->lock);
    if (len == 0) {
        mu_unlock(&cb->full);
    }
    else {
        if (cb->in == cb->out)
            mu_unlock(&cb->empty); /* Buffer no longer full */
        cb->out += len;
        if (cb->out == cb->last)
            cb->out = cb->first;
        cb->nbytes -= len;
        if (cb->nbytes != 0 || cb->closed)
            mu_unlock(&cb->full); /* Buffer is not yet empty */
	cb->getpcount = 0;
    }
    mu_unlock(&cb->lock);
}

/*
** cb_putp -- get pointer to next free bytes.
** The 'blocking' parameter is interpreted as for cb_getp.
** Returns count of available bytes (0 if failure).
** If nonzero return, a call to cb_putpdone must follow.
*/
int
cb_putp(cb, ps, blocking)
    register struct circbuf *cb;
    char **ps;
    int blocking;
{
    if (cb->putpcount != 0)
    	abort(); /* Should've called cb_getpdone() first */
    if (blocking > 0) {
	mu_lock(&cb->empty);
    }
    else {
	if (mu_trylock(&cb->empty, (interval)blocking) < 0)
	    return 0;
    }
    mu_lock(&cb->lock);
    if (cb->closed) {
    	mu_unlock(&cb->lock);
    	mu_unlock(&cb->empty);
#if DEBUG
 { where(); printf("cb_putp(0x%x, 0x%x, %d)= -1\n", cb, ps, blocking); 
	endWhere(); }
#endif
    	return -1;
    }
    *ps = cb->in;
    cb->putpcount = (cb->out > cb->in ? cb->out : cb->last) - cb->in;
    mu_unlock(&cb->lock);
#if DEBUG
 if (cb->putpcount <= 0)
 { where(); printf("cb_putp(0x%x, 0x%x, %d)= %d\n", cb, ps, blocking,
	cb->putpcount); endWhere(); }
#endif
    return cb->putpcount;
}

void
cb_putpdone(cb, len)
    register struct circbuf *cb;
    int len;
{
    if (cb->putpcount == 0 || len < 0 || len > cb->putpcount)
    	abort(); /* Require 0 <= len <= putpcount and previous cb_putp call */
    /* We get here with cb->empty locked */
    mu_lock(&cb->lock);
    if (len == 0 || cb->closed) {
        mu_unlock(&cb->empty);
    }
    else {
        if (cb->nbytes == 0)
            mu_unlock(&cb->full); /* Buffer no longer empty */
        cb->in += len;
        if (cb->in == cb->last)
        	cb->in = cb->first;
        cb->nbytes += len;
        if (cb->out != cb->in)
            mu_unlock(&cb->empty); /* Buffer is not yet full */
        if (cb->usema)
            sema_mup(cb->usema, len);
    }
    cb->putpcount = 0;
    mu_unlock(&cb->lock);
}
