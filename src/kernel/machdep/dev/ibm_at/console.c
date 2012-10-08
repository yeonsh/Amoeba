/*	@(#)console.c	1.12	94/05/17 11:18:03 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * console.c
 *
 * This file contains the driver for a number of AT-bus video cards.
 * The supported interfaces are: MGA, CGA, EGA, VGA and most of the
 * other IBM-AT compatible video cards. Which video interface is present
 * is dynamically determined at kernel startup.
 * The driver implements a simple console interface with, optionally,
 * a small set of ANSI escape sequences. The driver allows the console
 * to be used as a simple ASCII terminal with no fancy graphics. Whether
 * this would qualify as a workstation is up to the user :-).
 *
 * Author:
 *	Leendert van Doorn
 */
#include <stddef.h>
#include <string.h>
#include <amoeba.h>
#include <machdep.h>
#include <global.h>
#include <fault.h>
#include <map.h>
#include <assert.h>
INIT_ASSERT
#include <tty/term.h>
#include <console.h>
#include <bool.h>
#include <sys/proto.h>
#include "i386_proto.h"

#include "cmos.h"
#include "asy.h"

#ifndef CRT_DEBUG
#define	CRT_DEBUG	0
#endif

#ifndef CRT_IOPORT
#define	CRT_IOPORT	ASY_LINE0
#endif

#ifndef CRT_BAUDRATE
#define CRT_BAUDRATE	9600
#endif

#ifndef MIN
#define	MIN(x,y) ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
#define	MAX(x,y) ((x) > (y) ? (x) : (y))
#endif

/* these describe the current state of the console driver */
static int crt_cmos;		/* CMOS settings */
static int crt_type;		/* controller type */
static int crt_ncols;		/* number of columns */
static int crt_nrows;		/* number of rows */
static int crt_col;		/* current column */
static int crt_row;		/* current row */
static int crt_color;		/* color or mono interface */
static int crt_port;		/* I/O port to address 6845 */
static int crt_modereg;		/* software copy of 6845 mode register */
static vir_bytes crt_base;	/* base of video ram */

/* these describe the state of the screen blank routines */
static uint32 crt_lasttimeevent;/* last time since any key was struck */
static int crt_screenblanked;	/* whether screen is blanked or not */

#ifndef NO_ANSI_ESCAPES
/* these describe the current state of the ANSI state machine */
static int ansi_state;		/* current state in the ANSI FA */
static int *ansi_paramptr;	/* argument pointer */
static int ansi_paramvec[ANSI_NPARAMS]; /* argument vector */
static int ansi_colors[] = { 0, 4, 2, 6, 1, 5, 3, 7}; /* ANSI to PC colors */
#endif /* NO_ANSI_ESCAPES */
static int ansi_blank = 0x07;	/* background attributes (normal) */
static int ansi_attr = 0x07;	/* character attributes (normal) */

#ifndef NDEBUG
static int crt_debug;		/* current debug level */
#endif

#ifdef IOP
extern int iop_enabled;		/* determine whether X is in control */
#endif

void kbdinit();
void crt_textmode();
void crt_enabledisplay();

static void out_char();
static void move_video();
static void blank_video();
static void cursor();
static void next_line();
static void screenblank();
static void set_6845();
static void video();

#ifndef NO_ANSI_ESCAPES
static void ansi_escape();
static void ansi_cs();
static void prev_line();
#endif /* NO_ANSI_ESCAPES */

#ifdef CRT_TRACE
void asy_putchar();
void asy_puts();
void asy_printnum();
#endif /* CRT_TRACE */

/*
 * This routine doesn't really have anything to do with the UART
 * but with the initialization of the screen driver. The kernel
 * already assumes that it can't print to the console until init-
 * uart() is called.
 */
