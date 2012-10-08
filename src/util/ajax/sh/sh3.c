/*	@(#)sh3.c	1.4	96/02/27 13:04:57 */
#define Extern extern
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <setjmp.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/file.h>
#include "sh.h"

/* -------- exec.c -------- */
/* #include "sh.h" */

/*
 * execute tree
 */

static	char	*signame[] = {
	"Signal 0",
	"Hangup",
	NULL,	/* interrupt */
	"Quit",
	"Illegal instruction",
	"Trace/BPT trap",
	"Abort",
	"EMT trap",
	"Floating exception",
	"Killed",
	"Bus error",
	"Memory fault",
	"Bad system call",
	NULL,	/* broken pipe */
	"Alarm clock",
	"Terminated",
};
#define	NSIGNAL (sizeof(signame)/sizeof(signame[0]))

static	struct	op *findcase();
static	void	brkset();
static	void	echo();
static	int	forkexec();
static	int	parent();

#ifdef AMOEBA

#include <thread.h>
#include <module/path.h>	/* For path_first function */

extern int errno, sys_nerr;
extern char *sys_errlist[];


#else	/* Not AMOEBA */

char *
path_first(dirlist, filename, path_buff)
        char *dirlist, *filename, *path_buff;
{
        register char *dirend, *rtn;
        register int dirlen;

        if (dirlist == NULL) return NULL;
        for (dirend = dirlist; *dirend != '\0' && *dirend != ':'; dirend++)
                /* no-op */ ;
        dirlen = dirend - dirlist;
        if (dirlen == 0 || *filename == '/')
                /* The first dir is an empty string or file is absolute: */
                (void)strcpy(path_buff, filename);
        else {
                (void)strncpy(path_buff, dirlist, dirlen);
                path_buff[dirlen] = '/';
                (void)strcpy(path_buff + dirlen + 1, filename);
        }
        rtn = *dirend == '\0' || *filename == '/' ? NULL : dirend + 1;
        return rtn;
}

#endif	/* Not AMOEBA */

static int ioredir();
static int undo_redir();

