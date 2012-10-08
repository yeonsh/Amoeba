/*	@(#)cprintf.c	1.3	96/02/27 10:17:01 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * cprintf.c
 *
 * Console printf/getline primitives.
 */
#ifdef AMOEBA
#include "amtools.h"

static capability *ttycap;

int
cinit()
{
    /* we need tty capability for the <cr> input */
    return (ttycap = getcap("TTY")) != NILCAP;
}

/* ARGSUSED */
void
cprintf(fmt, a0, a1, a2, a3, a4)
    char *fmt;
{
    char buf[100];

    sprintf(buf, fmt, a0, a1, a2, a3, a4);
    (void) fswrite(ttycap, 0L, buf, (long)strlen(buf));
}

char *
cgetline()
{
    static char buf[100];

    buf[0] = '\0';
    (void) fsread(ttycap, 0L, buf, (long)sizeof(buf));
    return buf;
}
#endif /* AMOEBA */

#ifdef UNIX
static int ttyfd;

int
cinit()
{
    return (ttyfd = open("/dev/tty", 2)) >= 0;
}

/* ARGSUSED */
void
cprintf(fmt, a0, a1, a2, a3, a4)
    char *fmt;
{
    char buf[100];

    sprintf(buf, fmt, a0, a1, a2, a3, a4);
    write(ttyfd, buf, strlen(buf));
}

char *
cgetline()
{
    static char buf[100];

    read(ttyfd, buf, sizeof(buf));
    return buf;
}
#endif /* UNIX */
