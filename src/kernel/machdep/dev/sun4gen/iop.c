/*	@(#)iop.c	1.4	96/02/27 13:54:41 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * iop.c
 *
 * Machine (Sun4c & Sun4m) dependent IOP routines, including the
 * interface for keyboard and mouse interrupts.
 *
 * Author:
 *	Greg Sharp, Jan 1993 -	Made necessary changes to Sun3 version to get
 *				it to run on sparcstations.
 *		    Feb 1993 -	Added support for colour displays.
 *		    Aug 1993 -	Added cvt_io_addr call so that the code could
 *				be used for the sun4m as well.
 *		    May 1994 -  Fixed mouse driver to accept the real Mouse
 *				systems 5 byte format and to init its button
 *				data.
 */
#include <amoeba.h>
#include <map.h>
#include <ampolicy.h>
#include <machdep.h>
#include <semaphore.h>
#include <assert.h>
INIT_ASSERT
#include <cmdreg.h>
#include <stdcom.h>
#include <stderr.h>
#include <module/mutex.h>
#include <openprom.h>
#include <iop/iop.h>
#include <iop/colourmap.h>
#include <iop/iopsvr.h>
#include <sys/debug.h>
#include <string.h>
#include <mmuproto.h>

extern void		iop_cvt_io_addr _ARGS((phys_bytes * addr));

/* Used by the mouse driver below - it needs to be initialised! */
static int old_buttons;

/* Graphics card info */
#define	CG3_OFFSET	256*1024
#define	CG3_SIZE	((((900 * 1152) + (2 * 128 * 1024) + 4095)/4096)*4096)
#define	CG3_CMAPOFF	(4*1024*1024)

#define	CG6_OFFSET	((0x16000 + CLICKSIZE - 1) & ~(CLICKSIZE - 1))
#define	CG6_SIZE	((900 * 1152) + CG6_OFFSET)
#define	CG6_CMAPOFF	(2*1024*1024)

#ifndef __STDC__
#define	volatile /**/
#endif

/*
 * The following seems to be the way colour maps are implemented in hardware.
 * It defies description - just look at the code for setcmap below and weep.
 */
typedef struct dev_cmap		dev_cmap;
struct dev_cmap {
    volatile uint32	addr;
    volatile uint32	cmap;
    volatile uint32	ctrl;
    volatile uint32	omap;
};

typedef union core_cmap	core_cmap;
union core_cmap {
    uint8	n_map[256 * 3];	    /* How you expect a normal colour map to look! */
    uint32	h_map[256 * 3 / 4]; /* How the hardware wants you to access it */
};

static struct gdevtab {
    char *	name;
    long	devoffset;	/* The offset from normal bitmap position */
    long	devsize;	/* Amount of address space to map in */
    long	cmapoffset;	/* Where to find the colour map */
} gdevs[] = {
	{ "bwtwo",		  0,	      -1,	         -1 },
	{ "cgthree",	-CG3_OFFSET,	CG3_SIZE,	CG3_CMAPOFF },
	{ "cgsix",	-CG6_OFFSET,	CG6_SIZE,	CG6_CMAPOFF },
	{ "",			  0,	       0,		  0 }
};

#define	SZ_GDEVS	(sizeof gdevs / sizeof (struct gdevtab))

static IOPFrameBufferInfo	fbinfo;
static phys_bytes		fbsize; /* Size of the frame buffer in bytes */
static phys_bytes		bmaddr; /* Virtual address of bitmap */
static dev_cmap *		cmap_ptr; /* Virtual address of bitmap */

extern void kbd_setleds _ARGS(( int ));


/*
 * The display devices on a sun4c must be mapped in very early since
 * they do unpleasant things when opened.  We call iop_devinit() as soon
 * as possible after initmm.
 */