int
execute(t, pin, pout, act)
register struct op *t;
int *pin, *pout;
int act;
{
	register struct op *t1;
	int i, pv[2], rv, child, a;
	char *cp, **wp, **wp2;
	struct var *vp;
	struct brkcon bc;

#ifndef SIG_HANDLER_BUG_FIXED
	/* Sometimes interrupt handler doesn't get called, so
	 * sigaction.c has the following function to check whether
	 * the bug has occurred and, if so, call the user's handler:
	 */
	 sigcallpending();
#endif
	if (t == NULL)
		return(0);
	rv = 0;
	a = areanum++;
	wp = (wp2 = t->words) != NULL
	     ? eval(wp2, t->type == TCOM ? DOALL : DOALL & ~DOKEY)
	     : NULL;

	switch(t->type) {
	case TPAREN:
	case TCOM:
		rv = forkexec(t, pin, pout, act, wp, &child);
		if (child) {
			exstat = rv;
			leave();
		}
		break;

	case TPIPE:
		if ((rv = openpipe(pv)) < 0)
			break;
		pv[0] = remap(pv[0]);
		pv[1] = remap(pv[1]);
		(void) execute(t->left, pin, pv, 0);
		rv = execute(t->right, pv, pout, 0);
		break;

	case TLIST:
		(void) execute(t->left, pin, pout, 0);
		rv = execute(t->right, pin, pout, 0);
		break;

	case TASYNC:
		i = parent();
		if (i != 0) {
			if (i != -1) {
				setval(lookup("!"), putn(i));
				if (pin != NULL)
					closepipe(pin);
				if (talking) {
					prs(putn(i));
					prs("\n");
				}
			} else
				rv = -1;
			setstatus(rv);
		} else {
			signal(SIGINT, SIG_IGN);
			signal(SIGQUIT, SIG_IGN);
			if (talking)
				signal(SIGTERM, SIG_DFL);
			talking = 0;
			if (pin == NULL) {
				close(0);
				open("/dev/null", 0);
			}
			exit(execute(t->left, pin, pout, FEXEC));
		}
		break;

	case TOR:
	case TAND:
		rv = execute(t->left, pin, pout, 0);
		if ((t1 = t->right)!=NULL && (rv == 0) == (t->type == TAND))
			rv = execute(t1, pin, pout, 0);
		break;

	case TFOR:
		if (wp == NULL) {
			wp = dolv+1;
			if ((i = dolc) < 0)
				i = 0;
		} else {
			i = -1;
			while (*wp++ != NULL)
				;			
		}
		vp = lookup(t->str);
		while (setjmp(bc.brkpt))
			if (isbreak)
				goto broken;
		brkset(&bc);
		for (t1 = t->left; i-- && *wp != NULL;) {
			setval(vp, *wp++);

#ifndef SIG_HANDLER_BUG_FIXED
			/* Give a signal handler a chance to execute: */
			threadswitch();
#endif
			rv = execute(t1, pin, pout, 0);
		}
		brklist = brklist->nextlev;
		break;

	case TWHILE:
	case TUNTIL:
		while (setjmp(bc.brkpt))
			if (isbreak)
				goto broken;
		brkset(&bc);
		t1 = t->left;
		while ((execute(t1, pin, pout, 0) == 0) == (t->type == TWHILE)){
#ifndef SIG_HANDLER_BUG_FIXED
			/* Give a signal handler a chance to execute: */
			threadswitch();
#endif
			rv = execute(t->right, pin, pout, 0);
#ifndef SIG_HANDLER_BUG_FIXED
			threadswitch();
#endif
		}
		brklist = brklist->nextlev;
		break;

	case TIF:
	case TELIF:
		rv = !execute(t->left, pin, pout, 0)?
			execute(t->right->left, pin, pout, 0):
			execute(t->right->right, pin, pout, 0);
		break;

	case TCASE:
		if ((cp = evalstr(t->str, DOSUB|DOTRIM)) == 0)
			cp = "";
		if ((t1 = findcase(t->left, cp)) != NULL)
			rv = execute(t1, pin, pout, 0);
		break;

	case TBRACE:
		if ((rv = ioredir(t->ioact, pin, pout, 1)) < 0)
			break;
		if (rv >= 0 && (t1 = t->left))
			rv = execute(t1, pin, pout, 0);
		undo_redir();
		break;
	}

broken:
	t->words = wp2;
	isbreak = 0;
	freehere(areanum);
	freearea(areanum);
	areanum = a;
	if (talking && intr) {
		closeall();
		fail();
	}
	if ((i = trapset) != 0) {
		trapset = 0;
		runtrap(i);
	}
	return(rv);
}

