/*	@(#)sh1.c	1.3	96/03/04 14:06:22 */
/*
** Minix shell, sh1.c
**
** Initial version by ??????, ?? ??? ????
**
** CHANGE LOG:
** -----------
**
** 14 Nov 1989, peterb@cwi.nl, sh1.c
** New memory allocation. Instead of using brk and sbrk, memory management is
** done using malloc and free. The new memory allocation can be used by 
** compiling this module with the define MALLOC.
**
** 14 Nov 1989, peterb@cwi.nl, sh5.c
** "\" processing updated. PS2 was not prompted when using \'s in commands.
** Fixed in the routine getc()
**
** 28 Nov 1989, condict@cs.vu.nl, sh1.c
** Fixed isassign so it doesn't crash on non-Vaxen when given NULL pointer.
**
** 29 Nov 1989, condict@cs.vu.nl, sh4.c
** Changed it to use POSIX-compatible <dirent.h>, instead of <sys/dir.h>
**
*/
#ifdef AMOEBA
/*
** 17 Dec 1989, guido@cwi.nl, sh.h, sh1.c, sh3.c
** Hack to make pwd work; see cwd_set.c, getcwd.c.  (This should really
** be done by the Ajax library but I can't figure out how.)
*/
#endif
/*
** 15 Jan 1990, condict@cs.vu.nl, sh3.c
** Enhanced forkexec() to allow I/O redirection and pipelines in built-in
** commands, and in shell statements (e.g., while, for).  (See ioredir().)
*/
#ifdef AMOEBA
/*
** 15 Jan 1990, condict@cs.vu.nl, sh3.c
** Enhanced forkexec() to use newproc instead of fork/exec, wherever possible.
** The only simple commands that still use fork/exec are ones with temporary
** environment variable assignments prefixed (e.g. x=1 date).  Use of
** "( cmd-list )" and "{ cmd-list }&" still cause fork, of course.
*/
#endif
/*
** 21 Jan 1990, condict@cs.vu.nl, sh5.c
** Optimized remap() to keep all fds < FDBASE open, whether or not in use, so
** that a single dup returns a high fd (instead of 7 dup's/ 6 closes).
**
** 25 Jan 1990, condict@cs.vu.nl, sh3.c
** Added echo as a new built-in command.  (See doecho().)
*/

#define Extern extern
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <setjmp.h>
#include <stdlib.h>
#include "sh.h"
/* -------- sh.c -------- */
/*
 * shell
 */
/* #include "sh.h" */

int	intr;
int	inparse;
char	flags['z'-'a'+1];
char	*flag = flags-'a';
char	*elinep = line+sizeof(line)-5;
char	*null	= "";
int	heedint =1;
struct	env	e ={line, iostack, iostack-1, NULL, FDBASE, NULL};

extern	char	**environ;	/* environment pointer */

/*
 * default shell, search rules
 */
char	shellname[] = "/bin/sh";
char	search[] = ":/bin:/usr/bin";

void	(*qflag)() = SIG_IGN;

