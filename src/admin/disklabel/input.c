/*	@(#)input.c	1.7	96/02/27 10:12:37 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * INPUT ROUTINES
 *
 *      This file contains the input handling routines and tty mode setting
 *	code.
 */

#include "amoeba.h"
#include "ctype.h"
#include "sgtty.h"
#include "signal.h"
#include "stdlib.h"
#include "module/disk.h"
#include "vdisk/disklabel.h"
#include "dsktab.h"
#include "proto.h"

#define	BEL	07

/*
 * TTY MODES
 *
 *	Routines to set and restore the tty modes while we are playing with
 *	menus.  We only bother to trap the most probable signals for restoring
 *	the tty mode on interrupt.  We also arrogantly assume that no one else
 *	has already set signal handlers which we should also honour.
 */
static struct sgttyb orgstate, newstate;

static void
restore_tty_mode()
{
    stty(0, &orgstate);
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
}


/*ARGSUSED*/
static void
restore_and_exit(intr)
int	intr;	/* the argument from signal() */
{
    stty(0, &orgstate);
    printf("\n");
    exit(1);
}


static void
set_tty_mode()
{
    gtty(0, &orgstate);
    newstate = orgstate;
    newstate.sg_flags |= CBREAK;
    newstate.sg_flags &= ~ECHO;
    /* If we get an interrupt we restore the tty modes before we die */
    signal(SIGINT, restore_and_exit);
    signal(SIGQUIT, restore_and_exit);
    stty(0, &newstate);
}

/*
 * INPUT ROUTINES
 */

static int
getrawchar()
{
    int c;

    c = getchar();
    if (c == '\r')
	c = '\n';
    if (isprint(c) || c == '\b' || c == BEL || c == '\n')
	putchar(c);
    else
	putchar('?');
    return c;
}

/*
 * getnum
 *
 *	Reads an unsigned decimal integer from the keyboard.
 *	It takes as parameter the first character that will form the
 *	the number.  The reason for this abysmal hack is so that you
 *	can read the character in advance and then decide if you need
 *	to call getnum or some other routine.  To call this routine
 *	when you don't want to preread do getnum(getchar()).
 *
 *	NB.  You can't just press return and have that == 0
 *	     You can backspace past the beginning of the field.
 */

static int
getnum(digit)
int	digit;
{
    int	result;
    int position;

    position = 0;
    for (result = 0; ; digit = getchar())
	if (isdigit(digit))
	{
	    putchar(digit);
	    result = result * 10 + digit - '0';
	    position++;
	}
	else
	    if (digit == '\n' || digit == '\r')
	    {
		if (position > 0)
		{
		    putchar(digit);
		    return result;
		}
		else
		    putchar(BEL);
	    }
	    else
		if (digit == '\b' && position > 0)
		{
		    putchar('\b');
		    putchar(' ');
		    putchar('\b');
		    result /= 10;
		    position--;
		}
		else	/* invalid character: don't show it */
		{
		    putchar(BEL);
		}
}


/*
 * getnumber
 *
 *	For those that need to get a number without peeking at the first
 *	character
 */

int
getnumber(s)
char * s;
{
    int number;

    set_tty_mode();
    printf(s);
    number = getnum(getchar());
    restore_tty_mode();
    return number;
}


/*
 * getline
 *
 *	Reads until either a newline is found or its buffer is full.
 *	NB: getrawchar will suspend if there is no input.
 *	We process backspaces out of politeness.
 *	The newline is converted to a null byte to make a string of it.
 *	If p is initially >= e then a null byte is still installed at *p.
 */

void
getline(p, e)
char *	p;	/* pointer to start of input buffer */
char *	e;	/* pointer to end of input buffer */
{
    char * begin = p;

    set_tty_mode();
    while (p < e && (*p = getrawchar()) != '\n')
	if (*p != '\b')
	    p++;
	else
	    if (p > begin)
		p--;
    *p = '\0';
    restore_tty_mode();
}


/*
 * confirm
 *
 *	Repeatedly prints the given string (a yes/no question) and reads from
 *	the keyboard until it gets either a 'y', 'Y', 'n' or 'N' in response.
 *	Returns 1 for yes and 0 for no.
 */

int
confirm(s)
char *	s;			/* question string */
{
    char	answer;		/* response to yes/no questions */

    set_tty_mode();
    for (answer = 0;;)
    {
	printf("%s? [y or n]: ", s);
	answer = getrawchar();
	printf("\n");
	if (answer == 'n' || answer == 'N')
	{
	    restore_tty_mode();
	    return 0;
	}
	if (answer == 'y' || answer == 'Y')
	{
	    restore_tty_mode();
	    return 1;
	}
    }
}


/*
 * getfield routines
 *
 *	These routines print an enquiry string, followed by the current value
 *	(if already set) and wait for the user to enter a new value.  If a
 *	newline is entered then the previous value of the field remains the
 *	same.  Alas, once a non-newline is typed then backspacing to the
 *	beginning won't accept a <CR> as a valid answer.
 */

void
getfield32(s, number)
char *	s;
int32 *	number;
{
    int	digit;

    set_tty_mode();
    do
    {
	printf(s);
	if (*number >= 0)
	    printf("[%d] ", *number);
	digit = getchar();
	if (*number < 0 || digit != '\n')
	    *number = getnum(digit);
	else
	    putchar('\n');
    } while (*number < 0);
    restore_tty_mode();
}


void
getfield16(s, number)
char *	s;
int16 *	number;
{
    int	digit;

    set_tty_mode();
    do
    {
	printf(s);
	if (*number >= 0)
	    printf("[%d] ", *number);
	digit = getchar();
	if (*number < 0 || digit != '\n')
	    *number = getnum(digit);
	else
	    putchar('\n');
    } while (*number < 0);
    restore_tty_mode();
}


void
pause _ARGS(( void ))
{
    set_tty_mode();
    printf("\ncontinue ?");
    (void) getchar();
    /* erase the question */
    printf("\b\b\b\b\b\b\b\b\b\b");
    printf("          ");
    printf("\b\b\b\b\b\b\b\b\b\b");
    restore_tty_mode();
}
