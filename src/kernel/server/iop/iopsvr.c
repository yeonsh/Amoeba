/*	@(#)iopsvr.c	1.11	96/02/27 14:13:48 */
/*
 * Copyright 1996 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * iop.c
 *
 * The machine independent part of the IOP server. This file contains
 * the code to handle event queue management, and IOP requests.
 *
 * Author:
 *	Leendert van Doorn
 * Modified:
 *	Gregory J. Sharp, Oct 1993 - added marshalling of framebuffer info.
 */
#include <amoeba.h>
#include <ampolicy.h>
#include <machdep.h>
#include <semaphore.h>
#include <cmdreg.h>
#include <stdcom.h>
#include <stderr.h>
#include <iop/iop.h>
#include <iop/colourmap.h>
#include <iop/iopsvr.h>
#include <bool.h>
#include <string.h>
#include <sys/proto.h>
#include <module/mutex.h>
#include <module/rnd.h>
#include <module/prv.h>
#include <module/buffers.h>
#include <assert.h>
INIT_ASSERT

#define BUFSIZE	1024

#ifndef NR_IOPSVR_THREADS
#define NR_IOPSVR_THREADS	2
#endif
#ifndef IOPSVR_STKSIZ
#define IOPSVR_STKSIZ		0 /* default size */
#endif

/*
 * All events are stored in a circular buffer, which is
 * protected by mutexes and semaphores.
 */
#define	NEVENTS	256
static IOPEvent	event_buffer[NEVENTS];
static int		event_in, event_out;
static semaphore	event_sema;
static mutex		event_mutex;
static capability	iopcap;		/* our capability */

int			iop_enabled;	/* when turned on X is in control */


/*
 * Signal catcher
 */
/*ARGSUSED*/
static void
iopcatcher(thread_no, sig)
int thread_no;
signum sig;
{
    /* ignore signals */
}

/*
 * Get requested number of events from circular buffer
 */
static errstat
getevents(size, evbuf, num_events)
    bufsize size;	 /* in:  max #events to return in evbuf */
    char *evbuf;	 /* in:  pointer to buffer where events are returned */
    bufsize *num_events; /* out: the # events received */
{
    int nevents;

    if (mu_trylock(&event_mutex, (interval) -1) < 0)
	return STD_INTR;
    if (size > BUFSIZE/sizeof(IOPEvent))
	size = BUFSIZE/sizeof(IOPEvent);

    nevents = sema_trymdown(&event_sema, (int) size, (interval) -1);
    if (nevents <= 0) {
	mu_unlock(&event_mutex);
	return STD_INTR;
    }

    if (event_out + nevents > NEVENTS) {
	/* copy in two steps */
	size_t n = NEVENTS - event_out;

	(void) memmove((_VOIDSTAR) evbuf,
			(_VOIDSTAR) (event_buffer + event_out),
			n * sizeof (IOPEvent));
	(void) memmove((_VOIDSTAR) (evbuf + (n * sizeof (IOPEvent))),
			(_VOIDSTAR) event_buffer,
			(size_t) (nevents - n) * sizeof (IOPEvent));
	event_out = nevents - n;
    } else {
	(void) memmove((_VOIDSTAR) evbuf,
			(_VOIDSTAR) (event_buffer + event_out),
			(size_t) nevents * sizeof (IOPEvent));
	event_out += nevents;
	if (event_out == NEVENTS)
	    event_out = 0;
    }
    mu_unlock(&event_mutex);
    *num_events = nevents;
    return STD_OK;
}

/*
 * IOP server
 */