main(argc, argv)
int argc;
register char **argv;
{
	register int f;
	register char *s;
	int cflag;
	char *name, **ap;
	int (*iof)();

	initarea();
	if ((ap = environ) != NULL) {
		while (*ap)
			assign(*ap++, !COPYV);
		for (ap = environ; *ap;)
			export(lookup(*ap++));
	}
	closeall();
	areanum = 1;

	shell = lookup("SHELL");
	if (shell->value == null)
		setval(shell, shellname);
	export(shell);

	homedir = lookup("HOME");
	if (homedir->value == null)
		setval(homedir, "/");
	export(homedir);

#ifdef AMOEBA
	workdir = lookup("_WORK");
	if (workdir->value == null)
		setval(workdir, "/");
	export(workdir);
#endif

	setval(lookup("$"), itoa(getpid(), 1));

	path = lookup("PATH");
	if (path->value == null)
		setval(path, search);
	export(path);

	ifs = lookup("IFS");
	if (ifs->value == null)
		setval(ifs, " \t\n");

	prompt = lookup("PS1");
	if (prompt->value == null)
#ifndef UNIXSHELL
		setval(prompt, "$ ");
#else
		setval(prompt, "% ");
#endif
	if (geteuid() == 0) {
		setval(prompt, "# ");
		prompt->status &= ~EXPORT;
	}
	cprompt = lookup("PS2");
	if (cprompt->value == null)
		setval(cprompt, "> ");

	iof = filechar;
	cflag = 0;
	name = *argv++;
	if (--argc >= 1) {
		if(argv[0][0] == '-' && argv[0][1] != '\0') {
			for (s = argv[0]+1; *s; s++)
				switch (*s) {
				case 'c':
					prompt->status &= ~EXPORT;
					cprompt->status &= ~EXPORT;
					setval(prompt, "");
					setval(cprompt, "");
					cflag = 1;
					if (--argc > 0)
						PUSHIO(aword, *++argv, iof = nlchar);
					break;
	
				case 'q':
					qflag = SIG_DFL;
					break;

				case 's':
					/* standard input */
					break;

				case 't':
					prompt->status &= ~EXPORT;
					setval(prompt, "");
					iof = linechar;
					break;
	
				case 'i':
					talking++;
				default:
					if (*s>='a' && *s<='z')
						flag[*s]++;
				}
		} else {
			argv--;
			argc++;
		}
		if (iof == filechar && --argc > 0) {
			setval(prompt, "");
			setval(cprompt, "");
			prompt->status &= ~EXPORT;
			cprompt->status &= ~EXPORT;
			if (newfile(name = *++argv))
				exit(1);
		}
	}
	setdash();
	if (e.iop < iostack) {
		PUSHIO(afile, 0, iof);
		if (isatty(0) && isatty(1) && !cflag)
			talking++;
	}
	signal(SIGQUIT, qflag);
	if (name && name[0] == '-') {
		talking++;
		if ((f = open(".profile", 0)) >= 0)
			next(remap(f));
		if ((f = open("/etc/profile", 0)) >= 0)
			next(remap(f));
	}
	if (talking)
		signal(SIGTERM, sig);
	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		signal(SIGINT, onintr);
	dolv = argv;
	dolc = argc;
	dolv[0] = name;
	if (dolc > 1)
		for (ap = ++argv; --argc > 0;)
			if (assign(*ap = *argv++, !COPYV))
				dolc--;	/* keyword */
			else
				ap++;
	setval(lookup("#"), putn((--dolc < 0) ? (dolc = 0) : dolc));

	for (;;) {
		if (talking && e.iop <= iostack)
			prs(prompt->value);
		onecommand();
	}
}

setdash()
{
	register char *cp, c;
	char m['z'-'a'+1];

	cp = m;
	for (c='a'; c<='z'; c++)
		if (flag[c])
			*cp++ = c;
	*cp = 0;
	setval(lookup("-"), m);
}

newfile(s)
register char *s;
{
	register f;

	if (strcmp(s, "-") != 0) {
		f = open(s, 0);
		if (f < 0) {
			prs(s);
			err(": cannot open");
			return(1);
		}
	} else
		f = 0;
	next(remap(f));
	return(0);
}

onecommand()
{
	register i;
	jmp_buf m1;

	while (e.oenv)
		quitenv();
	areanum = 1;
	freehere(areanum);
	freearea(areanum);
	garbage();
	wdlist = 0;
	iolist = 0;
	e.errpt = 0;
	e.linep = line;
	yynerrs = 0;
	multiline = 0;
	inparse = 1;
	intr = 0;
	execflg = 0;
	setjmp(JMPCAST (failpt = (xint *) m1));  /* Bruce Evans' fix */
	if (setjmp(JMPCAST (failpt = (xint *) m1)) || yyparse() || intr) {
		while (e.oenv)
			quitenv();
		scraphere();
		if (!talking && intr)
			leave();
		inparse = 0;
		intr = 0;
		return;
	}
	inparse = 0;
	brklist = 0;
	intr = 0;
	execflg = 0;
	if (!flag['n'])
		execute(outtree, NOPIPE, NOPIPE, 0);
	if (!talking && intr) {
		execflg = 0;
		leave();
	}
	if ((i = trapset) != 0) {
		trapset = 0;
		runtrap(i);
	}
}

void
fail()
{
	longjmp(JMPCAST failpt, 1);
	/* NOTREACHED */
}

void
leave()
{
	if (execflg)
		fail();
	scraphere();
	freehere(1);
	runtrap(0);
	exit(exstat);
	/* NOTREACHED */
}