void
inituart()
{
    /*
     * Setup CRT base memory, port, and crt_color flag; depends on whether
     * this is an color of mono card. The case 'EGA' is a bit awkward since
     * we only know that it is not a CGA or monochrome adapter, we assume
     * EGA for backward compatibility, and do our best to find out whether
     * its a VGA adapter.
     */
    crt_cmos = (cmos_read(CMOS_EQB) >> 4) & 0x03;
    switch (crt_cmos) {
    case CMOS_CGA40:
	/* old cga adapter, with 40 columns per row */
	crt_color = TRUE;
	crt_nrows = 25;
	crt_ncols = 40;
	crt_port = CRT_COLOR6845;
	crt_base = PTOV(CRT_COLORBASE);
	crt_type = CRT_TYPE_CGA40;
	break;
    case CMOS_CGA80:
	/* cga adapter, with 80 columns per row */
	crt_color = TRUE;
	crt_nrows = 25;
	crt_ncols = 80;
	crt_port = CRT_COLOR6845;
	crt_base = PTOV(CRT_COLORBASE);
	crt_type = CRT_TYPE_CGA80;
	break;
    case CMOS_MONO80:
	/* monochrome adapter, with 80 columns per row */
	crt_color = FALSE;
	crt_nrows = 25;
	crt_ncols = 80;
	crt_port = CRT_MONO6845;
	crt_base = PTOV(CRT_MONOBASE);
	/* don't know how to identify a hercules controller yet */
	crt_type = CRT_TYPE_MONO80;
	break;
    case CMOS_EGA:
    default:
	/* EGA/VGA/XGA adapter, with 80 columns per row */
	crt_color = TRUE;
	crt_nrows = 25;
	crt_ncols = 80;
	crt_port = CRT_COLOR6845;
	crt_base = PTOV(CRT_COLORBASE);
	/* magic test to determine controller's type */
	out_byte(VGA_SEQREG, 0x05);
	if (in_byte(VGA_SEQREG) != 0x05) {
	    /* alpha text mode, 16 Kb CGA emulation */
	    out_word(VGA_SEQREG, (1 << 8) | VGA_SEQ_MEMMODE);
	    crt_color = TRUE; /* how ??? */
	    crt_type = CRT_TYPE_EGA;
	} else {
	    crt_color = in_byte(VGA_MISCOUTPUT) & 0x01;
	    crt_type = CRT_TYPE_VGA;
	}
	break;
    }

    /*
     * Map in the correct set of pages, reset current row
     * and column and initialize video memory.
     */
    page_mapin((phys_bytes) crt_base, (phys_bytes)(2 * crt_ncols * crt_nrows));
    crt_row = crt_col = 0;
    crt_textmode((uint16 *) NULL);

    /*
     * Setup screen blanking sweeper. In case of a coldstart
     * kernel we will not run turn on the screen blank feature.
     */
    video(1);
    crt_screenblanked = FALSE;
    crt_lasttimeevent = getmilli();
#ifndef COLDSTART
    sweeper_set(screenblank, 0L, (interval) (CRT_BLANKTIME/2), FALSE);
#endif /* COLDSTART */

#if defined(CRT_TRACE) && !defined(ASY)
    /*
     * For debuging purposes its very useful to save the console
     * output in a window with a scroll-bar. BEWARE: since uart
     * I/O is very slow, some kernel tables may fill up. This may
     * cause the kernel to crash.
     */
    /* disable asy interrupts, we poll */
    out_byte(CRT_IOPORT + ASY_IER, 0);

    /* set baud rate */
    out_byte(CRT_IOPORT + ASY_LCR, ASY_LCR_DLAB);
    out_byte(CRT_IOPORT + ASY_DLLSB, ASY_BAUDRATE(CRT_BAUDRATE) & 0xFF);
    out_byte(CRT_IOPORT + ASY_DLMSB, (ASY_BAUDRATE(CRT_BAUDRATE) >> 8) & 0xFF);

    /* turn on 8 bits, 1 stop bit, no parity */
    out_byte(CRT_IOPORT + ASY_LCR, ASY_LCR_8BIT);
    out_byte(CRT_IOPORT + ASY_MCR, ASY_MCR_DTR|ASY_MCR_RTS|ASY_MCR_OUT2);
#endif

#ifndef NDEBUG
    /*
     * Display some statistics
     */
    if ((crt_debug = kernel_option("crt")) == 0)
	crt_debug = CRT_DEBUG;
    if (crt_debug > 0) {
	printf("Video interface: %s\n", crt_color ? "COLOR" : "MONO");
	printf("Video memory base: %x\n", crt_base);
	printf("Video port: %x\n", crt_port);
	printf("Video size: %dx%d\n", crt_nrows, crt_ncols);

	printf("Video cmos info: ");
	switch (crt_cmos) {
	case CMOS_CGA40:
	    printf("cga40");
	    break;
	case CMOS_CGA80:
	    printf("cga80");
	    break;
	case CMOS_MONO80:
	    printf("mono80");
	    break;
	case CMOS_EGA:
	    printf("ega");
	    break;
	default:
	    printf("unknow (%x)", crt_cmos);
	    break;
	}
	printf(" (%x)\n", cmos_read(CMOS_EQB));

	printf("Video controller type: ");
	switch (crt_type) {
	case CRT_TYPE_CGA40:
	    printf("cga40\n");
	    break;
	case CRT_TYPE_CGA80:
	    printf("cga80\n");
	    break;
	case CRT_TYPE_MONO80:
	    printf("mono\n");
	    break;
	case CRT_TYPE_HERCULES:
	    printf("hercules\n");
	    break;
	case CRT_TYPE_EGA:
	    printf("ega\n");
	    break;
	case CRT_TYPE_VGA:
	    printf("vga\n");
	    break;
	default:
	    printf("unknown\n");
	}
    }
#endif

#ifndef NO_INPUT_FROM_CONSOLE
    /*
     * Initialize keyboard
     */
    kbdinit();
#endif
}