static void
iopserver()
{
    getsig(iopcatcher, 0);
    for (;;) {
	bufsize reqlen, replen;
	bufptr rbufptr;
	char reqbuf[BUFSIZE];
	header hdr;
	rights_bits rights;
	int click_volume;

	hdr.h_port = iopcap.cap_port;
#ifdef USE_AM6_RPC
	reqlen = rpc_getreq(&hdr, reqbuf, BUFSIZE);
#else
	reqlen = getreq(&hdr, reqbuf, BUFSIZE);
#endif
	if (ERR_STATUS(reqlen)) {
	    printf("iopsvr: getreq returned %d\n", reqlen);
	    continue;
	}

	/* By default assume an empty reply buffer.  Commands that do
	 * return data will override this.
	 */
	rbufptr = NULL;
	replen = 0;

	/* Must have all rights except for STD_INFO command */
	if (prv_decode(&hdr.h_priv, &rights, &iopcap.cap_priv.prv_random) == 0
		 && (rights == PRV_ALL_RIGHTS || hdr.h_command == STD_INFO)) {
	    switch (hdr.h_command) {

	    /*
	     * Generic enable/disable requests
	     */
	    case IOP_ENABLE:
		if (reqlen != 0)
		    hdr.h_status = STD_ARGBAD;
		else
		    hdr.h_status = iop_doenable();
		if (hdr.h_status == STD_OK)
		    iop_enabled = TRUE;
		break;
	    case IOP_DISABLE:
		if (reqlen != 0)
		    hdr.h_status = STD_ARGBAD;
		else {
		    /* never disallow a disable request! */
		    iop_dodisable();
		    iop_enabled = FALSE;
		    hdr.h_status = STD_OK;
		}
		break;

	    /*
	     * For IOP servers that support more than one mouse protocol.
	     * This request is harmless so we don't worry to much about
	     * the IOP server being enabled.
	     */
	    case IOP_MOUSECONTROL:
		if (reqlen != 0)
		    hdr.h_status = STD_ARGBAD;
		else {
		    IOPmouseparams ms;

		    ms.mp_type = hdr.h_extra & IOP_CHORDMIDDLE;
		    ms.mp_type = hdr.h_extra & ~IOP_CHORDMIDDLE;
		    ms.mp_baudrate = hdr.h_offset;
		    ms.mp_samplerate = hdr.h_size;
		    hdr.h_status = iop_domousecontrol(&ms);
		}
		break;

	    /*
	     * Suns map in the frame buffer using this request,
	     * it also returns some display dependent information.
	     */
	    case IOP_FRAMEBUFFERINFO:
		if (reqlen != 0)
		    hdr.h_status = STD_ARGBAD;
		else
		    if (iop_enabled) {
			IOPFrameBufferInfo * fbinfo;

			hdr.h_status = iop_doframebufferinfo(&fbinfo);
			if (hdr.h_status == STD_OK) {
			    char *	p;
			    char *	e;

			    /* Pack the struct */
			    assert(BUFSIZE >= sizeof (IOPFrameBufferInfo));
			    e = reqbuf + sizeof (IOPFrameBufferInfo);
			    p = buf_put_long(reqbuf, e, fbinfo->type);
			    p = buf_put_short(p, e, (short) fbinfo->width);
			    p = buf_put_short(p, e, (short) fbinfo->height);
			    p = buf_put_short(p, e, (short) fbinfo->depth);
			    p = buf_put_short(p, e, (short) fbinfo->stride);
			    p = buf_put_short(p, e, (short) fbinfo->xmm);
			    p = buf_put_short(p, e, (short) fbinfo->ymm);
			    p = buf_put_cap(p, e, &fbinfo->fb);
			    p = buf_put_bytes(p, e, (_VOIDSTAR) fbinfo->name, IOPNAMELEN);
			    if (p == 0)
				hdr.h_status = STD_SYSERR;
			    else
			    {
				rbufptr = reqbuf;
				replen = p - reqbuf;
			    }
			}
		    }
		    else
			hdr.h_status = STD_DENIED;
		break;

	    /*
	     * The vga driver just maps in a 64 Kb memory bank of the frame
	     * buffer. For the bank switching to work it also needs to map
	     * in some I/O ports.
	     */
	    case IOP_MAPMEMORY:
		if (reqlen != sizeof (capability))
			hdr.h_status = STD_ARGBAD;
		else
		    if (iop_enabled) {
			rbufptr = reqbuf;
			hdr.h_status = iop_domapmemory((capability *) reqbuf);
		    }
		    else
			hdr.h_status = STD_DENIED;
		replen = hdr.h_status == STD_OK ? sizeof (capability) : 0;
		break;

	    case IOP_UNMAPMEMORY:
		if (reqlen != sizeof (capability))
		    hdr.h_status = STD_ARGBAD;
		else {
		    if (iop_enabled) {
			rbufptr = reqbuf;
			hdr.h_status = iop_dounmapmemory((capability *) reqbuf);
		    }
		    else
			hdr.h_status = STD_DENIED;
		}
		replen = hdr.h_status == STD_OK ? sizeof (capability) : 0;
		break;

	    /*
	     * Enable/disable all device interrupts. This way the X-server
	     * can perform some timing depending operations (like estimating
	     * the graphic board's clock frequencies).
	     */
	    case IOP_INTDISABLE:
		if (reqlen != 0)
		    hdr.h_status = STD_ARGBAD;
		else
		    if (iop_enabled)
			hdr.h_status = iop_dointdisable();
		    else
			hdr.h_status = STD_DENIED;
		break;

	    case IOP_INTENABLE:
		if (reqlen != 0)
		    hdr.h_status = STD_ARGBAD;
		else
		    if (iop_enabled)
			hdr.h_status = iop_dointenable();
		    else
			hdr.h_status = STD_DENIED;
		break;

	    /*
	     * Various requests to control keyboard specific things
	     */
	    case IOP_KEYCLICK:
		if (iop_enabled) {
		    click_volume = hdr.h_extra;
		    if (click_volume < 0 || click_volume > 100)
			hdr.h_status = STD_ARGBAD;
		    else
			hdr.h_status = iop_dokeyclick(click_volume);
		}
		else
		    hdr.h_status = STD_DENIED;
		break;

	    case IOP_SETLEDS:
		if (reqlen != 0)
		    hdr.h_status = STD_ARGBAD;
		else
		    if (iop_enabled)
			hdr.h_status = iop_dosetleds(hdr.h_offset);
		    else
			hdr.h_status = STD_DENIED;
		break;

	    case IOP_GETLEDS:
		if (reqlen != 0)
		    hdr.h_status = STD_ARGBAD;
		else
		    if (iop_enabled)
			hdr.h_status = iop_dogetleds(&hdr.h_offset);
		    else
			hdr.h_status = STD_DENIED;
		break;

	    case IOP_BELL:
		if (reqlen != 0)
		    hdr.h_status = STD_ARGBAD;
		else {
		    if (iop_enabled) {
			hdr.h_status = iop_dobell((int) hdr.h_extra,
						  (int) hdr.h_offset,
						  (int) hdr.h_size);
		    } else {
			hdr.h_status = STD_DENIED;
		    }
		}
		break;

	    case IOP_KBDTYPE:
		if (reqlen != 0)
		    hdr.h_status = STD_ARGBAD;
		else {
		    if (iop_enabled) {
			hdr.h_status = iop_dokbdtype((int *) &hdr.h_offset);
		    } else {
			hdr.h_status = STD_DENIED;
		    }
		}
		break;

	    case IOP_SETCMAP:
	    {
		/* Set the colour map */

		char * p;
		char * end;
		colour_map cmap;

		if (iop_enabled) {
		    end = reqbuf + reqlen;
		    p = buf_get_long(reqbuf, end, (long *) &cmap.c_index);
		    p = buf_get_long(p, end, (long *) &cmap.c_count);
		    if (p == 0 || (end - p) != 3 * cmap.c_count)
			hdr.h_status = STD_ARGBAD;
		    else {
			cmap.c_red = (uint8 *) p;
			cmap.c_green = cmap.c_red + cmap.c_count;
			cmap.c_blue = cmap.c_green + cmap.c_count;
			hdr.h_status = iop_dosetcmap(&cmap);
		    }
		} else {
		    hdr.h_status = STD_DENIED;
		}
		break;
	    }

	    /*
	     * Return the events we've stored
	     */
	    case IOP_GETEVENTS:
		if (iop_enabled) {
		    hdr.h_status = getevents(hdr.h_size, reqbuf, &hdr.h_size);
		    rbufptr = reqbuf;
		    replen = hdr.h_size * sizeof (IOPEvent);
		} else
		    hdr.h_status = STD_DENIED;
		break;

	    /*
	     * Some standard commands we support
	     */
	    case STD_INFO:
		hdr.h_status = STD_OK;
		rbufptr = "screen/keyboard/mouse";
		hdr.h_size = replen = strlen(rbufptr);
		break;
	    case STD_STATUS:
		/* we use the reqbuf since it is probably big enough */
		replen = hdr.h_size = iopdump(reqbuf, reqbuf + BUFSIZE);
		rbufptr = reqbuf;
		hdr.h_status = STD_OK;
		break;
	    default:
		hdr.h_status = STD_COMBAD;
		break;
	    }
	} else
	    hdr.h_status = STD_CAPBAD;
#ifdef USE_AM6_RPC
	rpc_putrep(&hdr, rbufptr, replen);
#else
	putrep(&hdr, rbufptr, replen);
#endif
    }
}

