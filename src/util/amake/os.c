/*	@(#)os.c	1.6	96/02/27 13:06:34 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include "systypes.h"
#include <sys/stat.h>
#include "standlib.h"
#include "unistand.h"

#if defined(ACK) && !defined(AMOEBA) && !defined(_POSIX_SOURCE)
/* Oh boy. With the SunOs 4.1 headers we unwillingly include conflicting
 * typedefs. The following typedefs & defines are only needed to avoid
 * this clash on Unix when compiling with Ack.
 * Note: this is a hack, but I see no other way out.
 */
#define __sys_stdtypes_h		/* fake that we included it already */
typedef int             pid_t;          /* process id */
typedef unsigned short  mode_t;         /* file mode bits */
typedef short           nlink_t;        /* links to a file */
#define _TIME_T
typedef long            time_t;         /* value = secs since epoch */
#define _SIZE_T
typedef unsigned int	size_t;
#endif /* defined(ACK) && !defined(AMOEBA) && !defined(_POSIX_SOURCE) */

#include "global.h"
#include "alloc.h"
#include "idf.h"
#include "error.h"
#include "expr.h"
#include "scope.h"
#include "objects.h"
#include "dbug.h"
#include "statefile.h"
#include "buffer.h"
#include "docmd.h"
#include "main.h"
#include "caching.h"
#include "slist.h"
#include "type.h"
#include "conversion.h"
#include "builtin.h"
#include "os.h"

extern int errno;

#if defined(__STDC__) || defined(AMOEBA)
#include <string.h>
#define ErrString(errno)	strerror(errno)
#else
/* nonportable, but what else? */
extern char *sys_errlist[];
#define ErrString(errno)	sys_errlist[errno]
#endif /* __STDC__ || AMOEBA */

#ifdef __STDC__
PUBLIC volatile int Interrupted = FALSE;
#else
PUBLIC int Interrupted = FALSE;	/* set TRUE when user types <DEL> */
#endif

PUBLIC struct expr *NoValueId;	/* value id for non existing file */

#ifdef amunix /* currently only Amoeba and Unix(like) systems are supported */

#if defined(_POSIX_SOURCE) || defined (_POSIX_C_SOURCE) || defined(_MINIX)
#define HAS_WAITPID
#else
#if !defined(V7) && !defined(SYSV)
/* Only define HAS_WAIT3 for Unices with nonblocking wait(), like BSD wait3 */
#define HAS_WAIT3
#endif
#endif

#include <signal.h>
#include <sys/wait.h>

#ifdef AMOEBA

#include <amoeba.h>
#include <stderr.h>
#include <module/name.h>
#include <module/prv.h>

/*
** Follwing function is derived from ar_cap()
** (the size of this representation is somewhat smaller)
*/

static char pbuf[15 + 2*8 + 2*2*PORTSIZE];

PRIVATE struct expr *
CapabilityToString(cap)
capability *cap;
{
    (void) sprintf(pbuf,
		":%02x%02x%02x%02x%02x%02x:%ld:%02x:%02x%02x%02x%02x%02x%02x",
                cap->cap_port._portbytes[0] & 0xFF,
                cap->cap_port._portbytes[1] & 0xFF,
                cap->cap_port._portbytes[2] & 0xFF,
                cap->cap_port._portbytes[3] & 0xFF,
                cap->cap_port._portbytes[4] & 0xFF,
                cap->cap_port._portbytes[5] & 0xFF,
                prv_number(&cap->cap_priv),
                cap->cap_priv.prv_rights & 0xFF,
                cap->cap_priv.prv_random._portbytes[0] & 0xFF,
                cap->cap_priv.prv_random._portbytes[1] & 0xFF,
                cap->cap_priv.prv_random._portbytes[2] & 0xFF,
                cap->cap_priv.prv_random._portbytes[3] & 0xFF,
                cap->cap_priv.prv_random._portbytes[4] & 0xFF,
                cap->cap_priv.prv_random._portbytes[5] & 0xFF);
    return(StringExpr(pbuf));
}