static int
forkexec(t, pin, pout, act, wp, pforked)
register struct op *t;
int *pin, *pout;
int act;
char **wp;
int *pforked;
{
	int i, rv, (*shcom)();
	int doexec();
	register int f;
	char *cp;
	struct ioword **iopp;
	int resetsig;
	char **owp;
	int pid;

	owp = wp;
	resetsig = 0;
	*pforked = 0;
	shcom = NULL;
	rv = -1;	/* system-detected error */
	if (t->type == TCOM) {
		while ((cp = *wp++) != NULL)
			;
		cp = *wp;

		/* strip all initial assignments */
		/* not correct wrt PATH=yyy command  etc */
		if (flag['x'])
			echo (cp ? wp: owp);
		if (cp == NULL && t->ioact == NULL) {
			while ((cp = *owp++) != NULL && assign(cp, COPYV))
				;
			return(setstatus(0));
		}
		else if (cp != NULL)
			shcom = inbuilt(cp);
	}
	t->words = wp;
	f = act;

	if (shcom == NULL && (f & FEXEC) == 0) {
#ifdef AMOEBA
		/* Check whether there is a temporary assignment in the cmd: */
		int has_temp_assign = 0;
		if (owp != NULL && owp[0] != NULL && letter(*owp[0])) {
			char *cp;
			for (cp = owp[0]; *cp && letnum(*cp); cp++)
				/* nothing */ ;
			has_temp_assign = (*cp == '=');
		}
		if (!has_temp_assign && t->type == TCOM) {
			static int fdlist[NOFILE];
			if (fdlist[2] != 2)  /* First time: */
				for (i = 0; i < e.iofd; i++)
					fdlist[i] = i;

			if (wp[0] == NULL)	/* The ":" command: */
				return(setstatus(0));
			if (pin != NULL) {
				fdlist[0] = pin[0];
				fcntl(fdlist[0], F_SETFD, 0);
			} else
				fdlist[0] = 0;
			if (pout != NULL) {
				fdlist[1] = pout[1];
				fcntl(fdlist[1], F_SETFD, 0);
			} else
				fdlist[1] = 1;

			pid = 0;
			if ((iopp = t->ioact) != NULL)
				/* Do the I/O redirection into fdlist, without
				 * actually affecting the real user fds: */
				while (*iopp) {
					if ((i = iosetup(*iopp,
						    pin!=NULL, pout!=NULL)) < 0)
						pid = i;
					else if (i >= e.iofd) {
						fdlist[(*iopp)->io_unit] = i;
						/* Turn off close-on-exec, so
						 * newproc will let the child
						 * inherit the fd:
						 */
						fcntl(i, F_SETFD, 0);
					}
					iopp++;
				}	
			if (pid == 0) {
				pid = rnewproc(wp[0], wp, makenv(),
							  e.iofd, fdlist, -1L);
			}
			if (pin != NULL) {
				fdlist[0] = 0;
				closepipe(pin);
			}
			/* Close the redirected fds: */
			for (i = 0; i < e.iofd; i++)
				if (fdlist[i] != i) {
					close(fdlist[i]);
					fdlist[i] = i;
				}
			if (pid < 0)
				return(pid);
			return(pout==NULL? setstatus(waitfor(pid, 0)): 0);
		}
#endif /* AMOEBA */
		i = parent();
		if (i != 0) {
			if (pin != NULL)
				closepipe(pin);
			if (i == -1)
				return(rv);
			return(pout==NULL? setstatus(waitfor(i, 0)): 0);
		}
		if (talking) {
			signal(SIGINT, SIG_IGN);
			signal(SIGQUIT, SIG_IGN);
			resetsig = 1;
		}
		talking = 0;
		intr = 0;
		(*pforked)++;
		brklist = 0;
		execflg = 0;
	}
	if (owp != NULL)
		while ((cp = *owp++) != NULL && assign(cp, COPYV))
			if (shcom == NULL)
				export(lookup(cp));
	if ((rv = ioredir(t->ioact, pin, pout, (shcom != NULL))) < 0)
		return(rv);
	if (shcom) {
		rv = setstatus((*shcom)(t));
		undo_redir();
		return rv;
	}
	/* We've arranged in remap that all the high fds are close-on-exec:
	 * for (i=FDBASE; i<NOFILE; i++)
	 *	close(i);
	 */
	if (resetsig) {
		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
	}
	if (t->type == TPAREN)
		exit(execute(t->left, NOPIPE, NOPIPE, FEXEC));
	if (wp[0] == NULL)
		exit(0);
	cp = rexecve(wp[0], wp, makenv());
	prs(wp[0]); prs(": "); warn(cp);
	if (!execflg)
		trap[0] = NULL;
	leave();
	/* NOTREACHED */
}

/*
 * common actions when creating a new child
 */
static int
parent()
{
	register int i;

	i = fork();
	if (i != 0) {
		if (i == -1)
			warn("try again");
	}
	return(i);
}

/*
 * 0< 1> are ignored as required
 * within pipelines.
 *
 * Changed by condict -- 1/90
 *	Altered so that it returns a remap'd fd that is connected as specified
 *	by the iop, but without altering the association of the fd
 *      iop->io_unit.  Thus, no actual redirection is
 *	performed, but the caller gets the fd it needs to do the redirection.
 *	In case of failure, no new fd is opened and -1
 *	is returned.  If no user fd was affected, but neither was any error
 *	detected, 0 is returned, which is distinguishable from a remap'd fd.
 *	(The function used to return 0 for success, 1 for failure,
 *	so this is a change in interface.)
 */