/*
 * Initialize IOP server
 */
void
iopinit()
{
    capability pubcap;
    int i;

    mu_init(&event_mutex);
    sema_init(&event_sema, 0);
    uniqport(&iopcap.cap_port);
    uniqport(&iopcap.cap_priv.prv_random);
    if (prv_encode(&iopcap.cap_priv, (objnum) 0, PRV_ALL_RIGHTS,
      &iopcap.cap_priv.prv_random) < 0)
	panic("iopsvr: cannot prv_encode capability\n");

    /* publish the put capability of the server */
    pubcap.cap_priv = iopcap.cap_priv;
    priv2pub(&iopcap.cap_port, &pubcap.cap_port);

    (void) dirappend(DEF_IOPSVRNAME, &pubcap);
    for (i = 0; i < NR_IOPSVR_THREADS; i++)
	NewKernelThread(iopserver, (vir_bytes) IOPSVR_STKSIZ);
}

/*
 * Add a new event to the circular buffer.
 * BEWARE: this routine CANNOT be called at low interrupt level
 */
void
newevent(type, keyorbut, x, y)
    int type, keyorbut, x, y;
{
    register IOPEvent *evp;

    /* is there enough space in the buffer? */
    if (sema_level(&event_sema) >= NEVENTS-1)
	return;

    evp = event_buffer + event_in;
    if (++event_in == NEVENTS) event_in = 0;
    evp->type = type;
    evp->x = x;
    evp->y = y;
    evp->keyorbut = keyorbut;
    evp->time = getmilli();
    sema_up(&event_sema);
}

