/*	@(#)sty.c	1.5	96/03/01 16:56:03 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 *	Set/display posix tty settings.
 *
 *	This implementation knows that the indices VTIME
 *	VMIN, VEOF and VEOL have unique values, which is
 *	strictly not specified by Posix 1003 (see 7.1.2.6).
 *	As a result, it is reasonable to set canonical modes
 *	and noncanonical ones in one run.
 *	This is questionable; some even say it's wrong.
 *	I say it is the fastest way to do it.
 *
 *	It uses the tios_[sg]etattr stubs directly, not
 *	Posix' tc[sg]etattr. This makes it possible to set
 *	the tty in two transactions when option 'tty' is
 *	used: I don't have to open() the file - which is
 *	expensive when run under Ajax: it would check that
 *	it is not a directory, which is obviously wrong.
 *	If there are any directories around which support
 *	tios_[sg]etattr, I wouldn't want to stop them :-)
 *
 *	This file contains (in this order):
 *		- Tables for flags and chars
 *		- Printing routines
 *		- Functions to get the ttycapability and descriptor
 *		- Functions for option processing
 *		- main()
 *
 *	Author: Siebren van der Zee, CWI Amsterdam
 *	Changed Jul 9 '90, Siebren:
 *		bugfix in printing baudrate and character size.
 *	Changed Oct 25 '90, Siebren:
 *		add -rows and -cols printing.
 *		This is #ifdeffed on TIOS_GETWSIZE, which
 *		is the command code defined class/tios.h.
 */

	/* Amoeba includes:	*/
#include <amoeba.h>
#include <cmdreg.h>
#include <stderr.h>
#include <posix/termios.h>
#include <class/tios.h>
#include <module/name.h>

	/* Standard C includes	*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

/* Compile time consistency check - you never know where this ends up. */
#if VTIME==VEOF || VTIME==VEOL || VMIN==VEOF || VMIN==VEOL
#include "cannot_work_on_this_posix_implementation"
#endif


#define CTRL(ch)	((ch) & 0x1f)	/* Make a control char	*/

/*
 *	Right margin; used to limit the length of output lines.
 */
#define R_MARGIN	(80 - 5)
#define Yes	1
#define No	0

/*
 *	Global state:
 */
capability *Ttycap = NULL, cap;
char *Ttyname = NULL;		/* Name for name_lookup		*/
struct termios Tios;
char *Progname;
int Everything = No;		/* Print all info		*/
int Changed = No, Gottios = No;	/* Changed/got the tty settings	*/
char **Argv;			/* Processed argument vector	*/
#ifdef TIOS_GETWSIZE
int Size = No;			/* Print tty-size		*/
errstat GetwsErr;		/* Valid if Gottios == Yes	*/
int Rows, Cols;			/* Size of the tty if no error	*/
int ChangedSize = No;
#endif /* TIOS_GETWSIZE */

/*
 *	A posix tty roughly contains two types of information:
 *	tty chars, which can be off (i.e. _POSIX_VDISABLE), or
 *	have a character value, and flags containing one bit of
 *	information, which can only be off or on.
 *
 *	This program is highly table driven. For each bit and
 *	char it knows a default value. The default defines
 *	non-canonical reads as blocking reads of 1 character,
 *	and defines ttychars for canonical reads that behave reasonable
 *	for Amoeba (i.e, no TOSTOP etc.), and turns on canonical
 *	reads.
 */

/*
 *	The flags table. Split in four parts: local flags,
 *	input flags, output flags and control flags.
 */