iosetup(iop, pipein, pipeout)
register struct ioword *iop;
int pipein, pipeout;
{
	register u;
	char *cp, *msg;

	if (iop->io_unit == IODEFAULT)	/* take default */
		iop->io_unit = iop->io_flag&(IOREAD|IOHERE)? 0: 1;
	if (pipein && iop->io_unit == 0)
		return(0);
	if (pipeout && iop->io_unit == 1)
		return(0);
	msg = iop->io_flag&(IOREAD|IOHERE)? "open": "create";
	if ((iop->io_flag & IOHERE) == 0) {
		cp = iop->io_name;
		if ((cp = evalstr(cp, DOSUB|DOTRIM)) == NULL)
			return(-1);
	}
	if (iop->io_flag & IODUP) {
		if (cp[1] || !digit(*cp) && *cp != '-') {
			prs(cp);
			err(": illegal >& argument");
			return(-1);
		}
		if (*cp == '-')
			iop->io_flag = IOCLOSE;
		iop->io_flag &= ~(IOREAD|IOWRITE);
	}
	switch (iop->io_flag) {
	case IOREAD:
		u = open(cp, 0);
		break;

	case IOHERE:
	case IOHERE|IOXHERE:
		u = herein(iop->io_name, iop->io_flag&IOXHERE);
		cp = "here file";
		break;

	case IOWRITE|IOCAT:
		if ((u = open(cp, 1)) >= 0) {
			lseek(u, (long)0, 2);
			break;
		}
	case IOWRITE:
		u = creat(cp, 0666);
		break;

	case IODUP:
		u = dup(*cp-'0');
		break;

	case IOCLOSE:
		/* Caller had better close before exec.  Can't do it here. */
		return(0);

	default:
		u = -1;		/* Shouldn't happen */
		msg = "redirect -- internal err";
	}
	if (u < 0) {
		prs(cp);
		prs(": cannot ");
		warn(msg);
		return(-1);
	}
	return remap(u);
}

static int saved_fdlist[NOFILE];	/* For restoring original condition
					 * of fds after I/O redirection.
					 * Postion i contains a dup'd fd that
					 * is the same as fd i before redir,
					 * or -1 if fd i was not altered by
					 * redir, or 0 if fd i was not open
					 * before redir.
					 */

/* Use after dup2, to copy the state of the close-on-exec bit from
 * the oldfd to the newfd.
 */
int
dup2_cexcl(oldfd, newfd)
int oldfd, newfd;
{
		int is_close_on_exec = fcntl(oldfd, F_GETFD, 0);
		if (is_close_on_exec >= 0)
			fcntl(newfd, F_SETFD, is_close_on_exec);
		return is_close_on_exec;
}

static int
save_fd(fd)
int fd;
{
	int is_close_on_exec = fcntl(fd, F_GETFD, 0);
	if (is_close_on_exec >= 0) {
		saved_fdlist[fd] = remap(fd);
		if (saved_fdlist[fd] < 0)	/* Something went wrong,
					 	 * close fd on restore: */
			saved_fdlist[fd] = 0;
		else		/* Remap returns an fd set to close-on-exec,
				 * so restore original value: */
			fcntl(saved_fdlist[fd], F_SETFD, is_close_on_exec);
	} else {
		/* Fd is not open: save a 0 to indicate this: */
		saved_fdlist[fd] = 0;
	}
}

static int
ioredir(iopp, pin, pout, save_orig)
struct ioword **iopp;
int *pin, *pout;
int save_orig;
{
	register struct ioword *iop;
	int fd;
	for (fd = 0; fd < e.iofd; fd++)
		saved_fdlist[fd] = -1;
	if (pin != NULL) {
		if (save_orig)
			save_fd(0);
		dup2(pin[0], 0);
		close(pin[0]);
	}
	if (pout != NULL) {
		if (save_orig)
			save_fd(1);
		dup2(pout[1], 1);
		close(pout[1]);
	}
	if (iopp) {
		while ((iop = *iopp) != NULL) {
			fd = iosetup(iop, pin!=NULL, pout!=NULL);
			if (fd < 0) {
				if (save_orig)
					undo_redir();
				return fd;
			} else if (fd == 0) {
				if (iop->io_flag == IOCLOSE) {
					if (save_orig)
						save_fd(iop->io_unit);
					close(iop->io_unit);
				}
			} else if (fd >= e.iofd) {
				if (save_orig)
					save_fd(iop->io_unit);
				dup2(fd, iop->io_unit);
				close(fd);
			} else
				abort();	/* Shouldn't happen */
			iopp++;
		}
	}
	return 0;
}

static int
undo_redir()
{
	int fd, saved_fd;
	for (fd = 0; fd < e.iofd; fd++) {
		saved_fd = saved_fdlist[fd];
		if (saved_fd >= e.iofd) {
			dup2(saved_fd, fd);
			dup2_cexcl(saved_fd, fd);
			close(saved_fd);
		} else if (saved_fd == 0) {
			close(fd);
		} else if (saved_fd > 0) {
			err("(internal) bad saved fd"); /* Shouldn't happen */
		}
		saved_fdlist[fd] = -1;
	}
}