/*
 * Put a character onto the console.
 */
int
putchar(iop, ch)
    struct io *iop;
    char ch;
{
#if defined(CRT_TRACE) && !defined(ASY)
    asy_putchar(ch);
#endif

    /*
     * Apparently the video driver is not yet initialized
     */
    if (!crt_base)	
	return 0;

#ifdef IOP
    /*
     * When iop_enabled is enabled it means that X is controlling the console.
     * Since we don't want to interfere with X mysterious ways ...
     */
    if (iop_enabled) return 0;
#endif

    /*
     * Enable screen on any screen activity
     */
    crt_enabledisplay();

#ifndef NO_ANSI_ESCAPES
    /*
     * Handle ANSI escape sequences.
     */
    if (ansi_state)
	ansi_escape(ch);
    else
#endif /* NO_ANSI_ESCAPES */
    /*
     * Interpret the characters
     */
    switch (ch) {
    case 000:	/* null is often used as padding character */
	break;
    case 007:	/* ring my chimes */
	bleep();
	break;
    case '\b':	/* backspace */
	cursor(crt_col - 1, crt_row);
	break;
    case '\n':	/* newline */
	next_line(crt_col);
	break;
    case '\r':	/* cariage return */
	cursor(0, crt_row);
	break;
    case '\t':	/* tab */
	do {
	    if (crt_col <= crt_ncols-1)
		putchar(iop, ' ');
	} while (crt_col & CRT_TABMASK);
	break;
#ifndef NO_ANSI_ESCAPES
    case 033:	/* escape - ansi escape sequence introducer */
	ansi_state = 033;
	break;
#endif /* NO_ANSI_ESCAPES */
    default:
	crt_col++;
#if !CRT_LINEWRAP
	if (crt_col >= crt_ncols) break;
#else
	if (crt_col > crt_ncols)
	    next_line(1);
#endif
	/* display character and advance cursor */
	out_char((char *) (crt_base + (vir_bytes)(crt_row * crt_ncols * 2) +
	    		   (vir_bytes)((crt_col - 1) * 2)), ch, ansi_attr);
	cursor(crt_col, crt_row);
	break;
    }
    return 0;
}