struct flags {			/* Knowledge on flags:		*/
	int f_bit;		/* Which bit it is		*/
	int f_usual;		/* Whether it is usually on	*/
	char *f_name;		/* Name of the bit		*/
	char *f_xplain;		/* Short explanation		*/
} i_flags[] = {				/* Input modes			*/
	BRKINT,	BRKINT,	"brkint",	"Signal interrupt on break",
	ICRNL,	ICRNL,	"icrnl",	"Map CR to NL on input",
	IGNBRK,	0,	"ignbrk",	"Ignore break condition",
	IGNCR,	0,	"igncr",	"Ignore CR (don't transmit)",
	IGNPAR,	0,	"ignpar",	"Ignore characters with parity errors",
	INLCR,	0,	"inlcr",	"Map NL to CR on input",
	INPCK,	0,	"inpck",	"Enable input parity check",
	ISTRIP,	0,	"istrip",	"Strip high bit of input",
	IXOFF,	IXOFF,	"ixoff",	"Enable start/stop input control",
	IXON,	IXON,	"ixon",		"Enable start/stop output control",
	PARMRK,	0,	"parmrk",	"Mark parity errors with esc-0"
}, o_flags[] = {			/* Output flag			*/
	OPOST,	OPOST,	"opost",	"Perform output processing",
	XTAB,	0,	"xtab",		"Expand tabs on output"
}, c_flags[] = {			/* Control flags		*/
	/* Note that CSIZE and the baudrates are missing here	*/
	CLOCAL,	0,	"clocal",	"Ignore modem status lines",
	CREAD,	CREAD,	"cread",	"Enable receiver",
	CSTOPB,	0,	"cstopb",	"Send two stop bits, otherwise one",
	HUPCL,	0,	"hupcl",	"Hangup on last close",
	PARENB,	0,	"parenb",	"Parity enable",
	PARODD,	0,	"parenodd",	"Odd parity, otherwise even",
}, l_flags[] = {			/* Local flags			*/
	ECHO,	ECHO,	"echo",		"Enable echo",
	ECHOE,	ECHOE,	"echoe",	"Echo error-correcting backspace",
	ECHOK,	ECHOK,	"echok",	"Echo kill character",
	ECHONL,	0,	"echol",	"Echo '\\n'",
	ICANON,	ICANON,	"icanon",	"Canonical input editing",
	IEXTEN,	0,	"iexten",	"Enable local extentions",
	ISIG,	ISIG,	"isig",		"Enable signals",
	NOFLSH,	0,	"noflsh",	"Disable flush input on signals",
	TOSTOP,	0,	"tostop",	"Useless on Amoeba"
};

/*
 *	These are not _all_ chars around: VTIME and VMIN are missing,
 *	because they are not really characters but small integers.
 */
struct cc {			/* Knowledge on tty chars	*/
    char *c_name;		/* Name of the character	*/
    int c_index;		/* Index in c_cc array		*/
    cc_t c_usual;		/* Usual value, must be < 0x1f	*/
    char *c_xplain;		/* For the user			*/
} c_chars[] = {
    "eof",	VEOF,	CTRL('d'),		"signal end of file",
    "eol",	VEOL,	_POSIX_VDISABLE,	"secondary end of line",
    "erase",	VERASE,	CTRL('h'),		"erase previous char",
    "intr",	VINTR,	CTRL('c'),		"generate interrupt",
    "kill",	VKILL,	CTRL('x'),		"kill line",
    "quit",	VQUIT,	CTRL('\\'),		"generate quit signal",
    "susp",	VSUSP,	_POSIX_VDISABLE,	"useless on Amoeba",
    "start",	VSTART,	CTRL('q'),		"suspend output",
    "stop",	VSTOP,	CTRL('s'),		"resume output"
};

/* Default setting for non-canonical read: block, 1 character	*/
#define	DFLT_TIME	0
#define	DFLT_MIN	1
#define DFLT_ROWS	24
#define DFLT_COLS	80
/* Character size, baud rate	*/
#define DFLT_CSIZE	CS8	/* This one is in c_cflag	*/
#define DFLT_BAUD	B9600	/* Default baud rate. Do we want this? */
#define BAUD_BITS	0x0f	/* Which bits in c_cflags are the baudrate */

int bauds[] = {		/* Hmm... Is it ok to assume these definitions? */
	0, 50, 75, 110, 134, 150, 200, 300, 600,
	1200, 1800, 2400, 4800, 9600, 19200, 38400
};



	/********************************\
	 *				*
	 *	Printing routines	*
	 *				*
	\********************************/

char buffer[R_MARGIN];	/* This line can be used to sprintf on	*/

prints(s)
    char *s;
{
    static int position = 0;	/* Position within line	*/
    int len;
    position += len = strlen(s);
    if (s[0] == '\n') {		/* Special case	*/
	position = len - 1;
    } else if (position > R_MARGIN) {	/* Wrap line	*/
	putchar('\n');
	position = len;
    }
    fputs(s, stdout);
} /* prints */

/*
 *	Print flags unless they have the "usual" value or Everything is set
 */
print_flags(table, n, flags)
    struct flags table[];
    int n;	/* Size of the table	*/
    tcflag_t flags;	/* Actual flags	*/
{
    struct flags *fp;
    for (fp = &table[0]; fp < &table[n]; ++fp) {
	if (Everything || (fp->f_bit & flags) != fp->f_usual) {
	    sprintf(buffer, (fp->f_bit & flags) ? "%s " : "-%s ", fp->f_name);
	    prints(buffer);
	}
    }
} /* print_flags */

/*
 *	Printout the current tty settings
 */