static void
echo(wp)
register char **wp;
{
	register i;

	prs("+");
	for (i=0; wp[i]; i++) {
		if (i)
			prs(" ");
		prs(wp[i]);
	}
	prs("\n");
}

static struct op **
find1case(t, w)
struct op *t;
char *w;
{
	register struct op *t1;
	struct op **tp;
	register char **wp, *cp;

	if (t == NULL)
		return(NULL);
	if (t->type == TLIST) {
		if ((tp = find1case(t->left, w)) != NULL)
			return(tp);
		t1 = t->right;	/* TPAT */
	} else
		t1 = t;
	for (wp = t1->words; *wp;)
		if ((cp = evalstr(*wp++, DOSUB)) && gmatch(w, cp))
			return(&t1->left);
	return(NULL);
}

static struct op *
findcase(t, w)
struct op *t;
char *w;
{
	register struct op **tp;

	return((tp = find1case(t, w)) != NULL? *tp: NULL);
}

/*
 * Enter a new loop level (marked for break/continue).
 */
static void
brkset(bc)
struct brkcon *bc;
{
	bc->nextlev = brklist;
	brklist = bc;
}

/*
 * Wait for the last process created.
 * Print a message for each process found
 * that was killed by a signal.
 * Ignore interrupt signals while waiting
 * unless `canintr' is true.
 */
int
waitfor(lastpid, canintr)
register int lastpid;
int canintr;
{
	register int pid, rv;
	int s;
	int oheedint = heedint;

	heedint = 0;
	rv = 0;
	do {
		pid = wait(&s);
		if (pid == -1) {
#ifndef SIG_HANDLER_BUG_FIXED
			/* Sometimes interrupt handler doesn't get called: */
			if (errno == EINTR && !intr)
				onintr(0);
#endif
			if (errno != EINTR || canintr)
				break;
		} else {
			if ((rv = WAITSIG(s)) != 0) {
				if (rv < NSIGNAL) {
					if (signame[rv] != NULL) {
						if (pid != lastpid) {
							prn(pid);
							prs(": ");
						}
						prs(signame[rv]);
					}
				} else {
					if (pid != lastpid) {
						prn(pid);
						prs(": ");
					}
					prs("Signal "); prn(rv); prs(" ");
				}
				if (WAITCORE(s))
					prs(" - core dumped");
				if (rv >= NSIGNAL || signame[rv])
					prs("\n");
				rv = -1;
			} else
				rv = WAITVAL(s);
		}
	} while (pid != lastpid);
	heedint = oheedint;
	if (intr)
		if (talking) {
			if (canintr)
				intr = 0;
		}
		else
			onintr(0);
	return(rv);
}

int
setstatus(s)
register int s;
{
	exstat = s;
	setval(lookup("?"), putn(s));
	return(s);
}

/*
 * PATH-searching interface to execve.
 * If getenv("PATH") were kept up-to-date,
 * execvp might be used.
 */
char *
rexecve(c, v, envp)
char *c, **v, **envp;
{
	register char *sp, *tp;

	sp = any('/', c)? "": path->value;
	while (sp != NULL) {
		sp = path_first(sp, c, e.linep);
		execve(e.linep, v, envp);
		switch (errno) {
		case ENOEXEC:
			*v = e.linep;
			tp = *--v;	/* Save current value of v[-1] */
			*v = e.linep;	/* Set it to pathname of program */
			execve(shell->value, v, envp);
			*v++ = tp;	/* Restore old value of v[-1] */
			return("no Shell");

		case ENOMEM:
			return("program too big");

		case E2BIG:
			return("argument list too long");

		default:
			if (errno != ENOENT)
				return("unknown error in process startup");
			break;
		}
	}
	return(errno==ENOENT ? "not found" : "cannot execute");
}


#ifdef AMOEBA
/*
 * PATH-searching interface to newproc (similar to rexecve, above).
 */