#ifndef NOTTY
/*ARGSUSED*/
int
uart_stat(begin, end, iop)
    char *begin;	/* start of buffer to dump status in */
    char *end;		/* end of buffer */
    struct io *iop;	/* tty to dump status for */
{
    return bprintf(begin, end, "no status available\n") - begin;
}

/*ARGSUSED*/
uart_init(cdp, first)
    struct cdevsw *cdp;
    struct io *first;
{
    assert(first == &tty[0]);
    cdp->cd_write = putchar;
#if !VERY_SIMPLE && !NO_IOCTL
    cdp->cd_setDTR = 0;
    cdp->cd_setBRK = 0;
    cdp->cd_baud = 0;
#endif /* !VERY_SIMPLE && !NO_IOCTL */
#if !VERY_SIMPLE || RTSCTS
    cdp->cd_isusp = 0;
    cdp->cd_iresume = 0;
#endif /* !VERY_SIMPLE || RTSCTS */
    cdp->cd_status = uart_stat;
    return 1;
}
#endif


#if defined(CRT_TRACE) && !defined(ASY)
void
asy_putchar(ch)
    int ch;
{
    register int count;

    for (count = 0; count < 1000; count++) {
	if ((in_byte(CRT_IOPORT + ASY_LSR) & ASY_LSR_THRE) != 0) {
	    break;
	}
    }
    out_byte(CRT_IOPORT + ASY_TXB, ch & 0xff);
}

void
asy_puts(s)
    register char *s;
{
    for (; *s; s++) {
	if (*s == '\n')
	    asy_putchar('\r');
	asy_putchar(*s);
    }
}

void
asy_printnum(num, base)
    uint32 num, base;
{
    uint32 m = num % base;
    uint32 n = num / base;

    if (n > 0) asy_printnum(n, base);
    asy_putchar((base <= 10 || m < 10) ? m + '0' : m - 10 + 'A');
}
#endif /* defined(CRT_TRACE) && !defined(ASY) */

#ifndef NO_ANSI_ESCAPES
/*
 * Parse ANSI escape sequences
 */
static void
ansi_escape(ch)
    int ch;
{
    int i;

    switch (ansi_state) {
    case 033:	/* handle sequences that start with ESC */
	switch (ch) {
	case '[':	/* control sequence introducer */
	    for ( i = 0; i < ANSI_NPARAMS; i++)
		ansi_paramvec[i] = 0;
	    ansi_paramptr = ansi_paramvec;
	    ansi_state = '[';
	    break;
	case 'D':	/* IND - index */
	    next_line(crt_col);
	    ansi_state = 0;
	    break;
	case 'E':	/* NEL - next line */
	    next_line(0);
	    ansi_state = 0;
	    break;
	case 'M':	/* RI - reverse index */
	    prev_line(crt_col);
	    ansi_state = 0;
	    break;
	default:
	    ansi_state = 0;
	    break;
	}
	break;
    case '[':	/* handle sequences that start with a CSI */
	if (ch >= '0' && ch <= '9') {
	    if (ansi_paramptr < ansi_paramvec + ANSI_NPARAMS)
		*ansi_paramptr = *ansi_paramptr * 10 + (ch - '0');
	    break;
	}
	if (ch == ';') {
	    ansi_paramptr++;
	    break;
	}
	ansi_cs(ch);
    default:
	ansi_state = 0;
	break;
    }
}

/*
 * Interpret ANSI control sequences
 */