warn(s)
register char *s;
{
	if(*s) {
		prs(s);
		exstat = -1;
	}
	prs("\n");
	if (flag['e'])
		leave();
}

err(s)
char *s;
{
	warn(s);
	if (flag['n'])
		return;
	if (!talking)
		leave();
	if (e.errpt)
		longjmp(JMPCAST e.errpt, 1);
	closeall();
	e.iop = e.iobase = iostack;
}

newenv(f)
{
	register struct env *ep;

	if (f) {
		quitenv();
		return(1);
	}
	ep = (struct env *) space(sizeof(*ep));
	if (ep == NULL) {
		while (e.oenv)
			quitenv();
		fail();
	}
	*ep = e;
	e.oenv = ep;
	e.errpt = errpt;
	return(0);
}

quitenv()
{
	register struct env *ep;
	register fd;

	if ((ep = e.oenv) != NULL) {
		fd = e.iofd;
		e = *ep;
		/* should close `'d files */
		DELETE(ep);
		while (--fd >= e.iofd)
			close(fd);
	}
}

/*
 * Is any character from s1 in s2?
 */
int
anys(s1, s2)
register char *s1, *s2;
{
	while (*s1)
		if (any(*s1++, s2))
			return(1);
	return(0);
}

/*
 * Is character c in s?
 */
int
any(c, s)
register int c;
register char *s;
{
	while (*s)
		if (*s++ == c)
			return(1);
	return(0);
}

char *
putn(n)
register n;
{
	return(itoa(n, -1));
}

char *
itoa(u, n)
register int u;
{
	register char *cp;
	static char s[20];
	int m;

	m = 0;
	if (n < 0 && (int) u < 0) {
		m++;
		u = -u;
	}
	cp = s+sizeof(s);
	*--cp = 0;
	do {
		*--cp = u%10 + '0';
		u /= 10;
	} while (--n > 0 || u);
	if (m)
		*--cp = '-';
	return(cp);
}

next(f)
{
	PUSHIO(afile, f, filechar);
}

/*ARGSUSED*/
void
onintr(sig)
int sig;
{
#ifdef DEBUG
	prs("onintr: talking="); prn(talking);
	prs(", inparse="); prn(inparse);
	prs(", heedint="); prn(heedint);
	prs("\n");
#endif
	signal(SIGINT, onintr);
	intr = 1;
	if (talking) {
		if (inparse) {
			prs("\n");
			fail();
		}
	}
	else if (heedint) {
		execflg = 0;
		leave();
	}
}

letter(c)
register c;
{
	return(c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c == '_');
}

digit(c)
register c;
{
	return(c >= '0' && c <= '9');
}

letnum(c)
register c;
{
	return(letter(c) || digit(c));
}

char *
space(n)
int n;
{
	register char *cp;

	if ((cp = getcell((unsigned) n)) == 0)
		err("out of string space");
	return(cp);
}

char *
strsave(s, a)
register char *s;
{
	register char *cp, *xp;

	if ((cp = space(strlen(s)+1)) != NULL) {
		setarea((char *)cp, a);
		for (xp = cp; (*xp++ = *s++) != '\0';)
			;
		return(cp);
	}
	return("");
}

xfree(s)
register char *s;
{
	DELETE(s);
}

/*
 * trap handling
 */
void
sig(i)
register i;
{
	trapset = i;
	signal(i, sig);
}

runtrap(i)
{
	char *trapstr;

	if ((trapstr = trap[i]) == NULL)
		return;
	if (i == 0)
		trap[i] = 0;
	RUN(aword, trapstr, nlchar);
}

/* -------- var.c -------- */
/* #include "sh.h" */

static	char	*findeq();

/*
 * Find the given name in the dictionary
 * and return its value.  If the name was
 * not previously there, enter it now and
 * return a null value.
 */
