/*	@(#)ajputs.c	1.6	96/03/04 13:22:46 */
/*
 * Copyright 1996 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Low-level debugging output, directly to Amoeba STDERR */

#include <amoeba.h>
#include <string.h>
#include <file.h>
#include <amtools.h>

int
_ajax_puts(s)
	char *s;
{
	capability *cap;
	char buf[256];
	
	if ((cap = getcap("STDERR")) == 0)
		return 0;
	(void) strcat(strcpy(buf, s), "\n");
	(void) fswrite(cap, 0L, buf, (long)strlen(buf));
	return 1;
}

static char *
addnum(p, n)
	char *p;
	int n;
{
	if (n >= 10)
		p = addnum(p, n/10);
	*p++ = '0' + ((n%10)&0xf);
	return p;
}

int
_ajax_putnum(s, n)
	char *s;
	int n;
{
	char buf[256];
	char *p;
	
	strcpy(buf, s);
	p = buf + strlen(buf);
	*p++ = ':';
	*p++ = ' ';
	if (n < 0) {
		*p++ = '-';
		n = -n;
	}
	p = addnum(p, n);
	*p = '\0';
	return _ajax_puts(buf);
}