printflags()
{
    print_flags(i_flags, SIZEOFTABLE(i_flags), Tios.c_iflag);
    print_flags(o_flags, SIZEOFTABLE(o_flags), Tios.c_oflag);
    print_flags(c_flags, SIZEOFTABLE(c_flags), Tios.c_cflag);
    print_flags(l_flags, SIZEOFTABLE(l_flags), Tios.c_lflag);
} /* printflags */

/*
 *	Print a char from the c_cc array
 */
printchar(cp)
    struct cc *cp;
{
    char *s;	/* Name of the thing in ascii				*/
    cc_t ch;	/* Which char we're talking about - offset in c_cc	*/
    int usual;	/* What its usual value is				*/

    s = cp->c_name;
    if (Everything) usual = -3;	/* Bogus; makes sure it gets printed	*/
    else usual = cp->c_usual;
    ch = Tios.c_cc[cp->c_index];
    if (ch != usual) {
	if (ch == _POSIX_VDISABLE) 		/* Turned off	*/
	    sprintf(buffer, "-%s ", s);
	else if (ch < 0x1f)
	    sprintf(buffer, "%s '^%c' ", s, ch + 'A' - 1);
	else if (isascii(ch) && isprint(ch))
	    sprintf(buffer, "%s %c ", s, ch);
	else
	    sprintf(buffer, "%s 0x%x ", s, ch);
	prints(buffer);
    }
} /* printchar */

printttychars()
{
    int i;
    for (i = 0; i < SIZEOFTABLE(c_chars); ++i)
	printchar(&c_chars[i]);
    if (Everything || Tios.c_cc[VMIN] != DFLT_MIN) {
	sprintf(buffer, "min %d ", Tios.c_cc[VMIN]);
	prints(buffer);
    }
    if (Everything || Tios.c_cc[VTIME] != DFLT_TIME) {
	sprintf(buffer, "time %d ", Tios.c_cc[VTIME]);
	prints(buffer);
    }
    if (Everything || (Tios.c_cflag & BAUD_BITS) != DFLT_BAUD) {
	sprintf(buffer, "baud %d ", bauds[Tios.c_cflag & BAUD_BITS]);
	prints(buffer);
    }
    if (Everything || (Tios.c_cflag & CSIZE) != DFLT_CSIZE)
	switch (Tios.c_cflag & CSIZE) {
	case CS5: prints("cs5 "); break;
	case CS6: prints("cs6 "); break;
	case CS7: prints("cs7 "); break;
	case CS8: prints("cs8 "); break;
	default: prints("-csize "); break;	/* This cannot happen	*/
    }
#ifdef TIOS_GETWSIZE
    if ((Everything || Size) && GetwsErr == STD_OK && Gottios) {
	sprintf(buffer, "rows %d ", Rows);
	prints(buffer);
	sprintf(buffer, "cols %d ", Cols);
	prints(buffer);
    }
#endif /* TIOS_GETWSIZE */
} /* printttychars */




	/********************************************************\
	 *							*
	 *	To get the tty descriptor and capability	*
	 *							*
	\********************************************************/

/*
 *	Make sure you got a tty capability
 */
getttycap()
{
    if (Ttycap == NULL && (Ttycap = getcap("TTY")) == NULL) {
	fprintf(stderr, "%s: No \"TTY\" in cap-env\n", Progname);
	exit(2);
    }
} /* getttycap */

/*
 *	Make sure you got the tty descriptor:
 */
static void
gettios()
{
    errstat err;
    if (Gottios) return;
    /* Get tty status */
    if (Ttycap == NULL) getttycap();
    if ((err = tios_getattr(Ttycap, &Tios)) != STD_OK) {
	if (Ttyname == NULL) fprintf(stderr, "%s: cannot stat tty (%s)\n",
		Progname, err_why(err));
	else fprintf(stderr, "%s: cannot stat tty \"%s\" (%s)\n",
		Progname, Ttyname, err_why(err));
	exit(2);
    }
#ifdef TIOS_GETWSIZE
    GetwsErr = tios_getwsize(Ttycap, &Cols, &Rows);
    if (GetwsErr != STD_OK && GetwsErr != STD_COMBAD) {
	if (Ttyname == NULL)
	    fprintf(stderr, "%s: cannot get window size (%s)\n",
				Progname, err_why(GetwsErr));
	else
	    fprintf(stderr, "%s: cannot get window size of \"%s\" (%s)\n",
				Progname, Ttyname, err_why(GetwsErr));
    }
#endif /* TIOS_GETWSIZE */
    Gottios = Yes;
} /* gettios */




	/********************************\
	 *				*
	 *	Option processing	*
	 *				*
	\********************************/