struct var *
lookup(n)
register char *n;
{
	register struct var *vp;
	register char *cp;
	register int c;
	static struct var dummy;

	if (digit(*n)) {
		dummy.name = n;
		for (c = 0; digit(*n) && c < 1000; n++)
			c = c*10 + *n-'0';
		dummy.status = RONLY;
		dummy.value = c <= dolc? dolv[c]: null;
		return(&dummy);
	}
	for (vp = vlist; vp; vp = vp->next)
		if (eqname(vp->name, n))
			return(vp);
	cp = findeq(n);
	vp = (struct var *)space(sizeof(*vp));
	if (vp == 0 || (vp->name = space((int)(cp-n)+2)) == 0) {
		dummy.name = dummy.value = "";
		return(&dummy);
	}
	for (cp = vp->name; (*cp = *n++) && *cp != '='; cp++)
		;
	if (*cp == 0)
		*cp = '=';
	*++cp = 0;
	setarea((char *)vp, 0);
	setarea((char *)vp->name, 0);
	vp->value = null;
	vp->next = vlist;
	vp->status = GETCELL;
	vlist = vp;
	return(vp);
}

/*
 * give variable at `vp' the value `val'.
 */
void
setval(vp, val)
struct var *vp;
char *val;
{
	nameval(vp, val, (char *)NULL);
}

/*
 * if name is not NULL, it must be
 * a prefix of the space `val',
 * and end with `='.
 * this is all so that exporting
 * values is reasonably painless.
 */
void
nameval(vp, val, name)
register struct var *vp;
char *val, *name;
{
	register char *cp, *xp;
	char *nv;
	int fl;

	if (vp->status & RONLY) {
		for (xp = vp->name; *xp && *xp != '=';)
			putc(*xp++);
		err(" is read-only");
		return;
	}
	fl = 0;
	if (name == NULL) {
		xp = space(strlen(vp->name)+strlen(val)+2);
		if (xp == 0)
			return;
		/* make string:  name=value */
		setarea((char *)xp, 0);
		name = xp;
		for (cp = vp->name; (*xp = *cp++) && *xp!='='; xp++)
			;
		if (*xp++ == 0)
			xp[-1] = '=';
		nv = xp;
		for (cp = val; (*xp++ = *cp++) != '\0';)
			;
		val = nv;
		fl = GETCELL;
	}
	if (vp->status & GETCELL)
		xfree(vp->name);	/* form new string `name=value' */
	vp->name = name;
	vp->value = val;
	vp->status |= fl;
}

void
export(vp)
struct var *vp;
{
	vp->status |= EXPORT;
}

void
ronly(vp)
struct var *vp;
{
	if (letter(vp->name[0]))	/* not an internal symbol ($# etc) */
		vp->status |= RONLY;
}

/* Is sometimes passed a NULL pointer! */
int
isassign(s)
register char *s;
{
	if (!s || !letter(*s))
		return(0);
	for (; *s != '='; s++)
		if (*s == 0 || !letnum(*s))
			return(0);
	return(1);
}

int
assign(s, cf)
register char *s;
int cf;
{
	register char *cp;
	struct var *vp;

	if (!letter(*s))
		return(0);
	for (cp = s; *cp != '='; cp++)
		if (*cp == 0 || !letnum(*cp))
			return(0);
	vp = lookup(s);
	nameval(vp, ++cp, cf == COPYV? NULL: s);
	if (cf != COPYV)
		vp->status &= ~GETCELL;
	return(1);
}

int
checkname(cp)
register char *cp;
{
	if (!letter(*cp++))
		return(0);
	while (*cp)
		if (!letnum(*cp++))
			return(0);
	return(1);
}

void
putvlist(f, out)
register int f, out;
{
	register struct var *vp;

	for (vp = vlist; vp; vp = vp->next)
		if (vp->status & f && letter(*vp->name)) {
			if (vp->status & EXPORT)
				write(out, "export ", 7);
			if (vp->status & RONLY)
				write(out, "readonly ", 9);
			write(out, vp->name, (int)(findeq(vp->name) - vp->name));
			write(out, "\n", 1);
		}
}

int
eqname(n1, n2)
register char *n1, *n2;
{
	for (; *n1 != '=' && *n1 != 0; n1++)
		if (*n2++ != *n1)
			return(0);
	return(*n2 == 0 || *n2 == '=');
}

static char *
findeq(cp)
register char *cp;
{
	while (*cp != '\0' && *cp != '=')
		cp++;
	return(cp);
}

/* -------- gmatch.c -------- */
/*
 * int gmatch(string, pattern)
 * char *string, *pattern;
 *
 * Match a pattern as in sh(1).
 */

/* #define	NULL	0 */
#define	CMASK	0377
#define	QUOTE	0200
#define	QMASK	(CMASK&~QUOTE)
#define	NOT	'!'	/* might use ^ */

