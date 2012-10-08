/*	@(#)iop.c	1.9	96/02/27 13:53:03 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * iop.c
 *
 * Machine (Sun3) dependent IOP routines, including the interface for
 * keyboard and mouse interrupts.
 *
 * Author:
 *	Leendert van Doorn
 * Modified:
 *	Gregory J. Sharp May 1994 - mouse driver was all wrong.
 */
#include <map.h>
#include <amoeba.h>
#include <ampolicy.h>
#include <machdep.h>
#include <semaphore.h>
#include <debug.h>
#include <assert.h>
INIT_ASSERT
#include <cmdreg.h>
#include <stdcom.h>
#include <stderr.h>
#include <module/mutex.h>
#include "harddep.h"
#include <string.h>
#include <iop/iop.h>
#include <iop/colourmap.h>
#include <iop/iopsvr.h>

#define XPIXELS	(SCR_COR_X-SCR_ORG_X)
#define YPIXELS (SCR_COR_Y-SCR_ORG_Y)
#define XBYTES	(XPIXELS/8)

/* Used by the mouse driver below - it needs to be initialised! */
static int old_buttons;


void
iop_initdev()
{
    /* Initial state of buttons is released */
    old_buttons = 7;
}

errstat
iop_doenable _ARGS(( void ))
{
    printf("[IOP server enabled]\n");
    displaymapped();
    return STD_OK;
}

void
iop_dodisable _ARGS(( void ))
{
    displayunmapped();
    printf("[IOP server disabled]\n");
}

/* ARGSUSED */
errstat
iop_domousecontrol(mp)
    IOPmouseparams *	mp;
{
    return STD_COMBAD;
}


errstat
iop_doframebufferinfo(fbptr)
    IOPFrameBufferInfo ** fbptr;
{
    static IOPFrameBufferInfo fbinfo;
    extern phys_bytes bmaddr;

    /* Fill in fbinfo struct for X server */
    fbinfo.type = 1;				/* screen-specific type */
    fbinfo.width = SCR_COR_X-SCR_ORG_X;		/* width in pixels */
    fbinfo.height = SCR_COR_Y-SCR_ORG_Y;	/* height in pixels */
    fbinfo.depth = 1;				/* depth (bits/pixel) */
    fbinfo.stride = SCR_COR_X-SCR_ORG_X;	/* stride (scanline length) */
    fbinfo.xmm = 240;				/* width in mm */
    fbinfo.ymm = 200;				/* height in mm */
    (void) strcpy(fbinfo.name, "bwtwo");	/* name of the device type */

    /*
     * Get a segment capability for the memory where the frame buffer
     * is located and store it in the frame buffer info structure.
     */
    if (HardwareSeg(&fbinfo.fb, bmaddr+KERNELSTART,
		    (phys_bytes) XBYTES*YPIXELS, 0) != 0) {
	printf("iopsvr: cannot create frame buffer segment\n");
	return STD_SYSERR;
    }
    *fbptr = &fbinfo;
    return STD_OK;
}

/* ARGSUSED */
errstat
iop_domapmemory(cap)
    capability * cap;
{
    return STD_COMBAD;
}

/* ARGSUSED */
errstat
iop_dounmapmemory(cap)
    capability * cap;
{
    return STD_COMBAD;
}

/*
 * Disable device interrupts.
 */
errstat
iop_dointdisable _ARGS(( void ))
{
    return STD_COMBAD;
}

/*
 * Enable device interrupts.
 */
errstat
iop_dointenable _ARGS(( void ))
{
    return STD_COMBAD;
}

errstat
iop_dokeyclick(volume)
    int volume;
{
    assert(volume >= 0 && volume <= 100);
    if (volume == 0)
	stopclick();
    else
	startclick();
    return STD_OK;
}

/* ARGSUSED */
errstat
iop_dosetleds(leds)
    long leds;
{
    /* Sun 3 keybd doesn't have leds */
    return STD_COMBAD;
}

/* ARGSUSED */
errstat
iop_dogetleds(leds)
    long * leds;
{
    /* Sun 3 keybd doesn't have leds */
    return STD_COMBAD;
}

/* ARGSUSED */
errstat
iop_dobell(volume, pitch, duration)
    int volume;
    int pitch;
    int duration;
{
    bleep();
    return STD_OK;
}

errstat
iop_dokbdtype(type)
    int *	type;
{
    *type = 3;
    return STD_OK;
}

/*ARGSUSED*/
errstat
iop_dosetcmap(cmapp)
colour_map * cmapp;
{
    return STD_COMBAD;
}

/*
 * Keyboard interrupt; highest bit is the make/break bit,
 * the lower 6 bits represent the actual key code.
 */
void
keyboard_intr(byte)
    char byte;
{
    newevent(byte & 0x80 ? EV_KeyReleaseEvent : EV_KeyPressEvent,
	byte & 0x7f, -1, -1);
}

/*
 * Driver routine for Mouse System's 5-byte protocol at 1200 Baud.
 */
void
mouse_intr(byte)
    int byte;
{
    static char buffer[5];
    static int count;
    int b, buttons, dx, dy;

    if (count == 0)
    {
	/* Wait for the 1000 0xxx mouse sequence introducer */
	if ((byte & 0xF8) == 0x80)
	{
	    count = 1;
	    buffer[0] = (char) byte;
	}
	else
	{
	    DPRINTF(0, ("mouse_intr: byte 0x%x instead of sync\n", byte));
	}
    }
    else
    {
	/* Stash all other input, until we have 5 bytes */
	buffer[count++] = (char) byte;
	if (count < 5)
	    return;

	/* If any button changed, generate an event for it */
	buttons = buffer[0] & 0x07;
	if (buttons != old_buttons) {
	    for (b = 0; b < 3; b++) {
		int ev, mask = 1 << (2 - b);
		if ((buttons & mask) != (old_buttons & mask)) {
		    if (buttons & mask)
			ev = EV_ButtonRelease;
		    else
			ev = EV_ButtonPress;
		    /* keys are numbered 1, 2, and 3 */
		    newevent(ev, b+1, -1, -1);
		}
	    }
	    old_buttons = buttons;
	}

	dx = (int) buffer[1] + (int) buffer[3];
	dy = -((int) buffer[2] + (int) buffer[4]);

	/* if any mouse movement, generate an event for it */
	if (dx || dy)
	    newevent(EV_PointerDelta, 0, dx, dy);
	count = 0;
    }
}