/*
 *	Try finding a bit-name in a table
 */
int
intable(table, size, name)
    struct flags table[];
    int size;
    char *name;
{
    int i;
    for (i = 0; i < size; ++i)
	if (strcmp(table[i].f_name, name) == 0) return i;
    return -1;
} /* intable */

typedef enum { plus, minus, no } action_t; /* Optional first char of option */

/*
 *	Find suitable value for c_cc[i].
 *	Eats an argument if it needs one.
 */
int
getoptchar(action, i)
    action_t action;
    int i;	/* index in c_chars	*/
{
    switch((int) action) {
    case plus:	/* Set to default	*/
	return c_chars[i].c_usual;
    case minus:	/* Turn off		*/
	return _POSIX_VDISABLE;
    case no:	/* Set to argument	*/
	if (*Argv == NULL) {
	    fprintf(stderr, "%s: option %s expects an argument\n",
		Progname, c_chars[i].c_name);
	    exit(1);
	}

	/* Strip quotes (they may be mine!)	*/
	while (*Argv[0] == '\'' && strlen(*Argv) > 1
	 && Argv[0][strlen(*Argv) - 1] == '\'') {
	    Argv[0][strlen(*Argv) - 1] = '\0';
	    ++*Argv;
	}

	if (strlen(*Argv) == 1) {
	    /* Single char		*/
	    ++Argv;
	    return Argv[-1][0];
	} else if (strlen(*Argv) == 2 && *Argv[0] == '^') {
	    /* Control shorthand	*/
	    ++Argv;
	    return CTRL(Argv[-1][1]);
	} else return getoptnum(c_chars[i].c_name);
    }
    assert(0);	/*NOTREACHED*/
} /* getoptchar */

int
digit(ch, base, name)
    int ch, base;
    char *name;
{
    /* Get digit	*/
    if (ch <= '9' && ch >= '0') ch -= '0';
    else if (ch <= 'z' && ch >= 'a') ch += 10 - 'a';
    else if (ch <= 'Z' && ch >= 'A') ch += 10 - 'A';
    else {
	fprintf(stderr, "%s: (in parameter to %s) not a digit %c\n",
	    Progname, name, ch);
	exit(1);
    }
    if (ch >= base) {
	fprintf(stderr, "%s: (in parameter to %s) not a radix-%d digit %c\n",
	    Progname, name, ch);
	exit(1);
    }
    return ch;
} /* digit */

int
getoptnum(name)
    char *name;
{
    int n, base;
    char *p;

    if (*Argv == NULL) {
	fprintf(stderr, "%s: option %s requires a numeric parameter\n",
	    Progname, name);
	exit(1);
    }

    n = 0; p = *Argv++;
    if (p == NULL) abort();
    base = 10;
    if (*p == '0') {
	base = 8;
	if (*++p == 'x') {
	    base = 16;
	    ++p;
	}
    }

    while (*p != '\0')
	if (*p < '0' || *p > '9') {
	    fprintf(stderr, "%s: option %s needs a numeric parameter\n",
		Progname, name);
	    exit(1);
	} else n = base * n + digit(*p++, base, name);
    return n;
} /* getoptnum */

/*
 *	Set a flag in some c_?flag field; tries all flags in order
 *	Return success/failure
 */