static	char	*cclass();

int
gmatch(s, p)
register char *s, *p;
{
	register int sc, pc;

	if (s == NULL || p == NULL)
		return(0);
	while ((pc = *p++ & CMASK) != '\0') {
		sc = *s++ & QMASK;
		switch (pc) {
		case '[':
			if ((p = cclass(p, sc)) == NULL)
				return(0);
			break;

		case '?':
			if (sc == 0)
				return(0);
			break;

		case '*':
			s--;
			do {
				if (*p == '\0' || gmatch(s, p))
					return(1);
			} while (*s++ != '\0');
			return(0);

		default:
			if (sc != (pc&~QUOTE))
				return(0);
		}
	}
	return(*s == 0);
}

static char *
cclass(p, sub)
register char *p;
register int sub;
{
	register int c, d, not, found;

	if ((not = *p == NOT) != 0)
		p++;
	found = not;
	do {
		if (*p == '\0')
			return(NULL);
		c = *p & CMASK;
		if (p[1] == '-' && p[2] != ']') {
			d = p[2] & CMASK;
			p++;
		} else
			d = c;
		if (c == sub || c <= sub && sub <= d)
			found = !not;
	} while (*++p != ']');
	return(found? p+1: NULL);
}

#ifndef MALLOC
/* -------- area.c -------- */
#define	REGSIZE		sizeof(struct region)
#define GROWBY		256
#undef	SHRINKBY	64
#define FREE 32767
#define BUSY 0
#define	ALIGN (sizeof(int)-1)

/* #include "area.h" */
#define	NULL	0

struct region {
	struct	region *next;
	int	area;
};

/*
 * All memory between (char *)areabot and (char *)(areatop+1) is
 * exclusively administered by the area management routines.
 * It is assumed that sbrk() and brk() manipulate the high end.
 */
static	struct region *areabot;		/* bottom of area */
static	struct region *areatop;		/* top of area */
static	struct region *areanxt;		/* starting point of scan */
char	*sbrk();
char	*brk();

initarea()
{
	while ((int)sbrk(0) & ALIGN)
		sbrk(1);
	areabot = (struct region *)sbrk(REGSIZE);
	areabot->next = areabot;
	areabot->area = BUSY;
	areatop = areabot;
	areanxt = areabot;
}

char *
getcell(nbytes)
unsigned nbytes;
{
	register int nregio;
	register struct region *p, *q;
	register i;

	if (nbytes == 0)
		abort();	/* silly and defeats the algorithm */
	/*
	 * round upwards and add administration area
	 */
	nregio = (nbytes+(REGSIZE-1))/REGSIZE + 1;
	for (p = areanxt;;) {
		if (p->area > areanum) {

			/*
			 * merge free cells
			 */
			while ((q = p->next)->area > areanum && q != areanxt)
				p->next = q->next;
			/*
			 * exit loop if cell big enough
			 */
			if (q >= p + nregio)
				goto found;
		}
		p = p->next;
		if (p == areanxt)
			break;
	}
	i = nregio >= GROWBY ? nregio : GROWBY;
	p = (struct region *)sbrk(i * REGSIZE);
	if ((int)p == -1)
		return(NULL);
	p--;
	if (p != areatop)
		abort();	/* allocated areas are contiguous */
	q = p + i;
	p->next = q;
	p->area = FREE;
	q->next = areabot;
	q->area = BUSY;
	areatop = q;
found:
	/*
	 * we found a FREE area big enough, pointed to by 'p', and up to 'q'
	 */
	areanxt = p + nregio;
	if (areanxt < q) {
		/*
		 * split into requested area and rest
		 */
		if (areanxt+1 > q)
			abort();	/* insufficient space left for admin */
		areanxt->next = q;
		areanxt->area = FREE;
		p->next = areanxt;
	}
	p->area = areanum;
	return((char *)(p+1));
}

void
freecell(cp)
char *cp;
{
	register struct region *p;

	if ((p = (struct region *)cp) != NULL) {
		p--;
		if (p < areanxt)
			areanxt = p;
		p->area = FREE;
	}
}

void
freearea(a)
register int a;
{
	register struct region *p, *top;

	top = areatop;
	for (p = areabot; p != top; p = p->next)
		if (p->area >= a)
			p->area = FREE;
}