static void
ansi_cs(ch)
    int ch;
{
    int n;

    switch (ch) {
    case 'A':	/* CUU -  move cursor n lines up */
	n = ansi_paramvec[0] == 0 ? 1 : ansi_paramvec[0];
	cursor(crt_col, MAX(0, crt_row - n));
	break;
    case 'B':	/* CUD - move n lines down */
	n = ansi_paramvec[0] == 0 ? 1 : ansi_paramvec[0];
	cursor(crt_col, MIN(crt_nrows-1, crt_row + n));
	break;
    case 'C':	/* CUF - move n spaces right */
	n = ansi_paramvec[0] == 0 ? 1 : ansi_paramvec[0];
	cursor(MIN(crt_ncols-1, crt_col + n), crt_row);
	break;
    case 'D':	/* CUB - move n spaces left */
	n = ansi_paramvec[0] == 0 ? 1 : ansi_paramvec[0];
	cursor(MAX(0, crt_col - n), crt_row);
	break;
    case 'H':	/* CUP - move cursor to (m,n) */
	cursor(MAX(1, MIN(crt_ncols, ansi_paramvec[1])) - 1,
	    MAX(1, MIN(crt_nrows, ansi_paramvec[0])) - 1);
	break;
    case 'E':	/* CNL - advance cursor n lines */
	n = ansi_paramvec[0] == 0 ? 1 : ansi_paramvec[0];
	cursor(crt_col, MIN(crt_row + n, crt_nrows));
	break;
    case 'F':	/* CPL - back up cursor n lines */
	n = ansi_paramvec[0] == 0 ? 1 : ansi_paramvec[0];
	cursor(crt_col, MAX(crt_row - n, 0));
	break;
    case 'J':	/* ED - clear screen from cursor */
	n = ansi_paramvec[0] == 0 ? 0 : ansi_paramvec[0];
	if (n == 0) {
	    /* erase from active position to end of display */
	    blank_video(crt_base + 2*(crt_row*crt_ncols+crt_col), ansi_blank,
		(crt_nrows - crt_row - 1) * crt_ncols + (crt_ncols - crt_col));
	} else if (n == 1) {
	    /* erase from start of display to active position */
	    blank_video(crt_base,ansi_blank, crt_row * crt_ncols + crt_col + 1);
	} else if (n == 2) {
	    /* erase all of display */
	    blank_video(crt_base, ansi_blank, crt_nrows*crt_ncols);
	}
	break;
    case 'K':	/* EL - clear line from cursor */
	n = ansi_paramvec[0];
	if (n == 0) {
	    /* erase from active position to end of line inclusive */
	    blank_video(crt_base + 2 * (crt_row * crt_ncols + crt_col),
		ansi_blank, crt_ncols - crt_col);
	} else if (n == 1) {
	    /* erase from start of line to active position inclusive */
	    blank_video(crt_base + 2 * (crt_row * crt_ncols),
		ansi_blank, crt_col + 1);
	} else if (n == 2) {
	    /* erase all of line */
	    blank_video(crt_base + 2 * (crt_row * crt_ncols),
		ansi_blank, crt_ncols);
	}
	break;
    case 'X':	/* ECH - erase n characters (cursor doesn't move) */
	n = ansi_paramvec[0];
	blank_video(crt_base + 2 * (crt_row * crt_ncols + crt_col),
		ansi_blank, MIN(n, crt_ncols - crt_col));
	break;
    case 'L':	/* IL - insert n lines at cursor */
	n = ansi_paramvec[0] == 0 ? 1 : ansi_paramvec[0];
	if (n > (crt_nrows - crt_row))
	    n = crt_nrows - crt_row;
	move_video(crt_base + 2 * crt_row * crt_ncols,
	    crt_base + 2 * (crt_row + n) * crt_ncols,
	    (crt_nrows - crt_row - n) * crt_ncols);
	blank_video(crt_base + 2 * crt_row * crt_ncols,
	    ansi_blank, n * crt_ncols);
	break;
    case '@':	/* ICH - insert n characters at cursor */
	n = ansi_paramvec[0] == 0 ? 1 : ansi_paramvec[0];
	if (n > (crt_ncols - crt_col))
	    n = crt_ncols - crt_col;
	move_video(crt_base + 2 * ((crt_row * crt_ncols) + crt_col),
	    crt_base + 2 * ((crt_row * crt_ncols) + crt_col + n),
	    crt_ncols - crt_col - n);
	blank_video(crt_base + 2 * ((crt_row * crt_ncols) + crt_col),
	    ansi_blank, n);
	break;
    case 'M':	/* DL - delete n lines at cursor */
	n = ansi_paramvec[0] == 0 ? 1 : ansi_paramvec[0];
	if (n > (crt_nrows - crt_row))
	    n = crt_nrows - crt_row;
	move_video(crt_base + 2 * (crt_row + n) * crt_ncols,
	    crt_base + 2 * crt_row * crt_ncols,
	    (crt_nrows - crt_row - n) * crt_ncols);
	blank_video(crt_base + 2 * (crt_nrows - n) * crt_ncols,
	    ansi_blank, n * crt_ncols);
	break;
    case 'P':	/* DCH - delete n characters at cursor */
	n = ansi_paramvec[0] == 0 ? 1 : ansi_paramvec[0];
	if (n > (crt_ncols - crt_col))
	    n = crt_ncols - crt_col;
	move_video(crt_base + 2 * (crt_row * crt_ncols + crt_col + n),
	    crt_base + 2 * (crt_row * crt_ncols + crt_col),
	    crt_ncols - crt_col - n);
	blank_video(crt_base + 2 * (crt_row * crt_ncols + crt_ncols - n),
	    ansi_blank, n);
	break;
    case 'T':	/* SD - scroll n lines down */
	n = ansi_paramvec[0] == 0 ? 1 : ansi_paramvec[0];
	if (n > (crt_nrows - crt_row))
	    n = crt_nrows - crt_row;
	move_video(crt_base, crt_base + 2 * n * crt_ncols,
	    (crt_nrows - n) * crt_ncols);
	blank_video(crt_base, ansi_blank, n * crt_ncols);
	break;
    case 'S':	/* SU - scroll n lines up */
	n = ansi_paramvec[0] == 0 ? 1 : ansi_paramvec[0];
	if (n > (crt_nrows - crt_row))
	    n = crt_nrows - crt_row;
	move_video(crt_base + 2 * n * crt_ncols, crt_base,
	    (crt_nrows - n) * crt_ncols);
	blank_video(crt_base + 2 * (crt_nrows - n) * crt_ncols,
	    ansi_blank, n * crt_ncols);
	break;
    case 'm':	/* SGR - enables rendition n */
	n = ansi_paramvec[0] == 0 ? 0 : ansi_paramvec[0];
	switch (n) {
	case 0:	/* primary rendition */
	    ansi_attr = ansi_blank;
	    break;
	case 1:	/* bold (red fg) */
	    ansi_attr = crt_color ?
		(ansi_attr & 0xF0) | 0x04 : ansi_attr | 0x08;
	    break;
	case 4:	/* underline (blue fg) */
	    ansi_attr = crt_color ?
		(ansi_attr & 0xF0) | 0x01 : ansi_attr & 0x89;
	    break;
	case 5:	/* slow blink */
	case 6:	/* rapid blink (magenta fg) */
	    ansi_attr = crt_color ?
		(ansi_attr & 0xF0) | 0x05 : ansi_attr | 0x80;
	    break;
	case 7:	/* negative (black on light grey) */
	    if (crt_color) {
		ansi_attr = ((ansi_attr&0xf0) >> 4) | ((ansi_attr&0x0f) << 4);
	    } else {
		ansi_attr = ansi_attr & 0x70 ?
		    (ansi_attr & 0x88) | 0x07 : (ansi_attr & 0x88) | 0x70;
	    } 
	    break;
	default:
	    if (n >= 30 && n <= 37) {
		/* foreground color */
		ansi_attr = (ansi_attr & 0xf0) | ansi_colors[n - 30];
		ansi_blank = (ansi_blank & 0xf0) | ansi_colors[n - 30];
	    } else if (n >= 40 && n <= 47) {
		/* background color */
		ansi_attr = (ansi_attr & 0x0f) | (ansi_colors[n - 40] << 4);
		ansi_blank = (ansi_blank & 0x0f) | (ansi_colors[n - 40] << 4);
	    }
	}
	break;
    }
}