int
setttyflag(action, name)
    action_t action;
    char *name;
{
    int i;

    if ((i = intable(i_flags, SIZEOFTABLE(i_flags), name)) >= 0) {
	gettios();
	switch ((int) action) {	/* Can only switch on ints in C... */
	case no:	/* Set bit */
	    Tios.c_iflag |= i_flags[i].f_bit;
	    break;
	case minus:	/* Reset bit */
	    Tios.c_iflag &= ~i_flags[i].f_bit;
	    break;
	case plus:	/* Set to default value */
	    Tios.c_iflag &= ~i_flags[i].f_bit;
	    Tios.c_iflag |= i_flags[i].f_usual;
	}
	Changed = Yes;
	return Yes;
    }

    if ((i = intable(o_flags, SIZEOFTABLE(o_flags), name)) >= 0) {
	gettios();
	switch ((int) action) {	/* Can only switch on ints in C... */
	case plus:	/* Set bit */
	    Tios.c_oflag |= o_flags[i].f_bit;
	    break;
	case minus:	/* Reset bit */
	    Tios.c_oflag &= ~o_flags[i].f_bit;
	    break;
	case no:	/* Set to usual value */
	    Tios.c_oflag &= ~o_flags[i].f_bit;
	    Tios.c_oflag |= o_flags[i].f_usual;
	}
	Changed = Yes;
	return Yes;
    }

    if ((i = intable(c_flags, SIZEOFTABLE(c_flags), name)) >= 0) {
	gettios();
	switch ((int) action) {	/* Can only switch on ints in C... */
	case plus:	/* Set bit */
	    Tios.c_cflag |= c_flags[i].f_bit;
	    break;
	case minus:	/* Reset bit */
	    Tios.c_cflag &= ~c_flags[i].f_bit;
	    break;
	case no:	/* Set to usual value */
	    Tios.c_cflag &= ~c_flags[i].f_bit;
	    Tios.c_cflag |= c_flags[i].f_usual;
	}
	Changed = Yes;
	return Yes;
    }

    if ((i = intable(l_flags, SIZEOFTABLE(l_flags), name)) >= 0) {
	gettios();
	switch ((int) action) {	/* Can only switch on ints in C... */
	case plus:	/* Set bit */
	    Tios.c_lflag |= l_flags[i].f_bit;
	    break;
	case minus:	/* Reset bit */
	    Tios.c_lflag &= ~l_flags[i].f_bit;
	    break;
	case no:	/* Set to usual value */
	    Tios.c_lflag &= ~l_flags[i].f_bit;
	    Tios.c_lflag |= l_flags[i].f_usual;
	}
	Changed = Yes;
	return Yes;
    }

    /* It may be the name of a c_cc char: */
    for (i = 0; i < SIZEOFTABLE(c_chars); ++i)
	if (strcmp(name, c_chars[i].c_name) == 0) {
	    gettios();
	    Tios.c_cc[c_chars[i].c_index] = getoptchar(action, i);
	    Changed = Yes;
	    return Yes;
	}

    return 0;	/* Not found */
} /* setttyflag */


/*
 * These functions are called as: (action_t action, char *option_name);
 */
void
	opt_help(), opt_tty(), opt_all(), opt_min(),
	opt_time(), opt_reset(), opt_cs(), opt_baud();
#ifdef TIOS_GETWSIZE
void	opt_setsize(), opt_rows(), opt_cols();
#endif /* TIOS_GETWSIZE */

/*
 *	This table only contains options that have
 *	a different name than a posix tty flag.
 */
struct {
    char *o_name;	/* Name of the option; usage string		*/
    void (*o_func)();	/* Called with an action_t and its own name	*/
} option_table[] = {
    "tty",	opt_tty,	/* Which tty		*/
    "help",	opt_help,	/* Help user of sty	*/
    "reset",	opt_reset,	/* Default termios	*/
    "all",	opt_all,	/* Be verbose		*/
    "min",	opt_min,	/* Set c_cc[VMIN]	*/
    "time",	opt_time,	/* Set c_cc[VTIME]	*/
#ifdef TIOS_GETWSIZE
    "size",	opt_setsize,	/* Show tty-size	*/
    "rows",	opt_rows,
    "cols",	opt_cols,
#endif /* TIOS_GETWSIZE */
    "cs5",	opt_cs,		/* # bits per byte	*/
    "cs6",	opt_cs,
    "cs7",	opt_cs,
    "cs8",	opt_cs,
    "baud",	opt_baud	/* Set baudrate		*/
};

#ifdef TIOS_GETWSIZE

/*ARGSUSED*/
void
opt_setsize(action, name)
    action_t action;
    char *name;
{
    static int called_yet = No;

    if (called_yet) {
	fprintf(stderr, "%s: don't you repeat the %s option\n", Progname, name);
	exit(1);
    }
    called_yet = Yes;
    if (Changed) {
	fprintf(stderr, "%s: option \"size\" and \"reset\" do not mix\n");
	exit(2);
    }
    gettios();
    Size = Yes;
} /* opt_setsize */

/*ARGSUSED*/ void
opt_rows(action, name)
    action_t action;
    char *name;
{
    gettios();
    switch ((int) action) {
    case plus:	/* Default:	*/
	Rows = DFLT_ROWS;
	break;
    case no:
	Rows = getoptnum(name);
	break;
    case minus:
	fprintf(stderr, "%s: Option %s requires integer parameter\n",
		Progname, name);
	exit(1);
	break;
    }
    ChangedSize = Yes;
} /* opt_rows */

