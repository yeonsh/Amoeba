/*	@(#)keyboard.c	1.13	96/02/27 13:51:45 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * keyboard.c
 *
 * Driver for standard 101 key PC/AT keyboard.
 *
 * Author:
 *	Leendert van Doorn
 */
#include <amoeba.h>
#include <machdep.h>
#include <fault.h>
#include <map.h>
#include <sys/proto.h>
#include <iop/iop.h>
#include <tty/term.h>
#include <bool.h>
#include "i386_proto.h"

#include "pit.h"
#include "keyboard.h"

#ifndef KBD_DEBUG
#define	KBD_DEBUG	0
#endif
#ifndef I386_DEBUG
#define	I386_DEBUG	0
#endif

#define	MAXBLEEPS	10		/* maximum # of outstanding "bleeps" */
#define	BLEEPTIME	200		/* time to bleep (in milli seconds)*/
#define	BLEEPFREQ	1331		/* bleep frequency */

#ifndef ESCAPE
#define ESCAPE	'\036'
#endif

#ifndef NO_INPUT_FROM_CONSOLE

#ifndef NDEBUG
static int kbd_debug;		/* current debug level */
static int i386_debug;		/* current debug level */
#endif

#ifdef IOP
extern int iop_enabled;		/* determine whether X is in control */
#endif

/* these describe the state of the keyboard driver */
static int alt;			/* alt key active */
static int control;		/* control key active */
static int shift1, shift2;	/* left, right shift key active */
static int capslock;		/* caps-lock key active */
static int numlock;		/* num-lock key active */
static int scrollock;		/* scroll-lock key active */

/*
 * Key tables: one table for unshifted key values and
 * one for shifted keys. Entries with values >= 0x80
 * are indices into the special key table ``funckeys''.
 */
static uint8 keytab[] = { /* unshifted keys */
    0,    0x1B,  '1',    '2',   '3',   '4',   '5',   '6',
    '7',  '8',   '9',    '0',   '-',   '=',   '\b',  '\t',
    'q',  'w',   'e',    'r',   't',   'y',   'u',   'i',
    'o',  'p',   '[',    ']',   '\r',  0,     'a',   's',
    'd',  'f',   'g',    'h',   'j',   'k',   'l',   ';',
    '\'', '`',   0,      '\\',  'z',   'x',   'c',   'v',
    'b',  'n',   'm',    ',',   '.',   '/',   0,    '*',
    0,    ' ',   0,      0x89,  0x8A,  0x8B,  0x8C,  0x8D,
    0x8E,  0x8F,  0x90,  0x91,  0x92,  0,     0,     0x81,
    0x85,  0x82,  '-',   0x87,  0,     0x88,  '+',   0x83,
    0x86,  0x84,  0x80,  0x7F,  0,     0,     0,     0x93,
    0x94,  0,     0,     0,     0,     0,     0,     0,
    0,     0,     0,     0,     0,     0,     0,     0
};

static uint8 sfttab[] = { /* shifted keys */
    0,    0x1B,  '!',    '@',   '#',   '$',   '%',   '^',
    '&',  '*',   '(',    ')',   '_',   '+',   '\b',  '\t',
    'Q',  'W',   'E',    'R',   'T',   'Y',   'U',   'I',
    'O',  'P',   '{',    '}',   '\r',  0,     'A',   'S',
    'D',  'F',   'G',    'H',   'J',   'K',   'L',   ':',
    '"',  '~',   0,      '|',   'Z',   'X',   'C',   'V',
    'B',  'N',   'M',    '<',   '>',   '?',   0,     '*',
    0,    ' ',   0,      0x95,  0x96,  0x97,  0x98,  0x99,
    0x9A, 0x9B,  0x9C,   0x9D,  0x9E,  0,     0,     '7',
    '8',  '9',   '-',    '4',   '5',   '6',   '+',   '1',
    '2',  '3',   '0',    '.',   0,     0,     0,     0x9F,
    0xA0, 0,     0,      0,     0,     0,     0,     0,
    0,    0,     0,      0,     0,     0,     0,     0
};