/*
 * Previous line, scroll down if necessary
 */
static void
prev_line(col)
    int col;
{
    if (--crt_row < 0) {
	move_video(crt_base, crt_base + 2 * crt_ncols,
	    (crt_nrows - 1) * crt_ncols);
	blank_video(crt_base, ansi_blank, crt_ncols);
	crt_row++;
    }
    cursor(col, crt_row);
}
#endif /* NO_ANSI_ESCAPES */

/*
 * Next line, scroll up if necessary
 */
static void
next_line(col)
    int col;
{
    if (++crt_row >= crt_nrows) {
	move_video(crt_base + (2 * crt_ncols), crt_base,
	    (crt_nrows - 1) * crt_ncols);
	blank_video(crt_base + 2 * ((crt_nrows - 1) * crt_ncols),
	    ansi_blank, crt_ncols);
	crt_row--;
    }
    cursor(col, crt_row);
}

/*
 * Put a character in video RAM
 */
static void
out_char(src, ch, attr)
    register char *src;
    int ch, attr;
{
    *src = ch;
    *(src + 1) = attr;
}

/*
 * Switch over to text mode, possibly reseting the
 * controller and possibly displaying an text image.
 */
void
crt_textmode(screen)
    uint16 *screen;
{
    /* reset adapter to something sensible */
    switch (crt_type) {
    case CRT_TYPE_CGA40:
	crt_modereg = CRT_MODE_BLINK | CRT_MODE_ALPHA40 | CRT_MODE_ENABLE;
	out_byte(crt_port + CRT_MODE, crt_modereg);
	break;
    case CRT_TYPE_CGA80:
    case CRT_TYPE_MONO80:
    case CRT_TYPE_HERCULES:
	crt_modereg = CRT_MODE_BLINK | CRT_MODE_ALPHA80 | CRT_MODE_ENABLE;
	out_byte(crt_port + CRT_MODE, crt_modereg);
	break;
    case CRT_TYPE_EGA:
	/* don't know how to do it for EGA */
	break;
    case CRT_TYPE_VGA:
	/* and neither for VGA yet */
	break;
    }

    /* restore screen, if necessary */
    if (screen != NULL) {
	move_video((vir_bytes) screen, crt_base, crt_nrows * crt_ncols);
	relblk((vir_bytes) screen);
    } else
	blank_video(crt_base, ansi_blank, crt_ncols * crt_nrows);

    /* reset 6845 registers */
    set_6845(CRT_STARTLOW, 0);
    set_6845(CRT_STARTHIGH, 0);
    set_6845(CRT_STATUS, 15); /* block cursor */
    cursor(crt_col, crt_row);
    crt_enabledisplay();
}

