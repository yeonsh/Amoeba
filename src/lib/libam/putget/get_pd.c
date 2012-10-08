/*	@(#)get_pd.c	1.5	96/02/27 11:04:04 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** buf_get_pd
**	This routine gets a process descriptor plus subsequent segment
**	descriptors and thread descriptors from a buffer.  It does not
**	unpack the thread_ustate because this is so machine dependent (it
**	is a fault frame) that anyone who gets it had better know what
**	to do with it themselves.  We just dump it at the end of the thread
**	descriptor and hope that the receiver can figure out alignment, etc.
**	Like all other buf_put_/buf_get_ routines it returns 0 if the buffer is
**	not large enough.  Otherwise it returns a pointer to the next
**	available byte (after the process descriptor) in the buffer.
**	NB: we work out the biggest the process_d could possibly be and
**	malloc a chunk of memory ourselves.  We then return a pointer to the
**	to the position in memory immediately after the pd.
**	NB: If the thread descriptors have invalid lengths then we return 0!
*/

#include "amoeba.h"
#include "module/proc.h"
#include "stdlib.h"
#include "string.h"
#include "module/buffers.h"

/* Forward declarations */
static char *	buf_get_process_d _ARGS((char *, char *, process_d *));
static char *	buf_get_segment_d _ARGS((char*, char*, segment_d *));
static char *	buf_get_thread_d _ARGS((char*, char*, thread_d *));
static char *	buf_get_thread_idle _ARGS((char*, char*, thread_idle *));
static char *	buf_get_thread_kstate _ARGS((char*, char*, thread_kstate *));


char *
buf_try_get_pd(buf, bufend, pd, nthreadp)
register char *	buf;		/* start of buffer that holds process descr. */
char *		bufend;		/* end of buffer that holds process descr. */
process_d *	pd;		/* pointer to place to put process descr. */
int *		nthreadp;	/* number of thread descriptors found */
{
    int			i;	/* loop counter */
    segment_d *		segp;
    thread_d *		tdp;
    thread_idle *	tdi;
    thread_kstate *	tdk;
    char *		p;
    char *		lastthreadp;
    int			tocopy;	/* # bytes of thread_ustate to copy */

/* get the process descriptor struct from the buffer */
    buf = buf_get_process_d(buf, bufend, pd);
    if (buf == 0)
	return 0;

/* get the segment descriptor structs from the buffer */
    for (segp = PD_SD(pd), i = pd->pd_nseg; --i >= 0; segp++) 
	buf = buf_get_segment_d(buf, bufend, segp);
    if (buf == 0)
	return 0;

/*
** Try to get as much thread descriptor structs from the buffer as possible.
** These beasties may be followed by a thread_idle, thread_kstate and/or
** thread_ustate so we have to get them as well.
*/
    lastthreadp = buf;
    for (tdp = PD_TD(pd), i = 0; i < (int) pd->pd_nthread; tdp = TD_NEXT(tdp), i++)
    {
	buf = buf_get_thread_d(buf, bufend, tdp);
	if (buf == 0)
	    break;
	p = (char *) (tdp + 1);
	if (tdp->td_extra & TDX_IDLE)
	{
	    tdi = (thread_idle *) p;
	    p = (char *) (tdi + 1);
	    buf = buf_get_thread_idle(buf, bufend, tdi);
	}
	if (tdp->td_extra & TDX_KSTATE)
	{
	    tdk = (thread_kstate *) p;
	    p = (char *) (tdk + 1);
	    buf = buf_get_thread_kstate(buf, bufend, tdk);
	}
    /*
    ** If there is still more thread descriptor left we copy it
    ** as raw bytes since it is probably the thread_ustate which
    ** is a fault frame and we really can't encode it since we don't
    ** know what it is.  The receiver can work it out itself.
    */
	tocopy = (int) tdp->td_len - (p - (char *) tdp);
	if (tocopy < 0)	/* invalid */
	    return 0;
	if (buf == 0 || buf + tocopy > bufend)
	    break;
	if (tocopy > 0)
	{
	    (void) memmove(p, buf, tocopy);
	    buf += tocopy;
	}
	lastthreadp = buf;
    }
    
    *nthreadp = i;
    return lastthreadp;
}


char *
buf_get_pd(buf, bufend, pd)
register char *	buf;		/* start of buffer that holds process descr. */
char *		bufend;		/* end of buffer that holds process descr. */
process_d *	pd;		/* pointer to place to put process descr. */
{
    int nthreads;
    char *result;

    result = buf_try_get_pd(buf, bufend, pd, &nthreads);
    if (result != 0 && nthreads != pd->pd_nthread) {
	/* process descriptor was truncated */
	result = 0;
    }
    return result;
}

/*
** pdmalloc works out how big a buffer is needed for buf_get_pd and mallocs it.
** It isn't built into buf_get_pd since sometimes we already have a buffer and
** don't need to malloc.  It is a litte less efficient since we unpack the
** process_d struct twice if pdmalloc is called.
*/