PUBLIC struct expr *
GetValueId(obj)
struct object *obj;
/*
 * Get capability and convert it to an Amake expression.
 * To reduce the number of `name_lookup' calls, the caller should take care
 * to cache the result (as attribute of `obj') whenever possible.
 * For convenience capabilities are currently transformed in printable
 * strings. This might change someday.
 */
{
    capability cap;

    if (name_lookup(SystemName(obj), &cap) == STD_OK) {
	return(CapabilityToString(&cap));
    } else {
	return(NoValueId); /* file doesn't exist */
    }
}

#else

PUBLIC struct expr *
GetValueId(obj)
struct object *obj;
/*
 * Get last-file-change-time and convert it to an Amake expression.
 * To reduce the number of `stat' system calls, the caller should take care
 * to cache the result (as attribute of `obj') whenever possible.
 */
{
    struct stat buf;

    if (stat(SystemName(obj), &buf) == 0) {
	PutAttrValue(obj, Id_size, IntExpr((long)buf.st_size), GLOBAL_SCOPE);
#ifdef STR_ID
	{
	    char str[20];

	    (void) sprintf(str, ":%lx", (long)buf.st_mtime);
	    return(StringExpr(str));
	}
#else
	return(IntExpr((long)buf.st_mtime));
#endif
    } else {
	return(NoValueId); /* file doesn't exist */
    }
}
#endif

PUBLIC long
GetFileSize(name)
char *name;
{
    struct stat buf;
    long size;

    DBUG_PRINT("size", ("get size of `%s'", name));
    if (stat(name, &buf) == 0) {
	size = buf.st_size;
    } else {
	size = -1;
    }
    return(size);
}

PUBLIC long
GetObjSize(obj)
struct object *obj;
{
    struct expr *size_expr;
    long size;

    if ((size_expr = GetAttrValue(obj, Id_size)) != UnKnown) {
	size = size_expr->e_integer;
    } else {
	size = GetFileSize(SystemName(obj));
	PutAttrValue(obj, Id_size, IntExpr(size), GLOBAL_SCOPE);
	/* we might Put the Id_id as well */
    }
    return(size);
}

PUBLIC int
DoLink(existing_path, new_path)
char *existing_path, *new_path;
{
    int err;

    DBUG_PRINT("file", ("link(\"%s\", \"%s\")", existing_path, new_path));
    if (err = (link(existing_path, new_path) != 0)) {
	if (errno == EEXIST) {
	    /* When Amake has been aborted in previous runs, it may happen
	     * that directory .Amake contains objects that Amake doesn't
	     * know of.
	     */
	    Verbose(2, "`%s' unexpected, will be removed", new_path);
	    DoDelete(new_path, FATAL);
	    err = (link(existing_path, new_path) != 0);
	}
    }

    return(err != 0);
}

/*
 * Process startup & wait
 */

PUBLIC int
BlockingWait(statusp)
int *statusp;
{
    return(wait(statusp));
}

#if defined(HAS_WAITPID) || defined(HAS_WAIT3) || defined(SYSV)

#ifndef HAS_WAITPID
#include <sys/time.h>
#include <sys/resource.h>
#endif

/* System 5 #ifdefs by Jack Jansen */

PUBLIC int
NonBlockingWait(statusp)
int *statusp;
{
#ifdef HAS_WAITPID
    int pid = waitpid(-1, statusp, WNOHANG);
#else
#ifdef SYSV
    int pid = nbwait(statusp);
#else
    int pid = wait3(statusp, WNOHANG, (struct rusage *)NULL);
#endif
#endif

    if (pid > 0) {
	return(pid);
    } else {
	if (pid < 0) { /* pid == 0 means that there is no child ready */
	    Warning("Non blocking wait: %s", ErrString(errno));
	}
	return(-1);
    }
}

#else /* HAS_WAITPID || HAS_WAIT3 || SYSV */

/*
 * If there is no non-blocking wait present, we have some performance loss.
 * A non blocking wait is done when a new command has been built up.
 * Normally command building is a lot faster than the commands started up,
 * so we prefer not to do a blocking wait.
 * Alternatives are also possible: after the following (fake) NonBlockingWait
 * is called, say, 10 times, we could perform a BlockingWait(). Or we could
 * use a timer together with the expected run-time of commands now running.
 */