/*ARGSUSED*/ void
opt_cols(action, name)
    action_t action;
    char *name;
{
    gettios();
    switch ((int) action) {
    case plus:	/* Default:	*/
	Cols = DFLT_COLS;
	break;
    case no:
	Cols = getoptnum(name);
	break;
    case minus:
	fprintf(stderr, "%s: Option %s requires integer parameter\n",
		Progname, name);
	exit(1);
	break;
    }
    ChangedSize = Yes;
} /* opt_cols */

#endif /* TIOS_GETWSIZE */

/*
 *	Set the CSIZE bits in c_cflag
 */
/*ARGSUSED*/
void
opt_cs(action, name)
    action_t action;
    char *name;
{
    gettios();
    Tios.c_cflag &= ~CSIZE;	/* Reset size bits	*/
    switch (name[strlen(name) - 1]) {
    case '5':
	Tios.c_cflag |= CS5;	/* Set to 5 bits/byte	*/
	break;
    case '6':
	Tios.c_cflag |= CS6;	/* Set to 6 bits/byte	*/
	break;
    case '7':
	Tios.c_cflag |= CS7;	/* Set to 7 bits/byte	*/
	break;
    case '8':
	Tios.c_cflag |= CS8;	/* Set to 8 bits/byte	*/
	break;
    default:
	assert(0);
	break;
    }
    Changed = Yes;
} /* opt_cs */

void
opt_baud(action, name)
    action_t action;
    char *name;
{
    unsigned baud;
    gettios();
    switch ((int) action) {
    case plus:
	baud = DFLT_BAUD;
	break;
    case no:
    case minus:
	/* Hmm... Or should I do a lookup in bauds[]? */
	switch (baud = getoptnum(name)) {
	case 0: baud = B0; break;
	case 50: baud = B50; break;
	case 75: baud = B75; break;
	case 110: baud = B110; break;
	case 134: baud = B134; break;
	case 150: baud = B150; break;
	case 200: baud = B200; break;
	case 300: baud = B300; break;
	case 600: baud = B600; break;
	case 1200: baud = B1200; break;
	case 1800: baud = B1800; break;
	case 2400: baud = B2400; break;
	case 4800: baud = B4800; break;
	case 9600: baud = B9600; break;
	case 19200: baud = B19200; break;
	case 38400: baud = B38400; break;
	default: fprintf(stderr, "%s: illegal baudrate %d\n", Progname, baud);
	}
    }
    Tios.c_cflag &= ~BAUD_BITS;
    Tios.c_cflag |= baud;
    Changed = Yes;
} /* opt_baud */

char UsageString[] = "\
Usage: 'sty [tty <cap>] [reset] [ cs[5678] ] [ time <n> ] [ min <n> ]\n\
\t[ rows <n> ] [ cols <n> ] [ size ]\n\
\t[ [-+]flag ]... [ (-+)char ]... [ char value ]...'\n\
\tor: 'sty [tty <cap>] all' or: 'sty [-+]help'\n";

