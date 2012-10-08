/*	@(#)hscreen.h	1.3	94/04/06 15:41:16 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __HSCREEN_H__
#define __HSCREEN_H__

/*
** hscreen.h - Definitions for low-level X interface to hardware.
*/

/*
** h_fbinfo - structure returned by GETSCREENINFO call.
*/
struct _h_fbinfo {
    long		type;		/* screen specific identification */
    unsigned short	width;		/* width in pixels */
    unsigned short	height;		/* height in pixels */
    unsigned short	depth;		/* bits/pixel */
    unsigned short	stride;		/* scanline length in bits */
    unsigned short	xmm;		/* width in mm */
    unsigned short	ymm;		/* height in mm */
    capability		fb;		/* frame buffer capability */
};

typedef struct _h_fbinfo	h_fbinfo;

struct _h_event {
    unsigned short	type;		/* type of event */
    short		x, y;		/* X/Y coordinate (for some events) */
    unsigned short	keyorbut;	/* Key/button pressed */
    long		time;		/* The time it happened */
};

typedef struct _h_event h_event;

/* Event types */
#define EV_NoEvent		0
#define EV_KeyPressEvent	1
#define EV_KeyReleaseEvent	2
#define EV_ButtonPress		3
#define EV_ButtonRelease	4
#define EV_PointerMotion	5
#define EV_PointerDelta		6

/* Commands you can send to the low-level server: */

#define SC_GETSCREENINFO	1
#define SC_BELL			2
#define SC_AUTOREPEATOFF	3
#define SC_AUTOREPEATON		4
#define SC_WARPCURSOR		5
#define SC_LOADCURSOR		6
#define SC_SETPOINTERSCALE	7
#define SC_GETEVENTS		8
#define SC_KEYCLICKON		9
#define SC_KEYCLICKOFF		10
#define SC_COLORMAP		11
#define SC_COLORCURSOR		12
#define	SC_ENABLE		13
#define	SC_DISABLE		14
#define	SC_INBYTE		15
#define	SC_INWORD		16
#define	SC_OUTBYTE		17
#define	SC_OUTWORD		18

#endif /* __HSCREEN_H__ */