int
rnewproc(c, v, envp, nfd, fdlist, sigignore)
char *c, **v, **envp;
int nfd;
int fdlist[];
long sigignore;
{
	register int rtn;
	register char *sp, *tp;

	sp = any('/', c)? "": path->value;
	while (sp != NULL) {
		sp = path_first(sp, c, e.linep);


		rtn = newproc(e.linep, v, envp, nfd, fdlist, sigignore);


		if (rtn < 0 && errno == ENOEXEC) {
			*v = e.linep;
			tp = *--v;	/* Save current value of v[-1] */
			*v = e.linep;	/* Set it to pathname of program */
			rtn = newproc(shell->value, v, envp, nfd, fdlist, sigignore);
			*v++ = tp;	/* Restore old value of v[-1] */
			if (rtn < 0) {
				if (errno == ENOENT) {
					prs(v[0]); prs(": "); err("no Shell");
					return rtn;
				}
			}
		}
		if (rtn >= 0) return rtn;

		if (errno != ENOENT) {
			prs(v[0]); prs(": ");
			if (errno < 0 || errno >= sys_nerr)
				err("unknown error");
			else
				err(sys_errlist[errno]);
			return rtn;
		}
	}
	prs(v[0]); prs(": "); err("not found");
	return -1;
}
#endif

/*
 * Run the command produced by generator `f'
 * applied to stream `arg'.
 */
run(argp, f)
struct ioarg *argp;
int (*f)();
{
	struct op *otree;
	struct wdblock *swdlist;
	struct wdblock *siolist;
	jmp_buf ev, rt;
	xint *ofail;
	int rv;

	areanum++;
	swdlist = wdlist;
	siolist = iolist;
	otree = outtree;
	ofail = failpt;
	rv = -1;
	if (newenv(setjmp(JMPCAST (errpt = (xint *) ev))) == 0) {
		wdlist = 0;
		iolist = 0;
		pushio(argp, f);
		e.iobase = e.iop;
		yynerrs = 0;
		if (setjmp(JMPCAST (failpt = (xint *) rt)) == 0
		    && yyparse() == 0)
			rv = execute(outtree, NOPIPE, NOPIPE, 0);
		quitenv();
	}
	wdlist = swdlist;
	iolist = siolist;
	failpt = ofail;
	outtree = otree;
	freearea(areanum--);
	return(rv);
}

/* -------- do.c -------- */
/* #include "sh.h" */

/*
 * built-in commands: doX
 */

static	void	rdexp();
static	void	badid();
static	int	brkcontin();

dolabel()
{
	return(0);
}

dochdir(t)
register struct op *t;
{
	register char *cp, *er;

	if ((cp = t->words[1]) == NULL && (cp = homedir->value) == NULL)
		er = ": no home directory";
	else if(chdir(cp) < 0)
		er = ": bad directory";
	else {
#ifdef AMOEBA
		char *cwd = getenv("_WORK");
		if (cwd)
			setval(workdir, cwd);
		else
			setval(workdir, "/");
#endif
		return(0);
	}
	prs(cp != NULL? cp: "cd");
	err(er);
	return(1);
}

doshift(t)
register struct op *t;
{
	register n;

	n = t->words[1]? getn(t->words[1]): 1;
	if(dolc < n) {
		err("nothing to shift");
		return(1);
	}
	dolv[n] = dolv[0];
	dolv += n;
	dolc -= n;
	setval(lookup("#"), putn(dolc));
	return(0);
}

/*
 * execute login and newgrp directly
 */
dologin(t)
struct op *t;
{
	register char *cp;

	if (talking) {
		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
	}
	cp = rexecve(t->words[0], t->words, makenv());
	prs(t->words[0]); prs(": "); err(cp);
	return(1);
}

doumask(t)
register struct op *t;
{
	register int i, n;
	register char *cp;

	if ((cp = t->words[1]) == NULL) {
		i = umask(0);
		umask(i);
		for (n=3*4; (n-=3) >= 0;)
			putc('0'+((i>>n)&07));
		putc('\n');
	} else {
		for (n=0; *cp>='0' && *cp<='9'; cp++)
			n = n*8 + (*cp-'0');
		umask(n);
	}
	return(0);
}

doexec(t)
register struct op *t;
{
	register i;
	jmp_buf ex;
	xint *ofail;

	t->ioact = NULL;
	for(i = 0; (t->words[i]=t->words[i+1]) != NULL; i++)
		;
	if (i == 0)
		return(1);
	execflg = 1;
	ofail = failpt;
	if (setjmp(JMPCAST (failpt = (xint *) ex)) == 0)
		execute(t, NOPIPE, NOPIPE, FEXEC);
	failpt = ofail;
	execflg = 0;
	return(1);
}

dodot(t)
struct op *t;
{
	register i;
	register char *sp;
	char *cp;

