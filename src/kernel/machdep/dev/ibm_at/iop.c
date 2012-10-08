/*	@(#)iop.c	1.12	96/02/27 13:51:33 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * iop.c
 *
 * Machine (80386/VGA) dependent IOP routines, including the interface for
 * the various mouse protocols.
 *
 * Author:
 *	Leendert van Doorn
 */
#include <map.h>
#include <amoeba.h>
#include <ampolicy.h>
#include <machdep.h>
#include <cmdreg.h>
#include <stdcom.h>
#include <stderr.h>
#include <type.h>
#include <i80386.h>
#include <iop/iop.h>
#include <iop/colourmap.h>
#include <iop/iopsvr.h>
#include "i386_proto.h"

extern errstat mouse_init();
extern void mouse_enable(), mouse_disable();

static int Mouse_inited = 0;	/* set to 1 when the mouse is enabled */

uint16 *crt_graphmode();
void crt_textmode();

static uint16 *screen;

errstat
iop_doenable _ARGS(( void ))
{
    crt_enabledisplay();
    printf("[IOP server enabled]\n");
    screen = crt_graphmode();
    Mouse_inited = 0;
    /* Don't initialise the mouse yet; we don't know what kind is attached.
     * The iop_mousecontrol() will tell us what we should look for.
     */
    return STD_OK;
}

static void
iop_disable_mouse()
{
    if (Mouse_inited) {	/* disable current mouse */
	mouse_disable();
	Mouse_inited = 0;
    }
}

void
iop_dodisable _ARGS(( void ))
{
    crt_textmode(screen);
    screen = 0;
    iop_disable_mouse();
    printf("[IOP server disabled]\n");
}

errstat
iop_domousecontrol(mp)
    register IOPmouseparams *mp;
{
    errstat err;

    /* disable previous mouse before initialising the new one */
    iop_disable_mouse();
    if ((err = mouse_init(mp)) != STD_OK) {
	return err;
    }

    mouse_enable();
    Mouse_inited = 1;
    return STD_OK;
}

/* ARGSUSED */
errstat
iop_doframebufferinfo(fbptr)
    IOPFrameBufferInfo **fbptr;
{
    return STD_COMBAD;
}

/*
 * Make a segment containing the hardware screen.
 */
static errstat
mapmemory(addr, size, cap)
    phys_bytes addr;
    int size;
    capability *cap;
{
    if (HardwareSeg(cap, addr, (phys_bytes) size, 0) != 0) {
	printf("iopsvr: Cannot create screen segment\n");
	return STD_SYSERR;
    }
    return STD_OK;
}

/*
 * Map I/O address (specified by base, and size)
 */
static errstat
mapio(base, size)
    phys_bytes base;
    bufsize size;
{
    extern struct tss_s tss;
    char *bitmap;
    int ioaddr;

    if (base > MAX_TSS_IOADDR) 
	return STD_ARGBAD;
    if ((int) (base + size) < 0 || (base + size) > MAX_TSS_IOADDR) 
	return STD_ARGBAD;

    /*
     * Enable I/O permissions for ALL processes. Bits corresponding
     * to the I/O addresses will be cleared in the I/O permission
     * map in the kernel TSS. This will changed during the i80386
     * kernel revision, then each process will get its own TSS.
     */
    bitmap = (char *)&tss + SIZEOF_TSS;
    for (ioaddr = base; ioaddr < base + size; ioaddr++)
	bitmap[ioaddr >> 3] &= ~(1 << (ioaddr & 7));

    return STD_OK;
}

/* ARGSUSED */
errstat
iop_domapmemory(cap)
    capability *cap;
{
    /* XXX TEMPORARY HACK */
    if (mapio((phys_bytes) 0, MAX_TSS_IOADDR) != STD_OK)
	return STD_SYSERR;
    /* map in VGA video memory (base 0xA000, size 64Kb) */
    return mapmemory((phys_bytes) 0xA0000, 0x10000, cap);
}

/*
 * Unmap I/O address (specified by base, and size)
 */
static errstat
unmapio(base, size)
    phys_bytes base;
    int size;
{
    extern struct tss_s tss;
    char *bitmap;
    bufsize ioaddr;

    if (base > MAX_TSS_IOADDR) 
	return STD_ARGBAD;
    if ((int) (base + size) < 0 || (base + size) > MAX_TSS_IOADDR) 
	return STD_ARGBAD;

    /*
     * Disable I/O permissions for ALL processes. Bits corresponding
     * to the I/O addresses will be set n the I/O permission map in
     * the kernel TSS. This will changed during the i80386 kernel
     * revision, then each process will get its own TSS.
     */
    bitmap = (char *)&tss + SIZEOF_TSS;
    for (ioaddr = base; ioaddr < base + size; ioaddr++)
	bitmap[ioaddr >> 3] |= 1 << (ioaddr & 7);

    return STD_OK;
}

/* ARGSUSED */
errstat
iop_dounmapmemory(cap)
    capability * cap;
{
    /*
     * For now hardware segments cannot be destroyed.
     * This will be fixed in a later release:
     *
     *     std_destroy(cap);
     */
    /* XXX TEMPORARY HACK */
    if (unmapio((phys_bytes) 0, MAX_TSS_IOADDR) != STD_OK)
	return STD_SYSERR;
    return STD_OK;
}

/*
 * Disable device interrupts. This way the calling
 * process can handle some timing critical code.
 */
errstat
iop_dointdisable _ARGS(( void ))
{
    pic_stop();
    return STD_OK;
}

errstat
iop_dointenable _ARGS(( void ))
{
    pic_start();
    return STD_OK;
}

errstat
iop_dokeyclick(volume)
    int volume;
{
    if (volume < 0 || volume > 100)
	return STD_ARGBAD;
#if 0
    if (volume == 0)
	stopclick();
    else
	startclick();
#endif
    return STD_OK;
}

errstat
iop_dosetleds(leds)
    long leds;
{
    void kbd_setleds();

    kbd_setleds((int) leds);
    return STD_OK;
}

errstat
iop_dogetleds(leds)
    long *leds;
{
    *leds = (int) kbd_getleds();
    return STD_OK;
}

/* ARGSUSED */
errstat
iop_dobell(volume, pitch, duration)
    int	volume;
    int	pitch;
    int	duration;
{
    bleep();
    return STD_OK;
}

/*ARGSUSED*/
errstat
iop_dosetcmap(cmapp)
    colour_map * cmapp;
{
    return STD_COMBAD;
}

/*ARGSUSED*/
errstat
iop_dokbdtype(type)
    int * type;
{
    return STD_COMBAD;
}