PUBLIC int
NonBlockingWait(statusp)
int *statusp;
{
    return(-1);
}

#endif /* HAS_WAITPID || HAS_WAIT3 || SYSV */

PRIVATE char *
BuildCommandString(proc)
struct process *proc;
{
    extern char **Environment;

#ifdef AMOEBA
    return(proc->p_cmd);
#else
# ifdef USE_RSH
    if (F_use_rsh) {
	extern char *WorkDir();
	char *work;

	if ((work = WorkDir()) == NULL) {
	    Warning("couldn't find working directory, so no remote execution");
	    F_use_rsh = FALSE;
	    RenameProcessors(proc);
	    /* FALLTHROUGH */
	} else {
	    /* We now assume that the remote machine has exactly the same
	     * directory structure. We should use a part of erikb's pool
	     * exec module here.
	     */
	    BufferStart();
	    BufferAddString("cd ");
	    BufferAddString(work);
	    /* use && instead of ';' to avoid loosing a bad exit status */
	    BufferAddString("&& ");
	    BufferAddString(proc->p_cmd);
	    BufferAddString(proc->p_redir);
	    BufferAddString("&& sync");
	    return(BufferResult());
	}
    }
# endif
    BufferStart();
    BufferAddString(proc->p_cmd);
    BufferAddString(proc->p_redir);
    return(BufferResult());
#endif
}

#define BOURNE_SHELL "/bin/sh"

#ifdef AMOEBA

#define NR_FDS 3 	/* stdin, stdout and stderr */
#define MAX_ARGS 1000	/* temporary upper bound */

PRIVATE FILE *RedirFiles[NR_FDS];
PRIVATE int NrRedirs;

PRIVATE void
Redirect(obj, mode)
struct object *obj;
char *mode;
{
    FILE *fp;
    char *name = SystemName(obj);

    if ((fp = fopen(name, mode)) == NULL) {
	/* invalid bullet capability? */
	if (*mode == 'w' && errno == EIO && name_delete(name) == STD_OK) {
	    fp = fopen(name, mode);
	}
    }

    if (fp != NULL) {
	RedirFiles[NrRedirs++] = fp;
    } else {
	Warning("error while opening `%s' for %s: %s", SystemName(obj),
		(*mode == 'r') ? "reading" : "writing",
		ErrString(errno));
    }
}

PRIVATE void
UnRedirect()
{
    while (--NrRedirs >= 0) {
	/* don't close Amake's own in & outputs */
	if (fileno(RedirFiles[NrRedirs]) > 2) {
	    fclose(RedirFiles[NrRedirs]);
	}
    }
}

PRIVATE char *ArgVec[MAX_ARGS];
PRIVATE int NrArgs;
PRIVATE char PathBuf[128];

PRIVATE int
BuildArgs(arg_list)
struct slist *arg_list;
{
    NrArgs = 1;
    ArgVec[0] = ArgVec[1] = NULL;

    /* put the args in ArgVec[2] onward, to keep room to give the command
     * as argument to the shell, when it's a shellscript
     */

    ITERATE(arg_list, expr, arg, {
	register char *s;

	if (arg->e_type == T_OBJECT) {
	    s = CmdObjectName(arg->e_object);
	} else {
	    Item(cons) = (generic)GetExprOfType(arg, T_STRING);
	    s = do_get_string(ItemP(cons, expr));
	}
	if (NrArgs < MAX_ARGS) {
	    ArgVec[++NrArgs] = Salloc(s, (unsigned)(strlen(s) + 1));
	} else {
	    return(FALSE);
	}
    });

    ArgVec[NrArgs+1] = NULL;
    return(TRUE);
}

PRIVATE void
FreeArgs()
{
    while (NrArgs > 1) {
	free(ArgVec[NrArgs--]);
    }
}