/* function keys generate ANSI like escape sequences */
static char *funckeys[] = {
    "\033[@",	/* insert key */
    "\033[H",	/* home key */
    "\033[V",	/* page up */
    "\033[Y",	/* end */
    "\033[U",	/* page down */
    "\033[A",	/* arrow up */
    "\033[B",	/* arrow down */
    "\033[D",	/* arrow left */
    "\033[C",	/* arrow right */
    "\033OP",	/* F1 */
    "\033OQ",	/* F2 */
    "\033OR",	/* F3 */
    "\033OS",	/* F4 */
    "\033OT",	/* F5 */
    "\033OU",	/* F6 */
    "\033OV",	/* F7 */
    "\033OW",	/* F8 */
    "\033OX",	/* F9 */
    "\033OY",	/* F10 */
    "\033OZ",	/* F11 */
    "\033OA",	/* F12 */
    "\033Op",	/* shift F1 */
    "\033Oq",	/* shift F2 */
    "\033Or",	/* shift F3 */
    "\033Os",	/* shift F4 */
    "\033Ot",	/* shift F5 */
    "\033Ou",	/* shift F6 */
    "\033Ov",	/* shift F7 */
    "\033Ow",	/* shift F8 */
    "\033Ox",	/* shift F9 */
    "\033Oy",	/* shift F10 */
    "\033Oz",	/* shift F11 */
    "\033Oa"	/* shift F12 */
};

void readint();
void dump_cpu_ndp_type();
void dump_gdt();
void dump_tss();
void dump_kpd();
void kbd_leds();
void kbd_command();
void debugkeys();
void kbd_reboot();
static void kbdintr();


/*
 * Initialize keyboard
 */
void
kbdinit()
{
#ifndef NDEBUG
   if ((kbd_debug = kernel_option("kbd")) == 0)
	kbd_debug = KBD_DEBUG;
   if ((i386_debug = kernel_option("i386")) == 0)
	i386_debug = I386_DEBUG;
#endif
   setirq(KBD_IRQ, kbdintr);
   pic_enable(KBD_IRQ);
   numlock = 1;		/* num-lock is on */
   capslock = 0;	/* caps-lock is off */
   scrollock = 0;	/* scroll-lock is off */
   kbd_leds();
}

#ifdef IOP
void
kbdhandler(arg)
    long arg;
{
    newevent(arg & 0x80 ? EV_KeyReleaseEvent : EV_KeyPressEvent, (int) arg,
	     -1, -1);
}
#endif

/*
 * Keyboard interrupt
 */
/* ARGSUSED */
static void
kbdintr(irq, fp)
    int irq;
    struct fault *fp;
{
    uint16 keycode, val, code;
    uint16 make, key;			/* break is a dirty word in C */
    char *p;

    /* fetch the key code from the keyboard */
    keycode = in_byte(KBD_DATA);

    /* acknowledge key receipt in mysterious ways */
#if defined(ISA)
    out_byte(KBD_PORTB, (int) ((val = in_byte(KBD_PORTB)) | KBD_KBIT));
    out_byte(KBD_PORTB, (int) val);
#elif defined(MCA)
    out_byte(0x69, (int) ((val = in_byte(0x69)) ^ 0x10));
    out_byte(0x69, (int) val);
    out_byte(0x66, (int) ((val = in_byte(0x66)) & ~0x10));
    out_byte(0x66, (int) (val | 0x10));
    out_byte(0x66, (int) (val & ~0x10));
#endif

    /*
     * The AT keyboard interrupts twice per key, once when
     * depressed and once when released. Except for the shift
     * type keys these latter ones are filtered out. Key values
     * with their 8th bit set denote key releases.
     */
    key = keycode & 0x7F;
    make = keycode & 0x80 ? 1 : 0;

#ifdef IOP
    /*
     * X key event are processed before anything else. Since X wants raw
     * keyboard data it should also deal with raw translations itself.
     */
    if (iop_enabled) {
	enqueue(kbdhandler, (long)keycode);
	return;
    }
#endif

#ifndef NDEBUG
    if (kbd_debug > 1)
	printf("key = %x (make=%d): s1=%d,s2=%d,c=%d,a=%d,cl=%d,nl=%d,sl=%d\n",
	    key,make,shift1,shift2,control,alt,capslock,numlock,scrollock);
#endif

