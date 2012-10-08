/*	@(#)fdinit.c	1.4	96/02/27 11:05:34 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Data and initialization code for the file descriptor I/O module */
/* TO DO: use mutexes in closefiles? */

#include "ajax.h"
#include "fdstuff.h"
#include <caplist.h>

extern struct caplist *capv;

/* Global variables */

/* Tables */
struct fd _ajax_fd[NOFILE]; /* File descriptor blocks */
struct fd *_ajax_fdmap[NOFILE]; /* Handle to descriptor block map */

/* Other variables */
mutex _ajax_fdmu; /* Protects critical regions affecting fdmap or fd */
int _ajax_fdinited; /* Nonzero when initialization complete */

/* Stuff for on_exit */
typedef char * (*exitproc)(); /* Really recursive, but can't do that in C... */
extern exitproc _ajax_on_exit();
static exitproc nextinchain;

/* Forward declarations */
static void setup();
static void scancapv();
static void setfd();
static exitproc closefiles();

/* Initialize the fd world.
   This routine reads the process state that was passed at exec time
   and uses it to initialize its own process state.  There are ways to
   speed this code up, by passing more information in binary; for now,
   we use the existing caplist mechanism, which allows passing of an
   arbitrary number of named capabilities.  Only names starting with
   an underscore are used. */

void
_ajax_fdinit()
{
	static mutex mu;
	
	/* Here we MUST assume statically initialized mutexes are ready
	   for use, in unlocked state (in particular, 'mu' above) */
	
	MU_LOCK(&mu);
	if (!_ajax_fdinited) {
		long tout;
		
		/* Set the timeout to 10 seconds, unless already set */
		if ((tout = timeout(10000L)) != 0)
			timeout(tout);
		
		nextinchain = _ajax_on_exit(closefiles);
		scancapv();
		/* Add stdio streams if no Ajax open files found */
		setup(0, "STDIN",  FREAD | FWRITE);
		setup(1, "STDOUT", FREAD | FWRITE);
		setup(2, "STDERR", FREAD | FWRITE);
		/* (Yes, you can read from stdout or stderr if
		   it is a tty!  At least 'less' assumes this,
		   and works on real Unix, so what the heck...) */
		_ajax_fdinited = 1;
	}
	MU_UNLOCK(&mu);
}

/* Initialize pre-opened file descriptor */

static void
setup(i, name, flags)
	int i;
	char *name;
	int flags;
{
	struct fd *f;
	capability *cap;
	
	if ((cap = getcap(name)) == NILCAP)
		return;
	if (FD(i) != NILFD)
		return;
	f = &_ajax_fd[i];
	f->fd_flags = flags;
	f->fd_refcnt = 1;
	f->fd_pos = -1;
	f->fd_size = -1;
	f->fd_cap = *cap;
	f->fd_name[0] = '\0';
	mu_init(&f->fd_mu);
	FD(i) = f;
}

/* Scan a string for a colon followed by a number; return end of string */

static char *
getcnum(p, pnumber)
	char *p;
	long *pnumber;
{
	int negative;
	long number;
	int digit;
	
	if (p == NULL || *p++ != ':')
		return NULL;
	if (negative = (*p == '-'))
		++p;
	number = 0;
	while ((digit = *p - '0') >= 0 && digit <= 9) {
		number = 10*number + digit;
		++p;
	}
	*pnumber = negative ? -number : number;
	return p;
}

/* Scan the capability environment for names put into it by makecapv */

static void
scancapv()
{
	struct caplist *cl;
	
	for (cl = capv; cl->cl_name != NULL; ++cl) {
		char *p = cl->cl_name;
		long fd, fd2, flags, pos, size, mode, mtime;
		
		if (*p++ == '_') {
TRACE(p);
			switch (*p++) {
			
			case 'd': /* Dup descriptor */
				p = getcnum(p, &fd);
				p = getcnum(p, &fd2);
				if (p != NULL && fd >= 0 && fd < NOFILE &&
						fd2 >= 0 && fd2 < NOFILE) {
					struct fd *f = FD(fd2);
					if (f != NILFD) {
TRACE("dup");
						f->fd_refcnt++;
						FD(fd) = f;
					}
				}
				break;
				
			case 'f': /* File descriptor */
				p = getcnum(p, &fd);
				p = getcnum(p, &flags);
				p = getcnum(p, &pos);
				p = getcnum(p, &size);
				p = getcnum(p, &mode);
				p = getcnum(p, &mtime);
				if (p != NULL)
					setfd(fd, flags, pos, size,
						mode, mtime, cl->cl_cap);
				break;
			
			case 'b': /* Bullet file name and parent dir */
				p = getcnum(p, &fd);
				if (p != NULL && *p++ == ':' &&
						fd >= 0 && fd < NOFILE) {
					struct fd *f = FD(fd);
					if (f != NILFD) {
TRACE("bullet");
						f->fd_dircap = *cl->cl_cap;
						strncpy(f->fd_name,
								p, NAME_MAX);
						f->fd_name[NAME_MAX] = '\0';
					}
				}
				break;
			
			default:
				continue;
				/* Skip over cl->cl_name = NULL below */
			
			}
			/* Invalidate this entry and the rest of the list,
			 * so direct calls to amex() won't see it.
			 * Note that all file capabilities should be at the
			 * end of the capability environment.  Otherwise
			 * 'normal' capabilities would disappear from the
			 * environment as well.
			 */
			cl->cl_name = NULL;
		}
	}
}

static void
setfd(i, flags, pos, size, mode, mtime, cap)
	long i, flags, pos, size, mode, mtime;
	capability *cap;
{
	struct fd *f;
	
	if (i < 0 || i >= NOFILE || FD(i) != NILFD)
		return;
TRACENUM("setfd", i);
	f = &_ajax_fd[i];
	f->fd_refcnt = 1;
	f->fd_flags = flags;
	f->fd_pos = pos;
	f->fd_size = size;
	f->fd_mode = mode;
	f->fd_mtime = mtime;
	f->fd_cap = *cap;
	f->fd_name[0] = '\0';
	mu_init(&f->fd_mu);
	FD(i) = f;
}

static exitproc
closefiles()
{
	int i;
	struct fd *f;
	
	/* We don't try to get the locks for fd's on exit - we just close
	 * things in preparation for killing off the process.
	 * Any odd behaviour is the problem of the programmer.
	 */
	for (i = 0; i < NOFILE; ++i) {
		f = CLEANFD(FD(i));
		if (f != NILFD)
			_ajax_close(i, NILFD, 0);
	}
	return nextinchain;
}
