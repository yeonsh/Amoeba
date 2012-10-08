/*	@(#)print.c	1.3	94/04/06 11:40:33 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "monitor.h"
#include "svr.h"
#include "time.h"

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

static int inited = 0;

/*
 *	Initialise.
 *	Try using the capabilities STDERR and STDOUT in turn
 *	otherwise lookup /tty:00
 */
static capability *
init()
{
    static capability *capp, tty;

    if ((capp = getcap("STDERR")) != NULL || (capp = getcap("STDOUT")) != NULL)
    	return capp;
    if ((capp = getcap("ROOT")) != NULL
    && dir_lookup(capp, "tty:00", &tty) == STD_OK) {
	return capp = &tty;
    }
    return (capability *) NULL;
} /* init */

capability *outcap;

static void
pri(i)
    int i;
{
    char buf[20], *p;
    int neg;
    if (neg = i < 0) i = -i;
    p = buf + sizeof(buf);
    do {
	*--p = '0' + i % 10;
	i /= 10;
    } while (i > 0);
    if (neg) *--p = '-';
    (void) capsaid(outcap, fswrite(outcap, 0L, p,
				   (long)(buf + sizeof(buf) - p)));
} /* pri */

#if __STDC__

void
prf(char *fmt, ...)
{
    va_list rest;
    char *todo;
    static mutex mu;
    va_start(rest, fmt);

#else /* __STDC__ */

/*VARARGS*/ void
prf(fmt, va_alist)
    char *fmt;
    va_dcl
{ /*}*/
    va_list rest;
    char *todo;
    static mutex mu;
    va_start(rest);

#endif

    if (!inited) {
	if ((outcap = init()) == NILCAP) abort();
	inited = 1;
    }
    if (intr_c2s()) return;	/* Interrupted */
    mu_lock(&mu);
    todo = fmt;
    while (*fmt != '\0' && !badport(outcap)) {
	if (*fmt != '%') ++fmt;
	else {
	    if (todo != fmt) {
		(void) capsaid(outcap, fswrite(outcap, 0L, todo,
					       (long)(fmt - todo)));
		todo = fmt + 2;	/* Past the 'd' of %d */
	    }
	    switch(*++fmt) {
		case 'd':	/* Int */
		    pri(va_arg(rest, int));
		    break;
		case 's': {	/* String */
		    char *tmp;
		    tmp = va_arg(rest, char *);
		    (void) capsaid(outcap, fswrite(outcap, 0L, tmp,
						   (long)strlen(tmp)));
		    break;
		}
		case 'n':	/* progname name, date */
		    if (TimePrint) {
			time_t now;
			char *p, *ctime();
			int len;
			(void) capsaid(outcap, fswrite(outcap, 0L,
						       "bootsvr ", 8L));
			(void) time(&now);
			p = ctime(&now); /* Protect with other ctime call? */
			len = strlen(p);
			len -= 6;
			p[len++] = ':'; p[len++] = ' ';
			/* Replace " 1990\n" with ": " */
			(void) capsaid(outcap, fswrite(outcap, 0L, p,
						       (long)len));
		    } else {
			(void) capsaid(outcap, fswrite(outcap, 0L,
						       "bootsvr: ", 9L));
		    }
		    break;
		case '%':
		    (void) capsaid(outcap, fswrite(outcap, 0L, "%", 1L));
		    break;
		default:
		    fswrite(outcap, 0L, "Bad format!\n", 12L);
		    abort();
	    }
	    todo = ++fmt;
	}
    }
    if (todo != fmt)
	(void) capsaid(outcap, fswrite(outcap, 0L, todo, (long)(fmt - todo)));
    mu_unlock(&mu);
    va_end(rest);
} /* prf */