PRIVATE char *
DoEscape(str)
char *str;
{
    register char *p;

    BufferStart();
    for (p = str; *p != 0; p++) {
	if (strchr(SHELL_METAS, *p) != 0) { /* must be escaped */
	    BufferAdd('\\');
	}
	BufferAdd(*p);
    }
    return(BufferResult());
}

PRIVATE void
EscapeArgs(argv)
char *argv[];
/* the arguments will be provided to the shell. Quote them when needed. */
{
    register char **arg, *p;

    for (arg = argv; *arg != NULL; arg++) {
	for (p = *arg; *p != 0; p++) {
	    /* if one of the characters is a meta character,
	     * convert the entire arg, else don't alter the arg.
	     */
	    if (strchr(SHELL_METAS, *p) != 0) { /* must be escaped */
		*arg = DoEscape(p);
		free(p);
		break;
	    }
	}
    }
}

/* The next three functions, dealing with Amoeba command-lookup are copied
 * from the shell source code. The function newprocp couldn't be used, because
 * it doesn't take the possibility of shell scripts into account. Should it?
 */

PRIVATE int
any(c, s)
register int c;
register char *s;
{
    while (*s)
	if (*s++ == c)
	    return(1);
    return(0);
}

PRIVATE char *
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

PRIVATE int
rnewproc(c, v, envp, nfd, fdlist, sigignore)
char *c, **v, **envp;
int nfd;
int fdlist[];
long sigignore;
{
    register int rtn;
    register char *sp, *tp;

    sp = any('/', c)? "": getenv("PATH");
    while (sp != NULL) {
	sp = path_first(sp, c, PathBuf);

	rtn = newproc(PathBuf, v, envp, nfd, fdlist, sigignore);

	if (rtn < 0 && errno == ENOEXEC) {
	    *v = PathBuf;
	    tp = *--v;      /* Save current value of v[-1] */
	    *v = PathBuf;   /* Set it to pathname of program */
	    EscapeArgs(v + 1);
	    rtn = newproc(BOURNE_SHELL, v, envp, nfd, fdlist, sigignore);
	    *v++ = tp;      /* Restore old value of v[-1] */
	    if (rtn < 0) {
		if (errno == ENOENT) {
		    FatalError("%s: no Shell", v[0]);
                }
	    }
	}
	if (rtn >= 0) return rtn;

	if (errno != ENOENT) {
	    PrintError("amake: %s: %s\n", v[0], ErrString(errno));
	    return rtn;
	}
    }
    PrintError("amake: %s: not found\n", v[0]);
    return FORK_FAILURE;
}

PUBLIC int
ForkOff(proc)
struct process *proc;
/* Amoeba has a much faster alternative for fork()/exec() */
{
    extern char **Environment;
    int fdlist[NR_FDS], pid, i;
    struct expr *prog, *args, *standin, *standout, *standerr;

    get_exec_args(proc->p_args, &prog, &args, &standin, &standout, &standerr);

    /* Let Amake open the redirection files itself */
    NrRedirs = 0;
    if (standin->e_object != FooObject) { /* don't redirect by default */
	Redirect(standin->e_object, "r");
    } else {
	RedirFiles[NrRedirs++] = stdin;
    }

    Redirect(standout->e_object, "w");
    Redirect(standerr->e_object, "w");

    pid = -1;
    if (NrRedirs == NR_FDS) {
	for (i = 0; i < NrRedirs; i++) {
	    fdlist[i] = fileno(RedirFiles[i]);
	}

	/* for newproc we need an arglist rather than an argstring */
	if (BuildArgs(args->e_list)) {
	    ArgVec[1] = SystemName(prog->e_object);

	    pid = rnewproc(ArgVec[1], &ArgVec[1], Environment,
			       NrRedirs, fdlist, 0L);
	}
	FreeArgs();
    }

    if (pid < 0) pid = FORK_FAILURE;
    /* don't retry by default, because it is currently impossible to tell
     * the difference between temporary and "real" problems (like the arglist
     * being too big).
     */

    UnRedirect();
    return(pid);
}

#else

