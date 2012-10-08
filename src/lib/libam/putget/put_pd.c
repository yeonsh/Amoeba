/*	@(#)put_pd.c	1.4	96/02/27 11:04:13 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** buf_put_pd
**	This routine puts a process descriptor plus subsequent segment
**	descriptors and thread descriptors into a buffer.  It does not
**	pack the thread_ustate because this is so machine dependent (it
**	is a fault frame) that anyone who gets it had better know what
**	to do with it themselves.  To avoid alignment problems it is copied
**	byte for byte into the buffer and must be retrieved that way.
**	Like all other buf_put_/buf_get_ routines it returns 0 if the buffer is
**	not large enough.  Otherwise it returns a pointer to the next
**	available byte (after the process descriptor) in the buffer.
**	NB: If the thread descriptors have invalid lengths then we return 0!
*/

#include "amoeba.h"
#include "module/proc.h"
#include "module/buffers.h"
#include "string.h"

/* Forward declarations */
static char *	buf_put_process_d _ARGS((char *, char *, process_d *));
static char *	buf_put_segment_d _ARGS((char*, char*, segment_d *));
static char *	buf_put_thread_d _ARGS((char*, char*, thread_d *));
static char *	buf_put_thread_idle _ARGS((char*, char*, thread_idle *));
static char *	buf_put_thread_kstate _ARGS((char*, char*, thread_kstate *));

char *
buf_put_pd(buf, bufend, pd)
char *		buf;		/* start of buffer to hold process descr. */
char *		bufend;		/* end of buffer to hold process descr. */
process_d *	pd;		/* pointer to process descr. to put */
{
    int			i;	/* loop counter */
    segment_d *		segp;
    thread_d *		tdp;
    thread_idle *	tdi;
    thread_kstate *	tdk;
    char *		p;
    int			tocopy;	/* # bytes of thread_ustate to copy */

    if (buf == 0 || buf > bufend)
	return 0;

/* put the process descriptor struct into the buffer */
    buf = buf_put_process_d(buf, bufend, pd);

/* put the segment descriptor structs into the buffer */
    for (segp = PD_SD(pd), i = pd->pd_nseg; --i >= 0; segp++) 
	buf = buf_put_segment_d(buf, bufend, segp);

/*
** Put the thread descriptor structs into the buffer.
** These beasties may be followed by a thread_idle, thread_kstate and/or
** thread_ustate so we check to see what is there and put it in the buffer.
*/
    for (tdp = PD_TD(pd), i = pd->pd_nthread; --i >= 0; tdp = TD_NEXT(tdp))
    {
	buf = buf_put_thread_d(buf, bufend, tdp);
	p = (char *) (tdp + 1);
	if (tdp->td_extra & TDX_IDLE)
	{
	    tdi = (thread_idle *) p;
	    p = (char *) (tdi + 1);
	    buf = buf_put_thread_idle(buf, bufend, tdi);
	}
	if (tdp->td_extra & TDX_KSTATE)
	{
	    tdk = (thread_kstate *) p;
	    p = (char *) (tdk + 1);
	    buf = buf_put_thread_kstate(buf, bufend, tdk);
	}
    /*
    ** If there is still more thread descriptor left we copy it
    ** as raw bytes since it is probably the thread_ustate which
    ** is a fault frame and we really can't encode it since we don't
    ** know what it is.  The receiver must work it out itself.
    */
	tocopy = (int) tdp->td_len - (p - (char *) tdp);
	if (tocopy < 0 || buf == 0 || buf + tocopy > bufend)
	    return 0;
	if (tocopy > 0)
	{
	    (void) memmove(buf, p, tocopy);
	    buf += tocopy;
	}
    }
    
    return buf;
}


/*
** buf_put_process_d
**	The reason we test to see if ARCHSIZE bytes fit and not a whole
**	process_d is that we only need to be sure we don't write beyond
**	the buffer ourselves. buf_put_cap(), etc don't write beyond the end
**	of the buffer.
*/

static char *
buf_put_process_d(buf, bufend, pd)
char *		buf;		/* start of buffer to hold process descr */
char *          bufend;         /* end of buffer */
process_d *	pd;
{
    if (buf == 0 || buf > bufend || buf + ARCHSIZE > bufend)
	return 0;
    (void) memmove(buf, pd->pd_magic, ARCHSIZE);
    buf += ARCHSIZE;
    buf = buf_put_cap(buf, bufend, &pd->pd_self);
    buf = buf_put_cap(buf, bufend, &pd->pd_owner);
    buf = buf_put_uint16(buf, bufend, pd->pd_nseg);
    buf = buf_put_uint16(buf, bufend, pd->pd_nthread);
    return buf;
}


/*
** buf_put_segment_d
**	puts the capability and 4 longs of this struct into the buffer.
**	buf_put_cap() & buf_put_int32() check to make sure that they fit.
*/

static char *
buf_put_segment_d(buf, bufend, sdp)
char *		buf;		/* start of buffer to hold process descr */
char *		bufend;		/* end of buffer */
segment_d *	sdp;		/* pointer to segment descr struct to put */
{
    buf = buf_put_cap(buf, bufend, &sdp->sd_cap);
    buf = buf_put_int32(buf, bufend, sdp->sd_offset);
    buf = buf_put_int32(buf, bufend, sdp->sd_addr);
    buf = buf_put_int32(buf, bufend, sdp->sd_len);
    buf = buf_put_int32(buf, bufend, sdp->sd_type);
    return buf;
}


/*
** buf_put_thread_d
**	puts the 3 longs of this struct into the buffer.
**	buf_put_int32() checks to make sure that they fit.
*/

static char *
buf_put_thread_d(buf, bufend, td)
char *		buf;		/* start of buffer to hold process descr */
char *		bufend;		/* end of buffer */
thread_d *	td;		/* pointer to thread descr struct to put */
{
    buf = buf_put_int32(buf, bufend, td->td_state);
    buf = buf_put_int32(buf, bufend, td->td_len);
    buf = buf_put_int32(buf, bufend, td->td_extra);
    return buf;
}


/*
** buf_put_thread_idle
**	puts the 2 longs of this struct into the buffer.
**	buf_put_int32() checks to make sure that they fit.
*/

static char *
buf_put_thread_idle(buf, bufend, tdi)
char *		buf;		/* start of buffer to hold process descr */
char *		bufend;		/* end of buffer */
thread_idle *	tdi;		/* pointer to thread_idle struct to put */
{
    buf = buf_put_int32(buf, bufend, tdi->tdi_pc);
    buf = buf_put_int32(buf, bufend, tdi->tdi_sp);
    return buf;
}


/*
** buf_put_thread_kstate
**	puts the 5 longs of this struct into the buffer.
**	buf_put_int32() checks to make sure that they fit.
*/

static char *
buf_put_thread_kstate(buf, bufend, tdk)
char *		buf;		/* start of buffer to hold process descr */
char *		bufend;		/* end of buffer */
thread_kstate *	tdk;		/* pointer to thread_kstate struct to put */
{
    buf = buf_put_int32(buf, bufend, tdk->tdk_timeout);
    buf = buf_put_int32(buf, bufend, tdk->tdk_sigvec);
    buf = buf_put_int32(buf, bufend, tdk->tdk_svlen);
    buf = buf_put_int32(buf, bufend, tdk->tdk_signal);
    buf = buf_put_int32(buf, bufend, tdk->tdk_local);
    return buf;
}