void
setarea(cp,a)
char *cp;
int a;
{
	register struct region *p;

	if ((p = (struct region *)cp) != NULL)
		(p-1)->area = a;
}

int
getarea(cp)
char *cp;
{
	return ((struct region*)cp-1)->area;
}

void
garbage()
{
	register struct region *p, *q, *top;

	top = areatop;
	for (p = areabot; p != top; p = p->next) {
		if (p->area > areanum) {
			while ((q = p->next)->area > areanum)
				p->next = q->next;
			areanxt = p;
		}
	}
#ifdef SHRINKBY
	if (areatop >= q + SHRINKBY && q->area > areanum) {
		brk((char *)(q+1));
		q->next = areabot;
		q->area = BUSY;
		areatop = q;
	}
#endif
}
#else /* !MALLOC */

/*
** New memory allocater for the minix shell.
** The new memory allocater uses malloc and free instead of
** doing its own memeory management by sbrk. The new memory
** allocater has not made the assumption of the availability
** of contiguous memory.
**
** 061189, peterb@cwi.nl
*/

/*
** A cell data structure holds pointers to the next and the previous
** cell. Following the cell data structure comes the actual cell....
**
*/
typedef struct CellTypeX {
   struct CellTypeX *Next;		/* Next cell for this area */
   struct CellTypeX *Prev;		/* Previous cell for this area */
} CellType;

/*
** An area structure holds a cell data structure which points to the
** first cell for this area; holds a next pointer for the next area;
** and holds an area number....
**
*/
typedef struct AreaTypeX {
   CellType	     Cell;		/* Cell structure... */
   struct AreaTypeX *Next;		/* Next area */
   int		     Area;		/* Area number */
					/* Since area number do not have
					** to be contiguous in the list, 
					** the areanumber must be saved in the
					** area structure..
					*/
} AreaType;

/*
** AreaFirst and AreaLast are used to build an area list....
**
*/
static AreaType *AreaFirst = NULL;
static AreaType *AreaLast  = NULL;
static AreaType *FindArea();

initarea()
{
   /*
   ** For compatibility...
   **
   */
}

/*
** Getcell first finds an area, and adds the cell to the found area...
**
*/
char *
getcell(nbytes)
unsigned nbytes;
{
   char *AddCell();

   if (nbytes == 0)
      abort();


   /*
   ** Now, add a cell to the area list...
   **
   */
   return(AddCell(FindArea(areanum), nbytes));
}

/*
** freecell, free a cell...
**
*/
void
freecell(cp)
char *cp;
{
    register CellType *pCell;

    if (cp != NULL) {
	/*
	** Unlink element...
	*/
        pCell = (CellType *)cp - 1;
	pCell->Prev->Next = pCell->Next;
	if (pCell->Next != NULL) 
	    pCell->Next->Prev = pCell->Prev;

	free(pCell);
     }
}

/*
** freearea...
** Free the cells of an area, but leave the area alive and kicking
**
*/
void
freearea(area)
register int area;
{
   register AreaType *pArea = AreaFirst;
   register CellType *pCell, *pNextCell;

   while (pArea) {

      /*
      ** Found one?
      **
      */
      if (pArea->Area >= area) {

	 /*
	 ** Free the cells...
	 **
	 */
	 pCell = pArea->Cell.Next;
	 while (pCell) {
	    pNextCell = pCell->Next;
	    free(pCell);
	    pCell = pNextCell;
	 }

         pArea->Cell.Next = NULL;	
      }
      pArea = pArea->Next;
   }    
}

/*
** setarea, move a cell to another area...
**
*/
void
setarea(cp, a)
char *cp;
int  a;
{
   register CellType *pCell = (CellType *)cp - 1;
   register AreaType *pArea;
   AreaType *FindArea();

   if (cp == NULL)
      abort();

   /*
   ** Dirty, dirty...
   ** When previous is an area structure, the first four
   ** bytes represent a pointer to a cell list. When previous
   ** is a cell structure, the first four bytes represent the pointer
   ** to the next cell element...
   **
   */
   pCell->Prev->Next = pCell->Next;
   if (pCell->Next != NULL) 
      pCell->Next->Prev = pCell->Prev;

   /*
   ** Find new area...
   **
   */
   pArea = FindArea(a);

   /*
   ** Link cell in cell list for an area...
   **
   */
   pCell->Prev = (CellType *) pArea;
   pCell->Next = pArea->Cell.Next;
   if (pArea->Cell.Next != NULL)
      (pArea->Cell.Next)->Prev = pCell;
   pArea->Cell.Next = pCell;
}