#ifdef IOP
/*
 * Switch to graphics mode (sort of :-),
 * and save current screen image.
 */
uint16 *
crt_graphmode()
{
    vir_bytes screen;

    screen = getblk((vir_bytes)(crt_nrows*crt_ncols*sizeof(uint16)));
    if (screen == 0) return NULL;
    move_video(crt_base, screen, crt_nrows * crt_ncols);
    return (uint16 *) screen;
}
#endif /* IOP */

/*
 * Fast video memory move.
 * This routine is centralized here, since we might
 * want to do something special with very obsolete
 * video adapters (someday).
 */
static void
move_video(src, dst, wcount)
    vir_bytes src, dst;
    int wcount;
{
    (void) memmove((_VOIDSTAR) dst, (_VOIDSTAR) src, (size_t) (wcount * 2));
}

/*
 * Blank video memory using a special blank color
 */
static void
blank_video(src, blankcolor, wcount)
    vir_bytes src;
    int blankcolor, wcount;
{
    register char *p;

    for (p = (char *) src; wcount-- > 0; ) {
	*p++ = 0;
	*p++ = blankcolor;
    }
}

/*
 * Move cursor to position (x,y)
 */
static void
cursor(col, row)
    int col, row;
{
    /* some sanity checks */
    if (col < 0 || col >= crt_ncols || row < 0 || row > crt_nrows)
	return;
    crt_col = col;
    crt_row = row;
    set_6845(CRT_CURSOR, ((crt_row * crt_ncols * 2) + (crt_col * 2)) >> 1);
}