    /*
     * Enable screen on any key that's typed
     */
    crt_enabledisplay();

    /*
     * Shift type keys are sensitive to the make/break sequence.
     * E.g. holding the control key down while pressing another
     * key should result into the usual control-key code.
     * Left/right shift, and the alt keys are only active when
     * pressed, caps-lock, num-lock, and scroll-lock are toggles.
     */
    switch (key) {
    case KBD_LSHIFT:	/* left shift key */
	shift1 = !make;
	return;
    case KBD_RSHIFT:	/* right shift key */
	shift2 = !make;
	return;
    case KBD_CONTROL:	/* control key */
	control = !make;
	return;
    case KBD_ALT:	/* alt key */
	alt = !make;
	return;
    case KBD_CAPS:	/* caps-lock key */
	if (!make) {
	    capslock = 1 - capslock;
	    kbd_leds();
	}
	return;
    case KBD_SCROLL:	/* scroll-lock key */
	if (!make) {
	    scrollock = 1 - scrollock;
	    kbd_leds();
	    return;
	}
	return;
    case KBD_NUM:	/* num-lock or pause key */
	if (!make && control) {
	    /* pause key */
	} else if (!make) {
	    /* num-lock key */
	    numlock = 1 - numlock;
	    kbd_leds();
	}
	return;
    case KBD_PRINT:	/* possibly print screen */
	if (!make && shift1) {
	    enqueue(readint, (long) (ESCAPE | (0 << 16)));
	    return;
	}
	break;
    }

    /*
     * Allow CTRL ALT DELETE sequence to reboot the system.
     * There are no time delays as with reboot.
     */
    if (control && alt && keycode == KBD_DELETE) {
	stop_kernel();
	kbd_reboot();
    }

    /* filter out key releases */
    if (make) return;

    /* convert key code to ASCII representation */
    code = shift1 || shift2 ? sfttab[key] : keytab[key];

    if (control && key < KBD_TOPROW)
	code = sfttab[key];

    /* numeric key pad, including DEL and INS */
    if ((int)key >= KBD_SCODE1 && (int)key <= KBD_SCODE2 + 2)
	code = shift1 || shift2 || !numlock ? keytab[key] : sfttab[key];

    /* entries >= 0x80 are "indices" into the function key table */
    if (code < 0x80) {
	/* ordinary characters */
	if (capslock) {
	    if (code >= 'A' && code <= 'Z')
		code += 'a' - 'A';
	    else if (code >= 'a' && code <= 'z')
		code -= 'a' - 'A';
	}
	if (alt) code |= 0x80;
	/*
	 * Ignore keys that don't produce anything interesting, but still
	 * allow ^@ to produce null bytes
	 */
	if (code == 0)
	    return;
	if (control) code &= 037;
	enqueue(readint, (long) (code | (0 << 16)));
	return;
    } else {
#ifndef NDEBUG
	if (i386_debug > 0 && alt) { /* for now special meaning */
	    debugkeys((int) (code - 0x80), fp);
	    return;
	}
#endif
	for (p = funckeys[code - 0x80]; *p != '\0'; p++)
	    enqueue(readint, (long) (*p | (0 << 16)));
    }
}

#ifdef IOP
/*
 * Set the keyboard leds
 */
void
kbd_setleds(leds)
    int leds;
{
    if (leds & IOP_LED_CAP) capslock = 1; else capslock = 0;
    if (leds & IOP_LED_NUM) numlock = 1; else numlock = 0;
    if (leds & IOP_LED_SCROLL) scrollock = 1; else scrollock = 0;
    kbd_leds();
}

/*
 * Return current settings of keyboard leds
 */
int
kbd_getleds()
{
    int leds = 0;

    if (capslock) leds |= IOP_LED_CAP;
    if (numlock) leds |= IOP_LED_NUM;
    if (scrollock) leds |= IOP_LED_SCROLL;
    return leds;
}
#endif /* IOP */

/*
 * Turns on the keyboard leds,
 * depending on the state the driver is in.
 */