/*
** getarea, get an area from a cell pointer...
**
*/
int
getarea(cp)
char *cp;
{
   register CellType *pCell = (CellType *)cp - 1;

   if (cp == NULL)
      abort();

   /*
   ** Now find area...
   ** Last element is an area...
   **
   */
   while (pCell->Prev != NULL)
      pCell = pCell->Prev;

   return(((AreaType *)pCell)->Area);
}

/*
** Garbage, smourk...
**
*/
void
garbage()
{}

/*
** AddCell, allocate a cell and add it to the cell list...
**
*/
char *
AddCell(p, nbytes)
register AreaType *p;
unsigned nbytes;
{
   register CellType *cp;

   /*
   ** Allocate memory for cell data structure and data bytes...
   **
   */
   cp = (CellType *) malloc(nbytes + sizeof(CellType));
   if (cp == NULL)
      abort();

   /*
   ** Link cell in cell list for an area...
   **
   */
   cp->Prev = (CellType *) p;
   cp->Next = p->Cell.Next;
   if (p->Cell.Next != NULL)
      (p->Cell.Next)->Prev = cp;
   p->Cell.Next = cp;
   return((char *)(cp + 1));
}

/*
** Find the area pointer (or allocate a new area)....
**
*/
static AreaType *
FindArea(area)
int area;
{
   register AreaType *pArea = AreaFirst;

   /*
   ** Find area list...
   **
   */
   while (pArea && pArea->Area != area)
      pArea = pArea->Next;

   if (pArea == NULL) {
      /*
      ** Area not yet in AreaFirst...
      **
      */
      pArea = (AreaType *) malloc(sizeof(AreaType));
      if (pArea == NULL)
	 abort();

      if (AreaLast != NULL)
	 AreaLast->Next = pArea;
      else
	 AreaFirst      = pArea;
      AreaLast         = pArea;

      pArea->Next      = NULL;
      pArea->Area      = area;
      pArea->Cell.Next = NULL;
      pArea->Cell.Prev = NULL;

   }
   return(pArea);
}

#ifdef DEBUG

DumpArea()
{
   register AreaType *pArea = AreaFirst;
   register CellType *pCell;

   FooBie("AreaFirst 0x");
   FooBieLong(AreaFirst);
   FooBie("\n");

   while (pArea) {
      FooBie("\n");
      FooBieLong(pArea);
      FooBie(" Area ");
      FooBieLong(pArea->Area);
      FooBie(" Next ");
      FooBieLong(pArea->Next);
      FooBie(" CellList ");
      FooBieLong(pArea->Cell.Next);
      FooBie("\n");
#ifdef notdef
      pCell = pArea->Cell.Next;
      while (pCell) {
	 FooBieLong(pCell);
	 FooBie(" Next: ");
	 FooBieLong(pCell->Next);
	 FooBie(" Prev: ");
	 FooBieLong(pCell->Prev);
	 FooBie(" ");
	 pCell = pCell->Next;
      }
      FooBie("\n");
#endif /* notdef */
      pArea = pArea->Next;
   }
}

FooBieLong(LongVal)
   unsigned long LongVal;
{
   static char *Hex = "0123456789ABCDEF";
   char str[9];

   str[0] = Hex[((LongVal >> 28) & 0xF)];
   str[1] = Hex[((LongVal >> 24) & 0xF)];
   str[2] = Hex[((LongVal >> 20) & 0xF)];
   str[3] = Hex[((LongVal >> 16) & 0xF)];
   str[4] = Hex[((LongVal >> 12) & 0xF)];
   str[5] = Hex[((LongVal >> 8) & 0xF)];
   str[6] = Hex[((LongVal >> 4) & 0xF)];
   str[7] = Hex[((LongVal) & 0xF)];
   str[8] = '\0';
   FooBie(str);
}

FooBie(str)
   char *str;
{
   write(2, str, strlen(str));
}

#endif /* DEBUG */
#endif /* ! MALLOC */