/*
 * When necessary, enable display 
 */
void
crt_enabledisplay()
{
#ifdef IOP
    if (iop_enabled) return;
#endif
    crt_lasttimeevent = getmilli();
    if (crt_screenblanked) {
	crt_screenblanked = FALSE;
	video(1);
    }
}

/*
 * Sweeper routine to determine whether the screen has to be blanked.
 * This routine runs every BLANKTIME/2 milliseconds, so the screen
 * blank interval is about BLANKTIME to BLANKTIME+BLANKTIME/2 milli
 * seconds of no event activity.
 */
static void
screenblank()
{
    uint32 currentevent = getmilli();

#ifdef IOP
    if (iop_enabled) return;
#endif
    if (crt_screenblanked) return;
    /* just in case the clock wraps */
    if (currentevent < crt_lasttimeevent)
	crt_lasttimeevent = currentevent;
    if ((currentevent - crt_lasttimeevent) >= CRT_BLANKTIME) {
	crt_screenblanked = TRUE;
	video(0);
    }
}

/*
 * Turn video adapter on or off by toggling the enable bit in the
 * 6845 mode register for ordinary cards, and toggling the screen
 * off bit for VGA adapters.
 */
static void
video(on)
    int on;
{
    unsigned char clockmode;

    switch (crt_type) {
    case CRT_TYPE_CGA40:
    case CRT_TYPE_CGA80:
    case CRT_TYPE_MONO80:
	/* ordinary way on a 6845 chip */
	if (on) {
	    crt_modereg |= CRT_MODE_ENABLE;
	    out_byte(crt_port + CRT_MODE, crt_modereg);
	} else if (crt_modereg & CRT_MODE_ENABLE) {
	    register int count;

	    /* wait for vertical retrace */
	    for (count = 0; count < 1000000; count++) {
		if ((in_byte(crt_port + CRT_STATUS) & CRT_STATUS_VSYNC) != 0) {
		    break;
		}
	    }
	    crt_modereg &= ~CRT_MODE_ENABLE;
	    out_byte(crt_port + CRT_MODE, crt_modereg);
	}
	break;
    case CRT_TYPE_HERCULES:
    case CRT_TYPE_EGA:
	/* don't know how to do it for hercules and EGA */
	break;
    case CRT_TYPE_VGA:
	/* toggle the screen off bit in the clocking mode register */
	out_byte(VGA_SEQREG, VGA_SEQ_CLOCKINGMODE);
	clockmode = in_byte(VGA_SEQREG+1);
	clockmode = on ? clockmode & 0xDF : clockmode | 0x20;
	out_word(VGA_SEQREG, 0x0100);
	out_word(VGA_SEQREG, (int) ((clockmode << 8) | 0x01));
	out_word(VGA_SEQREG, 0x0300);
	break;
    }
}

/*
 * Set a register pair inside the 6845
 */
static void
set_6845(reg, val)
    int reg, val;
{
    register int flags;

    /*
     * Registers 10-11 control the format of the cursor.
     * Registers 12-13 tell the 6845 where to start in video RAM.
     * Registers 14-15 tell the 6845 where to put the cursor.
     * Note that registers 12-15 work in words.
     */
    flags = get_flags(); disable();
    out_byte(crt_port + CRT_INDEX, reg);
    out_byte(crt_port + CRT_DATA, val >> 8);
    out_byte(crt_port + CRT_INDEX, reg + 1);
    out_byte(crt_port + CRT_DATA, val & 0377);
    set_flags(flags);
}