process_d *
pdmalloc(buf, bufend)
char *	buf;
char *	bufend;
{
    process_d		procd;	/* temporary copy of process_d in buffer */
    register unsigned	max_pd_size, max_input_size;


    max_input_size = bufend - buf;
/* get the process descriptor struct from the buffer */
    buf = buf_get_process_d(buf, bufend, &procd);
    if (buf == 0)
	return 0;

/* allocate a buffer to hold the new process descriptor */
    max_pd_size = sizeof (process_d) +
		  sizeof (segment_d) * procd.pd_nseg +
		  MAX_THREAD_SIZE * procd.pd_nthread;

/* it cannot be more than the whole buffer */
    if (max_pd_size > max_input_size)
	max_pd_size = max_input_size;

    return (process_d *) malloc(max_pd_size);
}


/*
** buf_get_process_d
**	This applies a following sanity check to catch random data
**	offered as process descriptor:
**
**		pd_magic must be a string of at most ARCHSIZE-1
**		printable characters padded to ARCHSIZE with zero bytes.
*/

static char *
buf_get_process_d(buf, bufend, pd)
char *		buf;		/* start of buffer to hold process descr */
char *          bufend;         /* end of buffer */
process_d *	pd;
{
    int		i;

/*
** The reason we test to see if ARCHSIZE bytes fit and not a whole
** process_d is that we only need to be sure we don't write beyond
** the buffer ourselves. buf_get_cap(), etc don't write beyond the
** end of the buffer.
*/
    if (buf == 0 || buf > bufend || buf + ARCHSIZE > bufend)
	return 0;

/* Sanity check for pd_magic */
    for (i = 0; i < ARCHSIZE-1; i++)
	if (buf[i] <= ' ' || buf[i] >= 0x7f)
	    break;
    for (; i < ARCHSIZE; i++)
	if (buf[i] != '\0')
	    return 0;

    (void) memmove(pd->pd_magic, buf, ARCHSIZE);
    buf += ARCHSIZE;
    buf = buf_get_cap(buf, bufend, &pd->pd_self);
    buf = buf_get_cap(buf, bufend, &pd->pd_owner);
    buf = buf_get_uint16(buf, bufend, &pd->pd_nseg);
    buf = buf_get_uint16(buf, bufend, &pd->pd_nthread);
    return buf;
}


/*
** buf_get_segment_d
**	gets the capability and 4 longs of the segment descriptor struct from
**	the buffer.
**	buf_get_cap() & buf_get_int32() check for the end of the buffer.
*/

static char *
buf_get_segment_d(buf, bufend, sdp)
char *		buf;		/* start of buffer to hold process descr */
char *		bufend;		/* end of buffer */
segment_d *	sdp;		/* pointer to segment descr struct to get */
{
    buf = buf_get_cap(buf, bufend, &sdp->sd_cap);
    buf = buf_get_int32(buf, bufend, &sdp->sd_offset);
    buf = buf_get_int32(buf, bufend, &sdp->sd_addr);
    buf = buf_get_int32(buf, bufend, &sdp->sd_len);
    buf = buf_get_int32(buf, bufend, &sdp->sd_type);
    return buf;
}


/*
** buf_get_thread_d
**	gets the 3 longs of the thread descriptor struct from the buffer.
**	buf_get_int32() checks for the end of the buffer.
*/

static char *
buf_get_thread_d(buf, bufend, td)
char *		buf;		/* start of buffer to hold process descr */
char *		bufend;		/* end of buffer */
thread_d *	td;		/* pointer to thread descr struct to get */
{
    buf = buf_get_int32(buf, bufend, &td->td_state);
    buf = buf_get_int32(buf, bufend, &td->td_len);
    buf = buf_get_int32(buf, bufend, &td->td_extra);
    return buf;
}


/*
** buf_get_thread_idle
**	gets the 2 longs of the thread_idle struct from the buffer.
**	buf_get_int32() checks for the end of the buffer.
*/

static char *
buf_get_thread_idle(buf, bufend, tdi)
char *		buf;		/* start of buffer to hold process descr */
char *		bufend;		/* end of buffer */
thread_idle *	tdi;		/* pointer to thread_idle struct to get */
{
    buf = buf_get_int32(buf, bufend, &tdi->tdi_pc);
    buf = buf_get_int32(buf, bufend, &tdi->tdi_sp);
    return buf;
}


/*
** buf_get_thread_kstate
**	gets the 5 longs of the thread kernel state struct from the buffer.
**	buf_get_int32() checks for the end of the buffer.
*/

static char *
buf_get_thread_kstate(buf, bufend, tdk)
char *		buf;		/* start of buffer to hold process descr */
char *		bufend;		/* end of buffer */
thread_kstate *	tdk;		/* pointer to thread_kstate struct to get */
{
    buf = buf_get_int32(buf, bufend, &tdk->tdk_timeout);
    buf = buf_get_int32(buf, bufend, &tdk->tdk_sigvec);
    buf = buf_get_int32(buf, bufend, &tdk->tdk_svlen);
    buf = buf_get_int32(buf, bufend, &tdk->tdk_signal);
    buf = buf_get_int32(buf, bufend, &tdk->tdk_local);
    return buf;
}