/*
 * Dump IOP statistics
 */
int
iopdump(begin, end)
    char *begin, *end;
{
    char *oldbegin = begin;
    register int nevent;

    begin = bprintf(begin, end, "Time=%d, #events=%d\n",
	getmilli(), sema_level(&event_sema));
    if ((nevent = event_out) == event_in) {
        begin = bprintf(begin, end, "No events.\n");
        return begin - oldbegin;
    }

    while (nevent != event_in) {
        begin = bprintf(begin, end, "%d: ", nevent);
	switch (event_buffer[nevent].type) {
	case EV_NoEvent:
	    begin = bprintf(begin, end, "NoEvent @%ld\n", 
		event_buffer[nevent].time);
	    break;
	case EV_KeyPressEvent:
	    begin = bprintf(begin, end, "KeyPressEvent 0x%x @%ld\n",
		event_buffer[nevent].keyorbut, event_buffer[nevent].time);
	    break;
	case EV_KeyReleaseEvent:
	    begin = bprintf(begin, end, "KeyReleaseEvent 0x%x @%ld\n",
		event_buffer[nevent].keyorbut, event_buffer[nevent].time);
	    break;
	case EV_ButtonPress:
	    begin = bprintf(begin, end, "ButtonPress 0x%x",
		event_buffer[nevent].keyorbut);
	    /* The 386 uses this to report all mouse events, including motion */
	    if (event_buffer[nevent].x != 0 || event_buffer[nevent].y != 0)
	    {
		begin = bprintf(begin, end, " Delta (%d, %d)",
		    event_buffer[nevent].x, event_buffer[nevent].y);
	    }
	    begin = bprintf(begin, end, " @%ld\n", event_buffer[nevent].time);
	    break;
	case EV_ButtonRelease:
	    begin = bprintf(begin, end, "ButtonRelease 0x%x @%ld\n",
		event_buffer[nevent].keyorbut, event_buffer[nevent].time);
	    break;
	case EV_PointerMotion:
	    begin = bprintf(begin, end, "PointerMotion (%d,%d) @%ld\n",
		event_buffer[nevent].x, event_buffer[nevent].y,
		event_buffer[nevent].time);
	    break;
	case EV_PointerDelta:
	    begin = bprintf(begin, end, "PointerDelta (%d,%d) @%ld\n",
		event_buffer[nevent].x, event_buffer[nevent].y,
		event_buffer[nevent].time);
	    break;
	default:
	    begin = bprintf(begin, end, "UnknownEvent (0x%x)\n",
		event_buffer[nevent].type);
	    break;
	}
        if (++nevent == NEVENTS) nevent = 0;
    }
    return begin - oldbegin;
}