void
kbd_leds()
{
    int leds = 0;

    kbd_command(KBD_SETLEDS);
    if (capslock) leds |= KBD_LEDCAP; else leds &= ~KBD_LEDCAP;
    if (numlock) leds |= KBD_LEDNUM; else leds &= ~KBD_LEDNUM;
    if (scrollock) leds |= KBD_LEDSCROLL; else leds &= ~KBD_LEDSCROLL;
    kbd_command(leds);
}

/*
 * Send command to the keyboard
 */
void
kbd_command(cmd)
    int cmd;
{
    register int r;

    /* wait for keyboard to become ready to accept the command */
    for (r = 0; r < KBD_RETRIES && in_byte(KBD_STATUS) & KBD_BUSY; r++)
	/* nothing */;

    out_byte(KBD_DATA, cmd);

    /* wait for it to acknowledge the command */
    for (r = 0; r < KBD_RETRIES && in_byte(KBD_DATA) != KBD_ACK; r++)
	/* nothing */;
}

#ifndef NDEBUG
/*
 * Show some machine specific debug dump which cannot
 * be shown through the ordinary interface.
 */
void
debugkeys(code, fp)
    int code;
    struct fault *fp;
{
    switch (code) {
    case 9: /* F1 */
	printf("\nKey bindings:\n");
	printf("  <alt> <F1>    What you are looking at\n");
	printf("  <alt> <F2>    CPU and NDP type\n");
	printf("  <alt> <F3>    Frame dump\n");
	printf("  <alt> <F4>    Stack trace\n");
	printf("  <alt> <F5>    Dump global descriptor table\n");
	printf("  <alt> <F6>    Dump tss\n");
	printf("  <alt> <F7>    Dump kernel page tables\n");
        break;
    case 10: /* F2 */
	dump_cpu_ndp_type();
	break;
    case 11: /* F3 */
	framedump(fp, (unsigned long) -1, (unsigned long) -1);
	break;
    case 12: /* F4 */
	stackdump((char *) fp->if_eip, (long *) fp->if_ebp);
	break;
    case 13: /* F5 */
	dump_gdt();
	break;
    case 14: /* F6 */
	disable();
	dump_tss();
	enable();
	break;
    case 15: /* F7 */
	dump_kpd();
	break;
    }
}
#endif /* NDEBUG */
#endif /* NO_INPUT_FROM_CONSOLE */

/*
 * Reset the processor by asserting the reset bit at the keyboard
 * I/O processor (that's why this function is in this file).
 */
void
kbd_reboot()
{
#if defined(ISA) || defined(MCA)
    /*
     * The following magic disables the memory check during reboot.
     * On machines with lots of memory rechecking memory on every
     * reboot becomes a nuisance.
     */
    pic_stop();
    *((short *)0x472) = 0x1234;

    /*
     * Reset the machine by pulling a magic string
     */
    out_byte(KBD_STATUS, KBD_PULSE | (~KBD_RESETBIT & 0x0F));
#endif
}

/*
 * "bleeper" counter
 */
static int bleeper;


/*ARGSUSED*/
static void
stopbleep(arg)
long arg;
{
    if (--bleeper > 0) {
	sweeper_set(stopbleep, 0L, (interval) BLEEPTIME, TRUE);
	return;
    }
    out_byte(KBD_PORTB, in_byte(KBD_PORTB) & ~KBD_SPEAKER);
}

/*
 * Ring my chimes
 */
/*ARGSUSED*/
void
bleep()
{
    if (bleeper == 0) {
	bleeper = 1;
	/* set speaker timer */
	out_byte(PIT_MODE, PIT_MODE3 | ((PIT_CH2-PIT_CH0) << 6));
	out_byte(PIT_CH2, BLEEPFREQ);
	out_byte(PIT_CH2, BLEEPFREQ >> 8);
	/* turn tone on */
	out_byte(KBD_PORTB, in_byte(KBD_PORTB) | KBD_SPEAKER);
	sweeper_set(stopbleep, 0L, (interval) BLEEPTIME, TRUE);
    }
    if (bleeper < MAXBLEEPS) bleeper++;
}