/*ARGSUSED*/ void
opt_help(action, name)
    action_t action;
    char *name;
{
    int i;

    switch (action) {
    case minus:
	printf("%sUse '%s help' for more help\n", UsageString, Progname);
	break;

    case no:	/* Print chars, flags and the defaults	*/
	printf(UsageString);
	prints("Ttyflags [and their defaults] are: ");
	for(i = 0; i < SIZEOFTABLE(i_flags); ++i) {
	    sprintf(buffer, "%s [%s] ",
		i_flags[i].f_name, i_flags[i].f_usual ? "on" : "off");
	    prints(buffer);
	}
	for(i = 0; i < SIZEOFTABLE(o_flags); ++i) {
	    sprintf(buffer, "%s [%s] ",
		o_flags[i].f_name, o_flags[i].f_usual ? "on" : "off");
	    prints(buffer);
	}
	for(i = 0; i < SIZEOFTABLE(c_flags); ++i) {
	    sprintf(buffer, "%s [%s] ",
		c_flags[i].f_name, c_flags[i].f_usual ? "on" : "off");
	    prints(buffer);
	}
	for(i = 0; i < SIZEOFTABLE(l_flags); ++i) {
	    sprintf(buffer, "%s [%s] ",
		l_flags[i].f_name, l_flags[i].f_usual ? "on" : "off");
	    prints(buffer);
	}
	prints("\n\nTtychars [and their defaults] are: ");
	for (i = 0; i < SIZEOFTABLE(c_chars); ++i) {
	    sprintf(buffer, "%s ", c_chars[i].c_name);
	    prints(buffer);
	    if (c_chars[i].c_usual == _POSIX_VDISABLE)
		sprintf(buffer, "[off] ");
	    else
		sprintf(buffer, "[^%c] ", c_chars[i].c_usual + 'A' - 1);
	    prints(buffer);
	}
	/* Pretend time, min, rows, cols and baud are ttychars */
	sprintf(buffer, "time [%d] min [%d] ", DFLT_TIME, DFLT_MIN);
	prints(buffer);
	prints("rows [%d] cols [%d] baud [9600] ", DFLT_ROWS, DFLT_COLS);
	printf("\nAnd '%s +help' will tell you even more!\n", Progname);
	break;

    case plus:	/* Print meanings as well; one flag/char per line	*/
	printf("\n\nTtyflags [their defaults] and interpretation are:\n");
	for(i = 0; i < SIZEOFTABLE(i_flags); ++i)
	    printf("%-9s %s %s\n",
		i_flags[i].f_name, i_flags[i].f_usual ? "[on]  " : "[off] ",
		i_flags[i].f_xplain);
	for(i = 0; i < SIZEOFTABLE(o_flags); ++i)
	    printf("%-9s %s %s\n",
		o_flags[i].f_name, o_flags[i].f_usual ? "[on]  " : "[off] ",
		o_flags[i].f_xplain);
	for(i = 0; i < SIZEOFTABLE(c_flags); ++i)
	    printf("%-9s %s %s\n",
		c_flags[i].f_name, c_flags[i].f_usual ? "[on]  " : "[off] ",
		c_flags[i].f_xplain);
	for(i = 0; i < SIZEOFTABLE(l_flags); ++i)
	    printf("%-9s %s %s\n",
		l_flags[i].f_name, l_flags[i].f_usual ? "[on]  " : "[off] ",
		l_flags[i].f_xplain);

	printf("\nTtychars [their defaults] and function are:\n");
	for (i = 0; i < SIZEOFTABLE(c_chars); ++i) {
	    printf("%-9s ", c_chars[i].c_name);
	    if (c_chars[i].c_usual == _POSIX_VDISABLE)
		printf("[off]");
	    else
		printf("[^%c] ", c_chars[i].c_usual + 'A' - 1);
	    printf("  %s\n", c_chars[i].c_xplain);
	}
	/* Pretend some misc stuff are chars as well:	*/
	printf("%-9s [%4d] %s\n", "baud", 9600, "baudrate");
	printf("%-9s [%3d]  %s\n", "time", DFLT_TIME, "raw input timeout");
	printf("%-9s [%3d]  %s\n", "min", DFLT_MIN, "raw input minimum");
	printf("%-9s [%2d]  %s\n", "rows", DFLT_ROWS, "vertical tty size");
	printf("%-9s [%2d]  %s\n", "cols", DFLT_COLS, "horizontal tty size");
	printf("%-9s        %s\n", "size", "show tty size");
	break;
    }
    exit(action == minus ? 1 : 0);
} /* opt_help */

/*
 *	Set Tios to default values; pretend you got a correct one
 */
/*ARGSUSED*/ void
opt_reset(action, name)
    action_t action;
    char *name;
{
    int flag, i;
    static int called_yet = No;

    if (called_yet) {
	fprintf(stderr, "%s: don't you repeat the %s option\n", Progname, name);
	exit(1);
    }
    called_yet = 1;
    if (Changed) {
	fprintf(stderr,
	    "%s: option %s must not be preceeded by flags or chars\n",
	    Progname, name);
	exit(1);
    }
    if (action == plus) {	/* Maybe they mean something with the '+'? */
	fprintf(stderr,
	    "%s: just say %s if you mean %s\n", Progname, name, name);
	exit(1);
    }
    getttycap();		/* NOT the descriptor!	*/

    /* Fill Tios - first the flags	*/
    flag = 0;
    for (i = SIZEOFTABLE(i_flags); i--; ) flag |= i_flags[i].f_usual;
    Tios.c_iflag = flag;
    flag = 0;
    for (i = SIZEOFTABLE(o_flags); i--; ) flag |= o_flags[i].f_usual;
    Tios.c_oflag = flag;
    flag = DFLT_CSIZE|DFLT_BAUD;
    for (i = SIZEOFTABLE(c_flags); i--; ) flag |= c_flags[i].f_usual;
    Tios.c_cflag = flag;
    flag = 0;
    for (i = SIZEOFTABLE(l_flags); i--; ) flag |= l_flags[i].f_usual;
    Tios.c_lflag = flag;

    /* Posix doesn't specify what to do with these; may not be c_cc stuff */
    if (VTIME >= 0) Tios.c_cc[VTIME] = DFLT_TIME;
    if (VMIN >= 0) Tios.c_cc[VMIN] = DFLT_MIN;
    /* Then the c_cc stuff	*/
    for (i = SIZEOFTABLE(c_chars); i--; )
	Tios.c_cc[c_chars[i].c_index] = c_chars[i].c_usual;
    Changed = 1;
} /* opt_reset */