static void
set_bmdev_addr(p)
nodelist *	p;
{
    char	name[MAXDEVNAME];
    int		len;
    int		value;
    dev_reg_t	d;
    static int	ndevs;
    int		devindex;

    (void) obp_genname(p, name, name+MAXDEVNAME);
    if (ndevs == 0)
    {
	DPRINTF(0, ("iop: using display device `%s'\n", name));
    }
    else
    {
	DPRINTF(0, ("iop: found display device `%s' but ignoring it\n", name));
	return;
    }

    if (bmaddr != 0)
    {
	printf("iop: Display device `%s' already mapped in\n", name);
	return;
    }
    /* Get the device address */
    obp_devaddr(p, &d);

    len = obp_getattr(p->l_curnode, "name", (void *) fbinfo.name, IOPNAMELEN);
    if (len <= 0)
    {
	fbinfo.name[0] = '\0';
	printf("iop: can't get name attribute of %s\n", name);
	return;
    }
    fbinfo.name[IOPNAMELEN - 1] = '\0';

    for (devindex = 0; devindex < SZ_GDEVS; devindex++)
    {
	if (strcmp(fbinfo.name, gdevs[devindex].name) == 0)
	    break;
    }
    if (devindex >= SZ_GDEVS)
    {
	printf("iop: no device information for %s\n", fbinfo.name);
	return;
    }

    len = obp_getattr(p->l_curnode, "height", (void *) &value, sizeof value);
    if (len != sizeof value)
    {
	/* Open the device to set height, width and depth attributes */
	obp_devinit(name);
	len = obp_getattr(p->l_curnode, "height", (void *) &value, sizeof value);
	if (len != sizeof value)
	{
	    printf("iop: can't get height attribute of %s\n", name);
	    return;
	}
    }
    fbinfo.height = value;

    len = obp_getattr(p->l_curnode, "width", (void *) &value, sizeof value);
    if (len != sizeof value)
    {
	printf("iop: can't get width attribute of %s\n", name);
	return;
    }
    fbinfo.width = value;
    fbinfo.stride = value;

    len = obp_getattr(p->l_curnode, "depth", (void *) &value, sizeof value);
    if (len != sizeof value)
    {
	printf("iop: can't get depth attribute of %s, assuming 1\n", name);
	value = 1;
    }
    fbinfo.depth = value;

    /* Guess at how big the physical screen must be */
    fbinfo.type = 1; /*??*/
    fbinfo.xmm = 300;
    fbinfo.ymm = 240;
    ndevs++;

    fbsize = gdevs[devindex].devsize;
    if (fbsize == -1)
	fbsize = (fbinfo.width  + 7) / 8 * fbinfo.height * fbinfo.depth;

    /*
     * At this point things get ugly.  We have in fact received the address
     * of the ROM of the display device and not of the bitmap.  Bitter
     * searching reveals that the bitmap is half-way along the address space
     * of the device.  Now if it were only that simple.  Some devices also
     * have a few extra offsets due to overlay planes, etc which appear before
     * the bitmap in the address space. We use the gdevs table to figure it out.
     */
    bmaddr = d.reg_addr + (d.reg_size / 2) + gdevs[devindex].devoffset;
    iop_cvt_io_addr(&bmaddr);

    /* Now to map in the colour map registers */
    if (gdevs[devindex].cmapoffset != -1)
    {
	d.reg_addr += gdevs[devindex].cmapoffset;
	d.reg_size = 16;
	cmap_ptr = (dev_cmap *) mmu_mapdev(&d);
	if (cmap_ptr == 0)
	{
	    printf("iop: couldn't map in the colour map registers for '%s'\n", name);
	    bmaddr = 0;
	}
    }
    /*
     * We can't map in a hardware segment just yet - the rest of the
     * kernel isn't sufficiently initialised.  By the time it is we
     * can't open the display using obp_devinit.  That's why this is done
     * very early and mapping the hardware segment happens later when
     * it is actually needed.
     */
}