	if ((cp = t->words[1]) == NULL)
		return(0);
	sp = any('/', cp)? "": path->value;
	while (sp != NULL) {
		sp = path_first(sp, cp, e.linep);
		if ((i = open(e.linep, 0)) >= 0) {
			exstat = 0;
			next(remap(i));
			return(exstat);
		}
	}
	prs(cp);
	err(": not found");
	return(-1);
}

dowait(t)
struct op *t;
{
	register i;
	register char *cp;

	if ((cp = t->words[1]) != NULL) {
		i = getn(cp);
		if (i == 0)
			return(0);
	} else
		i = -1;
	setstatus(waitfor(i, 1));
	return(0);
}

doecho(t)
struct op *t;
{
	register char **wp;
	int nflag;
	char buf[256], *bp;	/* Buffer chars for efficiency */
	bp = buf; *bp = '\0';	/* Indicate empty buffer */

	nflag = 0;
	if (t->words[0] != NULL && t->words[1] != NULL) {
		wp = t->words + 1;
		if (wp[0][0] == '-' && wp[0][1] == 'n' && !wp[0][2]) {
			nflag++;
			wp++;
		}
		for ( ; *wp; wp++) {
			int len = strlen(*wp);
			/* len + 2 allows room for ' ' and '\0' terminator */
			if (bp + len + 2 >= buf + sizeof buf) {
				write(1, buf, bp - buf);
				bp = buf;
				*bp = '\0';
			}
			if (len + 2 >= sizeof buf) {
				/* Too big even for an empty buf: */
				write(1, *wp, len);
			} else {
				strcpy(bp, *wp);
				bp += len;	/* Invariant: *bp == '\0'; */
			}
			if (*(wp + 1))
				*bp++ = ' ', *bp = '\0';
		}
		if (bp > buf)
			write(1, buf, bp - buf);
	}
	if (nflag == 0) write(1, "\n", 1);
	return 0;
}

doread(t)
struct op *t;
{
	register char *cp, **wp;
	register nb;

	if (t->words[1] == NULL) {
		err("Usage: read name ...");
		return(1);
	}
	for (wp = t->words+1; *wp; wp++) {
		for (cp = e.linep; cp < elinep-1; cp++)
			if ((nb = read(0, cp, sizeof(*cp))) != sizeof(*cp) ||
			    *cp == '\n' ||
			    wp[1] && any(*cp, ifs->value))
				break;
		*cp = 0;
		if (nb <= 0)
			break;
		setval(lookup(*wp), e.linep);
	}
	return(nb <= 0);
}

doeval(t)
register struct op *t;
{
	int wdchar();

	return(RUN(awordlist, t->words+1, wdchar));
}

dotrap(t)
register struct op *t;
{
	register int  n, i;
	register int  resetsig;

	if (t->words[1] == NULL) {
		for (i=0; i<NSIG; i++)
			if (trap[i]) {
				prn(i);
				prs(": ");
				prs(trap[i]);
				prs("\n");
			}
		return(0);
	}
	resetsig = digit(*t->words[1]);
	for (i = resetsig ? 1 : 2; t->words[i] != NULL; ++i) {
		n = getsig(t->words[i]);
		xfree(trap[n]);
		trap[n] = 0;
		if (!resetsig) {
			if (*t->words[1] != '\0') {
				trap[n] = strsave(t->words[1], 0);
				setsig(n, sig);
			} else
				setsig(n, SIG_IGN);
		} else {
			if (talking)
				if (n == SIGINT)
					setsig(n, onintr);
				else
					setsig(n, n == SIGQUIT ? SIG_IGN 
							       : SIG_DFL);
			else
				setsig(n, SIG_DFL);
		}
	}
	return(0);
}

getsig(s)
char *s;
{
	register int n;

	if ((n = getn(s)) < 0 || n >= NSIG) {
		err("trap: bad signal number");
		n = 0;
	}
	return(n);
}

setsig(n, f)
register n;
void (*f)();
{
	if (n == 0)
		return;
	if (signal(n, SIG_IGN) != SIG_IGN || ourtrap[n]) {
		ourtrap[n] = 1;
		signal(n, f);
	}
}

getn(as)
char *as;
{
	register char *s;
	register n, m;

	s = as;
	m = 1;
	if (*s == '-') {
		m = -1;
		s++;
	}
	for (n = 0; digit(*s); s++)
		n = (n*10) + (*s-'0');
	if (*s) {
		prs(as);
		err(": bad number");
	}
	return(n*m);
}

