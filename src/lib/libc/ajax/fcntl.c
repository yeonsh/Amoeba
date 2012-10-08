/*	@(#)fcntl.c	1.3	94/04/07 10:26:04 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* fcntl(2) system call emulation */

#include "ajax.h"
#include "fdstuff.h"
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#define RDWRFLAGS (O_RDWR | O_RDONLY | O_WRONLY)
#define USERFLAGS (O_APPEND | O_NONBLOCK)
	/* None of these are really supported, but what the heck... */

static int
_ajax_dup(from, f, i)
        int from;
        struct fd *f;
        int i;
{
        NEWFD2(i);
        f->fd_refcnt++;
        if (i < 3)
                _ajax_setstdcap(i, &f->fd_cap);
        RETNEWFD(i, SETCLEXEC(f, 0));
}

#ifdef __STDC__
int fcntl(int i, int cmd, ...)
#else
int fcntl(i, cmd, va_alist)
	int i;
	int cmd;
	va_dcl
#endif
{
	struct fd *f;
	int arg;
	va_list ap;
#ifdef __STDC__
#define GET_ARG(a,t) { va_start(ap, cmd); { a = va_arg(ap, t); } va_end(ap); }
#else
#define GET_ARG(a,t) { va_start(ap); { a = va_arg(ap, t); } va_end(ap); }
#endif
	int ret;
	
	FDINIT();
	FDLOCK(i, f);
	ret = 0;
	switch (cmd) {
	case F_DUPFD:
		GET_ARG(arg, int);
		if (arg < 0 || arg >= NOFILE) {
			FDUNLOCK(f);
			ERR(EINVAL, "fcntl: arg out of range");
		}
		ret = _ajax_dup(i, f, arg);
		break;
	case F_GETFD:
		ret = GETCLEXEC(FD(i));
		break;
	case F_SETFD:
		GET_ARG(arg, int);
		FD(i) = SETCLEXEC(f, (arg&1));
		break;
	case F_GETFL:
		ret = f->fd_flags & USERFLAGS;

		/* Turn off the bits that specify readable/writeable: */
		ret &= ~RDWRFLAGS;

		/* Translate FREAD/FWRITE to the user accessible values: */
		if (f->fd_flags & FREAD)
			if (f->fd_flags & FWRITE)
				ret |= O_RDWR;
			else
				ret |= O_RDONLY;
		else
			ret |= O_WRONLY;
		break;
	case F_SETFL:
		GET_ARG(arg, int);
		f->fd_flags = f->fd_flags & ~USERFLAGS | arg & USERFLAGS;
		break;
	/* F_GETOWN and F_SETOWN are not supported, because SIGIO and
	   SIGURG aren't (in Amoeba, asynchronous I/O must be done
	   completely different) */
	default:
		FDUNLOCK(f);
		ERR(EINVAL, "fcntl: invalid command");
	}
	FDUNLOCK(f);
	return ret;
}