/*ARGSUSED*/
PRIVATE void
DoShell(proc, command)
struct process *proc;
char *command;
{
    extern char **Environment;
    char *shell;
    extern char **environ;

#ifdef USE_RSH
    if (F_use_rsh) {
	shell = "/usr/ucb/rsh";
	execle(shell, shell, proc->p_cpu, command, (char *)NULL, Environment);
    } else
#endif
    {
	shell = BOURNE_SHELL;
#ifdef USE_POOL
	p_exece(command, Environment);
#else
	execle(shell, shell, "-c", command, (char *)NULL, Environment);
#endif
    }

    Warning("couldn't load %s (%s)", shell, ErrString(errno));
    _exit(1);
}

#if defined(SYSV) || defined(V7) || defined(_MINIX)
#define vfork fork
#endif

PUBLIC int
ForkOff(proc)
struct process *proc;
{
    int npid;
    char *command;

#ifdef USE_POOL
    p_xinit();
#endif
#ifdef SYSV
    waitinit();
#endif
    command = BuildCommandString(proc);

    switch (npid = vfork()) { /* a lot better than fork()! */
    case -1:	    /* fork failed (not enough mem or too many procs) */
	return(-1);
    case 0:	    /* this is the child who has to start up cmd */
	DoShell(proc, command);
	/*NOTREACHED*/
	break;
    default:	    /* this is the parent */
	break;
    }
    free(command);
    return(npid);
}

#endif

/*
 * Signal handling: set boolean Interrupted when a SIGINT is received.
 * A premature exit is performed a moment later, when amake is ready for it.
 * At the moment of the interrupt itself, amake may be doing anything
 * (updating datastructures etc.), so in general it is not safe to
 * do something more sophisticating than printing a message (assuming
 * the print library used is re-entrant) and setting a flag.
 */

PUBLIC void
PrematureExit()
{
    DumpStatefile();
    if (!F_keep_intermediates) {
	RemoveIntermediates(); /* don't keep them, even on <DEL> */
    }
    exit(EXIT_FAILURE);
}

/*ARGSUSED*/
PRIVATE void
UserSignal(sig)
int sig;
{
    /* show that we got it; it requires an interruptable printf */
    Message("Ouch..\n");
    Interrupted = TRUE;
}

PUBLIC void
IgnoreUserSignals()
{
    (void) signal(SIGINT, SIG_IGN);
}

PRIVATE void
CatchSignals()
{
    if (signal(SIGINT, SIG_IGN) != SIG_IGN) {
	/* running on foreground */
	(void) signal(SIGINT, UserSignal);
    }
}

PUBLIC void
InitOS()
{
    CatchSignals();
#ifdef STR_ID
    NoValueId = StringExpr(NO_VALUE_ID);
#else
    NoValueId = IntExpr((long)NO_VALUE_ID);
#endif
}

#endif /* amunix */

PUBLIC int
Writable(obj)
struct object *obj;
{
    char *name = SystemName(obj);

    if (access(name, W_OK) != 0) {
	if (access(name, F_OK) != 0) { /* doesn't exist, check directory */
	    if (access(SystemName(obj->ob_parent), W_OK) != 0) {
		return(FALSE);
	    }
	} else { /* not writeable, although it does exist */
	    return(FALSE);
	}
    }
    return(TRUE);
}

PUBLIC void
DoDelete(filename, fatal)
char *filename;
enum fatal_or_not fatal;
{
    DBUG_PRINT("file", ("delete(\"%s\")", filename));
    if (unlink(filename) != 0) {
	if (fatal == FATAL) {
	    FatalError("couldn't remove `%s'", filename);
	} else {
	    Verbose(2, "couldn't remove `%s'", filename);
	}
    }
}

#define MAX_BYTES_COMPARE 30000 /* only compare reasonably (?) small files */

#if !defined(__STDC__) && !defined(AMOEBA)
#define memcmp(s1, s2, n)	bcmp(s2, s1, n)
#endif