dobreak(t)
struct op *t;
{
	return(brkcontin(t->words[1], 1));
}

docontinue(t)
struct op *t;
{
	return(brkcontin(t->words[1], 0));
}

static int
brkcontin(cp, val)
register char *cp;
{
	register struct brkcon *bc;
	register nl;

	nl = cp == NULL? 1: getn(cp);
	if (nl <= 0)
		nl = 999;
	do {
		if ((bc = brklist) == NULL)
			break;
		brklist = bc->nextlev;
	} while (--nl);
	if (nl) {
		err("bad break/continue level");
		return(1);
	}
	isbreak = val;
	longjmp(bc->brkpt, 1);
	/* NOTREACHED */
}

doexit(t)
struct op *t;
{
	register char *cp;

	execflg = 0;
	if ((cp = t->words[1]) != NULL)
		exstat = getn(cp);
	leave();
}

doexport(t)
struct op *t;
{
	rdexp(t->words+1, export, EXPORT);
	return(0);
}

doreadonly(t)
struct op *t;
{
	rdexp(t->words+1, ronly, RONLY);
	return(0);
}

static void
rdexp(wp, f, key)
register char **wp;
void (*f)();
int key;
{
	if (*wp != NULL) {
		for (; *wp != NULL; wp++)
			if (checkname(*wp))
				(*f)(lookup(*wp));
			else
				badid(*wp);
	} else
		putvlist(key, 1);
}

static void
badid(s)
register char *s;
{
	prs(s);
	err(": bad identifier");
}

doset(t)
register struct op *t;
{
	register struct var *vp;
	register char *cp;
	register n;

	if ((cp = t->words[1]) == NULL) {
		for (vp = vlist; vp; vp = vp->next)
			varput(vp->name, 1);
		return(0);
	}
	if (*cp == '-') {
		/* bad: t->words++; */
		for(n = 0; (t->words[n]=t->words[n+1]) != NULL; n++)
			;
		if (*++cp == 0)
			flag['x'] = flag['v'] = 0;
		else
			for (; *cp; cp++)
				switch (*cp) {
				case 'e':
					if (!talking)
						flag['e']++;
					break;

				default:
					if (*cp>='a' && *cp<='z')
						flag[*cp]++;
					break;
				}
		setdash();
	}
	if (t->words[1]) {
		t->words[0] = dolv[0];
		for (n=1; t->words[n]; n++)
			setarea((char *)t->words[n], 0);
		dolc = n-1;
		dolv = t->words;
		setval(lookup("#"), putn(dolc));
		setarea((char *)(dolv-1), 0);
	}
	return(0);
}

varput(s, out)
register char *s;
{
	if (letnum(*s)) {
		write(out, s, strlen(s));
		write(out, "\n", 1);
	}
}

#include <sys/types.h>
#include <sys/times.h>

#define	SECS	60L
#define	MINS	3600L

dotimes()
{
	struct tms tbuf;

	times(&tbuf);

	prn((int)(tbuf.tms_cutime / MINS));
	prs("m");
	prn((int)((tbuf.tms_cutime % MINS) / SECS));
	prs("s ");
	prn((int)(tbuf.tms_cstime / MINS));
	prs("m");
	prn((int)((tbuf.tms_cstime % MINS) / SECS));
	prs("s\n");
}

struct	builtin {
	char	*command;
	int	(*fn)();
};
static struct	builtin	builtin[] = {
	":",		dolabel,
	"cd",		dochdir,
	"shift",	doshift,
	"exec",		doexec,
	"wait",		dowait,
	"echo",		doecho,
	"read",		doread,
	"eval",		doeval,
	"trap",		dotrap,
	"break",	dobreak,
	"continue",	docontinue,
	"exit",		doexit,
	"export",	doexport,
	"readonly",	doreadonly,
	"set",		doset,
	".",		dodot,
	"umask",	doumask,
	"login",	dologin,
	"newgrp",	dologin,
	"times",	dotimes,
	0,
};

int (*inbuilt(s))()
register char *s;
{
	register struct builtin *bp;

	for (bp = builtin; bp->command != NULL; bp++)
		if (strcmp(bp->command, s) == 0)
			return(bp->fn);
	return(NULL);
}