void
iop_initdev()
{
    /* Find all the display devices the openprom knows about */
    obp_regnode("device_type", "display", set_bmdev_addr);

    /* The initial value of mouse buttons must be "released" */
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

/*ARGSUSED*/
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
    static int initialised;
    /*
     * When this routine is called the display devices have already been
     * found and the fbinfo struct has been filled in.  Now we can make
     * a segment for the frame-buffer and give it to the user.
     */
    assert(bmaddr != 0);

    /*
     * Get a segment server capability for the memory where the frame buffer
     * is located and store it in the frame buffer info structure.
     */
    if (!initialised) {
	if (HardwareSeg(&fbinfo.fb, bmaddr, fbsize, 0) != STD_OK) {
	    printf("iopsvr: cannot create frame buffer segment\n");
	    return STD_SYSERR;
	}
	initialised = 1;
    }

    *fbptr = &fbinfo;
    return STD_OK;
}

/*ARGSUSED*/
errstat
iop_domapmemory(cap)
    capability * cap;
{
    return STD_COMBAD;
}

/*ARGSUSED*/
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
iop_dointdisable()
{
    return STD_COMBAD;
}

/*
 * Enable device interrupts.
 */
errstat
iop_dointenable()
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

errstat
iop_dosetleds(leds)
    long leds;
{
    kbd_setleds((int) leds);
    return STD_OK;
}

/*ARGSUSED*/
errstat
iop_dogetleds(leds)
    long * leds;
{
    return STD_COMBAD;
}

/*ARGSUSED*/
errstat
iop_dobell(volume, pitch, duration)
    int volume;
    int	pitch;
    int	duration;
{
    bleep();
    return STD_OK;
}

errstat
iop_dokbdtype(type)
    int *	type;
{
    *type = 4;
    return STD_OK;
}

/*ARGSUSED*/
errstat
iop_dosetcmap(cmapp)
colour_map * cmapp;
{
    long	c;
    long	index;
    uint8 *	rmap;
    uint8 *	gmap;
    uint8 *	bmap;

    index = cmapp->c_index;
    if (index < 0 || index > 255)
	return STD_ARGBAD;
    rmap = cmapp->c_red;
    gmap = cmapp->c_green;
    bmap = cmapp->c_blue;
    c = cmapp->c_count;
    /* Limit the count to not exceed the number of map entries */
    if (c + index > 256)
	c = 256 - index;
    if (strcmp(fbinfo.name, "cgsix") == 0)
    {
	/*
	 * Our moment of triumph:  we write the start index an then we pop the colours
	 * in bunches of three, one after another into the cmap register.
	 */
	cmap_ptr->addr = index << 24;
	while (--c >= 0)
	{
	    cmap_ptr->cmap = (uint32) (*rmap++) << 24;
	    cmap_ptr->cmap = (uint32) (*gmap++) << 24;
	    cmap_ptr->cmap = (uint32) (*bmap++) << 24;
	}
    }
    else /* One of the even grosser little colourmaps - hold on tight */
    {
	static core_cmap Core_copy;
	uint8 * cp;
	uint32 * hp;
	int	h_count;
	
	/* The # words we have to write into the hardware is: */
	h_count = ((index + c + 3) / 4 - index / 4) * 3;

	/* Copy everything into the union in core */
	cp = &Core_copy.n_map[index * 3];
	while (--c >= 0)
	{
	    *cp++ = *rmap++;
	    *cp++ = *gmap++;
	    *cp++ = *bmap++;
	}
	/* Now write it to the colour map according to the weird hardware's needs */
	hp = &Core_copy.h_map[index / 4 * 3];
	cmap_ptr->addr = index & ~03;
	while (--h_count >= 0)
	{
	    cmap_ptr->cmap = *hp++;
	}
    }
    return STD_OK;
}


/*
 * Keyboard interrupt; highest bit is the make/break bit,
 * the lower 6 bits represent the actual key code.
 */
void
keyboard_intr(byte)
    long byte;
{
    newevent(byte & 0x80 ? EV_KeyReleaseEvent : EV_KeyPressEvent,
	(int) (byte & 0x7f), -1, -1);
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

    /*
     * Mouse system's mouse protocol
     */
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