PUBLIC int
FilesEqual(new, old)
struct object *new, *old;
/* Check if a generated file differs from an old version */
{
    long size1 = GetObjSize(new), size2 = GetObjSize(old);

    if (size1 == size2 && size1 != -1 && size1 < MAX_BYTES_COMPARE) {
	FILE *fp1 = NULL, *fp2 = NULL;
	int equal;
	char *buf1 = Malloc((unsigned)size1), *buf2 = Malloc((unsigned)size2);

	if ((fp1 = fopen(SystemName(new), "r")) == NULL ||
	    (fp2 = fopen(SystemName(old), "r")) == NULL ||
	    fread(buf1, 1, (int) size1, fp1) != size1 ||
	    fread(buf2, 1, (int) size2, fp2) != size2) {
	    Warning("read error while comparing `%s' with cached version",
		    SystemName(new));
	    equal = FALSE;
	} else {
	    equal = memcmp(buf1, buf2, size1) == 0;
	}
	free(buf1); free(buf2);
	if (fp1 != NULL) fclose(fp1);
	if (fp2 != NULL) fclose(fp2);
	return(equal);
    } else {
	return(FALSE);
    }
}	

PRIVATE struct slist *PrivateDirs = NULL;

#if defined(USE_POOL) && !defined(DELAY_TIME)
#define DELAY_TIME	1
#endif

#ifdef _S_IFMT /* posix style macros */
# define FILE_TYPE	_S_IFMT
# define DIRECTORY	_S_IFDIR
#else
#ifdef _IFMT	/* sun style macros? */
# define FILE_TYPE	_IFMT
# define DIRECTORY	_IFDIR
#else /* assume BSD-style macros */
# define FILE_TYPE	S_IFMT
# define DIRECTORY	S_IFDIR
#endif
#endif

PUBLIC void
MakePrivateDir(dir)
struct object *dir;
{
    if ((dir->ob_flags & O_CREATED) == 0) {
	char *name = SystemName(dir);
	struct stat buf;

	/* maybe it was created in a previous run */
	if (stat(name, &buf) != 0) {
	    if (errno != ENOENT || mkdir(name, 0777) != 0) {
		FatalError("cannot create `%s': %s", name, ErrString(errno));
	    } else {
#ifdef USE_POOL
		sleep(DELAY_TIME); /* wait until NFS has `exported' it */
#endif
	    }
	} else if ((buf.st_mode & FILE_TYPE) != DIRECTORY) {
	    FatalError("%s is not a directory", name);
	}
	Append(&PrivateDirs, dir);
	dir->ob_flags |= O_CREATED;
    }
}

PUBLIC void
CleanupPrivateDirs()
/* Try to remove the directories created with MakeAmakeDir, in reverse order.
 * Some of them might be non-empty, but then the rmdir() just fails, which
 * is just what we want.
 */
{
    struct cons *cons;

    for (cons = Tail(PrivateDirs); cons != NULL; cons = Prev(cons)) {
	rmdir(SystemName(ItemP(cons, object)));
    }
}

#ifdef SYSV
/* a few routines not (yet?) available in system 5 */

PUBLIC int
rename(fo, fn)
    char *fo, *fn;
{
    int rv;

    if ((rv=link(fo, fn)) < 0) return rv;
    if ((rv=unlink(fo)) < 0) {
        (void)unlink(fn);
        return rv;
    }
    return 0;
}

PRIVATE int
mkdir(name, mode)
    char *name;
#ifdef __STDC__
    mode_t mode;
#endif
{
    char buffer[256];

    (void)sprintf(buffer, "mkdir %s", name);
    return system(buffer);
}

PRIVATE int
rmdir(name)
    char *name;
{
    char buffer[256];

    (void)sprintf(buffer, "rmdir %s", name);
    return system(buffer);
}

int ndeadchild;
int waitinited;

PRIVATE void
waitinit() {
    if (waitinited) return;
    signal(SIGCLD, cldcatch);
}

PRIVATE void
cldcatch() {
    ndeadchild++;
    signal(SIGCLD, cldcatch);
}

PRIVATE void
nbwait(status)
    int *status;
{
    if (ndeadchild) {
        ndeadchild--;
        return wait(status);
    }
    return -1;
}
#endif
