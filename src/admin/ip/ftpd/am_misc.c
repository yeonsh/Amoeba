/*	@(#)am_misc.c	1.1	96/02/27 10:13:48 */
/*
 * Misc support routines added for Amoeba.
 */

#include <stdio.h>
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

int
openlog(str, opt1, opt2)
char *str;
int   opt1;
int   opt2;
{
	/* dummy */
	return 0;
}

int
logwtmp(ttyline, name, host)
char *ttyline;
char *name;
char *host;
{
	/* dummy */
	return 0;
}

void
#ifdef __STDC__
syslog(int opt, const char *fmt, ...)
#else
syslog(opt, fmt, va_alist) int opt; char *fmt; va_dcl
#endif
{
	va_list ap;

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

int
initgroups(name, gid)
char *name;
int   gid;
{
	/* dummy */
	return 0;
}

static char *shells[] = {
    "/bin/sh",
    "/bin/ksh"
};
#define NUMBER_SHELLS	(sizeof(shells) / sizeof(char *))

static int which_shell = 0;

char *
getusershell()
{
    if (which_shell < NUMBER_SHELLS) {
	return shells[which_shell++];
    } else {
	return NULL;
    }
}

void
endusershell()
{
    which_shell = 0;
}