/*
 *	These two cannot be inserted in c_cc;
 *	because Posix specifies something wierd.
 */
/*ARGSUSED*/ void
opt_time(action, name)
    action_t action;
    char *name;
{
    gettios();
    switch ((int) action) {
    case plus:	/* Default:	*/
	Tios.c_cc[VTIME] = DFLT_TIME;
	break;
    case no:
	Tios.c_cc[VTIME] = getoptnum(name);
	break;
    case minus:
	Tios.c_cc[VTIME] = 0;	/* Timer off; NOT _POSIX_VDISABLE!	*/
	break;
    }
    Changed = Yes;
} /* opt_time */

/*ARGSUSED*/ void
opt_min(action, name)
    action_t action;
    char *name;
{
    gettios();
    switch ((int) action) {
    case plus:	/* Default:	*/
	Tios.c_cc[VMIN] = DFLT_MIN;
	break;
    case no:
	Tios.c_cc[VMIN] = getoptnum(name);
	break;
    case minus:
	Tios.c_cc[VMIN] = 0;	/* Blocking off; NOT _POSIX_VDISABLE!	*/
	break;
    }
    Changed = Yes;
} /* opt_min */

/*ARGSUSED*/ void
opt_tty(action, name)
    action_t action;
    char *name;
{
    errstat err;

    if (Changed) {
	fprintf(stderr, "%s: cannot precede option %s with any other option\n",
	    Progname, name);
    }
    if (Ttycap != NULL) {
	fprintf(stderr, "%s: only ONE %s option allowed\n",
	    Progname, name);
	exit(1);
    }
    if (*Argv == NULL) {
	fprintf(stderr, "%s: Option %s requires pathname\n", Progname, name);
	exit(1);
    }

    if ((err = name_lookup(Ttyname = *Argv, Ttycap = &cap)) != STD_OK) {
	fprintf(stderr, "%s: cannot lookup %s (%s)\n",
	    Progname, *Argv, err_why(err));
	exit(2);
    }
    ++Argv;	/* Skip this option's argument	*/
} /* opt_tty */

/*ARGSUSED*/ void
opt_all(action, name)
    action_t action;
    char *name;
{
    Everything = 1;
} /* opt_all */

/*
 *	Process an option
 */
int
doopt()
{
    char *s;
    action_t action;
    int i;

    if (*Argv == NULL) return No;	/* Ready		*/
    s = *Argv++;			/* Eat an option	*/
    if (s[0] == '-') {
	++s; action = minus;
    } else if (s[0] == '+') {
	++s; action = plus;
    } else action = no;

    /* See if it is a special option: */
    for (i = 0; i < SIZEOFTABLE(option_table); ++i)
	if (strcmp(option_table[i].o_name, s) == 0) {
	    (*option_table[i].o_func)(action, s);
	    return Yes;	/* More options */
	}
    /* Maybe it is a posix tty flag: */
    if (setttyflag(action, s)) return Yes;

    fprintf(stderr, "%s: unknown option \"%s\"\n", Progname, s);
    opt_help(minus, "help");	/* This exits	*/
    /*NOTREACHED*/ /* NOTREACHED */ /* _NOT_ REACHED!!!!! */
} /* doopt */


/*ARGSUSED*/
main(argc, argv)
    int argc;
    char *argv[];
{
    errstat err;

    /* Process arguments	*/
    Argv = argv + 1; Progname = argv[0];
    while (doopt())
	;

    /*
     *	What did we do?
     *		changed something => set tty
     *		otherwise show the settings.
     */

    if (Changed || ChangedSize) {
	assert(Ttycap != NULL);	/* Otherwise it changed garbage	*/
	if (Changed) {
	    if ((err = tios_setattr(Ttycap, TCSANOW, &Tios)) != STD_OK) {
		fprintf(stderr, "%s: cannot set tty (%s)\n",
			Progname, err_why(err));
		exit(2);
	    }
	}
	if (ChangedSize) {
	    if ((err = tios_setwsize(Ttycap, Cols, Rows)) != STD_OK) {
		fprintf(stderr, "%s: cannot set tty size (%s)\n",
			Progname, err_why(err));
		exit(2);
	    }
	}
    } else {
	gettios();
	/* Print results		*/
	printflags();
	printttychars();
	putchar('\n');
    }
    return 0;
} /* main */
