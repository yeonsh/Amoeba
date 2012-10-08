/*	@(#)iop.h	1.9	96/02/27 10:36:28 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __IOP_H__
#define __IOP_H__
/*
 * iop.h - Definitions for low-level X interface to hardware
 */

/*
 *  Commands you can send to the low-level server
 */
#define	IOP_ENABLE		(IOP_FIRST_COM+1)
#define	IOP_DISABLE		(IOP_FIRST_COM+2)
#define IOP_FRAMEBUFFERINFO	(IOP_FIRST_COM+3)
#define	IOP_MAPMEMORY		(IOP_FIRST_COM+4)
#define	IOP_UNMAPMEMORY		(IOP_FIRST_COM+5)
#define IOP_GETEVENTS		(IOP_FIRST_COM+8)
#define IOP_AUTOREPEATOFF	(IOP_FIRST_COM+9)
#define IOP_AUTOREPEATON	(IOP_FIRST_COM+10)
#define IOP_KEYCLICK		(IOP_FIRST_COM+11)
#define	IOP_SETLEDS		(IOP_FIRST_COM+12)
#define	IOP_GETLEDS		(IOP_FIRST_COM+13)
#define IOP_BELL		(IOP_FIRST_COM+14)
#define	IOP_MOUSECONTROL	(IOP_FIRST_COM+15)
#define	IOP_INTDISABLE		(IOP_FIRST_COM+16)
#define	IOP_INTENABLE		(IOP_FIRST_COM+17)
#define	IOP_KBDTYPE		(IOP_FIRST_COM+18)
#define	IOP_SETCMAP		(IOP_FIRST_COM+19)

#define	IOPNAMELEN		10

/*
 * Frame Buffer structure returned by IOP_FRAMEBUFFERINFO call
 */
typedef struct _IOPFrameBufferInfo {
    long		type;		/* screen specific identification */
    unsigned short	width;		/* width in pixels */
    unsigned short	height;		/* height in pixels */
    unsigned short	depth;		/* bits/pixel */
    unsigned short	stride;		/* scanline length in bits */
    unsigned short	xmm;		/* width in mm */
    unsigned short	ymm;		/* height in mm */
    capability		fb;		/* frame buffer capability */
    char		name[IOPNAMELEN]; /* name of framebuffer device */
} IOPFrameBufferInfo;

/*
 * IOP event structure as returned by IOP_GETEVENTS
 */
typedef struct _IOPEvent {
    unsigned short	type;		/* type of event */
    short		x, y;		/* X/Y coordinate (for some events) */
    unsigned short	keyorbut;	/* Key/button pressed */
    long		time;		/* The time it happened */
} IOPEvent;

/*
 * Event types
 */
#define EV_NoEvent		0
#define EV_KeyPressEvent	1
#define EV_KeyReleaseEvent	2
#define EV_ButtonPress		3
#define EV_ButtonRelease	4
#define EV_PointerMotion	5
#define EV_PointerDelta		6

/*
 * Keyboard leds
 */
#define	IOP_LED_SCROLL		01	/* scroll lock led */
#define	IOP_LED_NUM		02	/* num lock led */
#define	IOP_LED_CAP		04	/* caps lock led */
#define	IOP_LED_CMPS		010	/* compose led */

/*
 * Mouse parameters
 */

/* A 2-button mouse fakes 3rd button with either emulation or a chord */
#define	IOP_CHORDMIDDLE		0x8000

typedef struct _IOPmouseparams {
    int mp_type;			/* mouse type */
    int mp_baudrate;			/* mouse's baud rate */
    int mp_samplerate;			/* mouse's sample rate */
    int	mp_chordmiddle;			/* chording is dis/enabled */
} IOPmouseparams;

/*
 * Supported mouse types
 */
#define	IOP_MOUSE_MM		0	/* Mouse Systems mouse */
#define	IOP_MOUSE_MS		1	/* Microsoft mouse */
#define	IOP_MOUSE_LB		2	/* Logitec bus mouse */
#define	IOP_MOUSE_MMS		3	/* MMseries */
#define	IOP_MOUSE_LOGI		4	/* Logitec serial mouse */
#define	IOP_MOUSE_LOGIMAN	5	/* MouseMan / TrackMan */
#define	IOP_MOUSE_PS2		6	/* PS/2 mouse */
#define	IOP_MOUSE_HITAB		7	/* Hitachi Tablet */

/* Prototypes for client functions: */

errstat iop_enable _ARGS(( capability * ));
errstat	iop_disable _ARGS(( capability * ));
errstat iop_mousecontrol _ARGS(( capability *, int type, int baud,
					int rate, int chordmiddle ));
errstat iop_framebufferinfo _ARGS(( capability *, IOPFrameBufferInfo * ));
errstat iop_map_mem _ARGS(( capability *, capability * videoseg ));
errstat iop_unmap_mem _ARGS(( capability *, capability * videoseg ));
errstat iop_intr_disable _ARGS(( capability * ));
errstat iop_intr_enable _ARGS(( capability * ));
errstat iop_keyclick _ARGS(( capability *, int volume ));
errstat iop_setleds _ARGS(( capability *, int leds ));
errstat iop_getleds _ARGS(( capability *, int * leds ));
errstat iop_ringbell _ARGS(( capability *, int vol, int pitch, int duration ));
errstat iop_getevents _ARGS(( capability *, IOPEvent *, int * ));

#endif /* __IOP_H__ */
