/*	@(#)tc_ux_marsh.c	1.1	96/02/27 13:07:58 */
/*
 * tc_ux_marshal, tc_ux_unmarshal
 *	This version is used to convert between the UNIX termios control
 *	characters and the Amoeba termios control characters.  For good
 *	measure Amoeba uses unsigned shorts for the flags and UNIX uses
 *	unsigned longs or ints.
 *	Joy!
 */

#include "amoeba.h"
#include <termios.h>
#include <string.h>
#include <stdio.h>

typedef	unsigned short	unshrt;


#ifdef DEBUG

static void
print_tios(s, arg)
char * s;
struct termios * arg;
{
    fprintf(stderr, s);
    fprintf(stderr, "\n  iflag = 0%o, oflag = 0%o, cflag = 0%o, lflag = 0%o\n",
			arg->c_iflag,arg->c_oflag,arg->c_cflag,arg->c_lflag);
    fprintf(stderr, "  intr     = %x\n", arg->c_cc[VINTR]);
    fprintf(stderr, "  quit     = %x\n", arg->c_cc[VQUIT]);
    fprintf(stderr, "  erase    = %x\n", arg->c_cc[VERASE]);
    fprintf(stderr, "  kill     = %x\n", arg->c_cc[VKILL]);
    fprintf(stderr, "  eof/min  = %x\n", arg->c_cc[VMIN]);
    fprintf(stderr, "  eol/time = %x\n", arg->c_cc[VTIME]);
    fprintf(stderr, "  susp     = %x\n", arg->c_cc[VSUSP]);
}

#endif /* DEBUG */


char *
tc_ux_marshal(adv, arg, eok)
char *		adv;	/* Buffer to stuff data into */
struct termios	arg;	/* Data to stuff into buffer */
int		eok;	/* 0 if endian different from ours */
{
    if (eok)
    {
	*(unshrt *) adv = (unshrt) arg.c_iflag;
	adv += 2;
	*(unshrt *) adv = (unshrt) arg.c_oflag;
	adv += 2;
	*(unshrt *) adv = (unshrt) arg.c_cflag;
	adv += 2;
	*(unshrt *) adv = (unshrt) arg.c_lflag;
	adv += 2;
    }
    else
    {
	/* Byte swap */
	*(unshrt *) adv = (0xff & (arg.c_iflag>>8)) + (arg.c_iflag << 8);
	adv += 2;
	*(unshrt *) adv = (0xff & (arg.c_oflag>>8)) + (arg.c_oflag << 8);
	adv += 2;
	*(unshrt *) adv = (0xff & (arg.c_cflag>>8)) + (arg.c_cflag << 8);
	adv += 2;
	*(unshrt *) adv = (0xff & (arg.c_lflag>>8)) + (arg.c_lflag << 8);
	adv += 2;

    }
    *adv++ = 0;  /* Amoeba doesn't use the first one */
    *adv++ = arg.c_cc[VINTR];
    *adv++ = arg.c_cc[VQUIT];
    *adv++ = arg.c_cc[VERASE];
    *adv++ = arg.c_cc[VKILL];
    *adv++ = arg.c_cc[VEOF];
    *adv++ = arg.c_cc[VEOL];
    *adv++ = arg.c_cc[VSTART];
    *adv++ = arg.c_cc[VSTOP];
    *adv++ = arg.c_cc[VMIN];
    *adv++ = arg.c_cc[VTIME];
    *adv++ = arg.c_cc[VSUSP];
#ifdef DEBUG
    print_tios("tc_ux_marshal", &arg);
#endif
    return adv;
}


char *
tc_ux_unmarshal(adv, arg, eok)
char *			adv;	/* Buffer to stuff data into */
struct termios *	arg;	/* Data to stuff into buffer */
int			eok;	/* 0 if endian different from ours */
{
    if (eok)
    {
	arg->c_iflag = *(unshrt *) adv;
	adv += 2;
	arg->c_oflag = *(unshrt *) adv;
	adv += 2;
	arg->c_cflag = *(unshrt *) adv;
	adv += 2;
	arg->c_lflag = *(unshrt *) adv;
    }
    else
    {
	arg->c_iflag = (*(unshrt *) adv << 8) + ((*(unshrt *) adv >> 8) & 0xff);
	adv += 2;
	arg->c_oflag = (*(unshrt *) adv << 8) + ((*(unshrt *) adv >> 8) & 0xff);
	adv += 2;
	arg->c_cflag = (*(unshrt *) adv << 8) + ((*(unshrt *) adv >> 8) & 0xff);
	adv += 2;
	arg->c_lflag = (*(unshrt *) adv << 8) + ((*(unshrt *) adv >> 8) & 0xff);
    }
    adv += 3;
    (void) memset(arg->c_cc, 0, NCCS);
    arg->c_cc[VINTR]  = *adv++;
    arg->c_cc[VQUIT]  = *adv++;
    arg->c_cc[VERASE] = *adv++;
    arg->c_cc[VKILL]  = *adv++;
    arg->c_cc[VEOF]   = *adv++;
    arg->c_cc[VEOL]   = *adv++;
    arg->c_cc[VSTART] = *adv++;
    arg->c_cc[VSTOP]  = *adv++;
    /*
     * And now we have to be clever - VMIN == VEOF and VEOL == VTIME
     * under many unix systems :-(  We only fill in the VMIN and VTIME
     * if they are what we are actually using.
     */
    if ((arg->c_lflag & ICANON) == 0)  /* no erase and kill processing */
    {
	arg->c_cc[VMIN]   = *adv++;
	arg->c_cc[VTIME]  = *adv++;
    }
    else
	adv += 2;
    arg->c_cc[VSUSP]  = *adv++;

#ifdef __SVR4
    /*
     * In Solaris 2 they made an arguably incorrect implementation of OPOST.
     * Therefore the following hack:
     */
    if (arg->c_oflag == OPOST)
	arg->c_oflag = OPOST | ONLCR;

#endif /* __SVR4 */
#ifdef DEBUG
    print_tios("tc_ux_unmarshal", arg);
#endif

    return adv;
}
