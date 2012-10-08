/*	@(#)exec_file.c	1.5	96/02/27 11:03:12 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** exec_file()
**	Given the capability for an amoeba binary and several other parameters
**	exec_file reads the process descriptor for a binary and calls
**	exec_pd to fill in any defaults for unspecified parameters.  This
**	in turn calls real_exec() to build the process on the target machine
**	and run it.
**	The binary must begin with the process descriptor (prepended by
**	ainstall) which is then followed by the normal a.out header and then
**	the segments (text, bss, data, etc) which comprise the binary.
**
**	exec_file is the lowest level portable interface to run Amoeba
**	programs. (The implementation needn't be portable; just the interface.)
**
**	Some helper functions have been separated out into their own files,
**	so they are accessible by the general public:
**		exec_multi_findhost(), buildstack().
**
** WARNING:
**	exec_file() and its helper functions must be reentrant, since
**	several Amoeba threads might call them concurrently.  Therefore,
**	everything should happen on the stack; no global or static variables
**	should be used.
*/

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "amoeba.h"
#include "ampolicy.h"
#include "cmdreg.h"
#include "stderr.h"
#include "caplist.h"
#include "module/proc.h"
#include "module/name.h"
#include "module/stdcmd.h"
#include "bullet/bullet.h"
#include "am_exerrors.h"
#include "exec_fndhost.h"
#include "class/run.h"

#ifdef DEBUG
#include <stdio.h>
#define dbprintf(list)	printf list
#else
#define dbprintf(list)	{}
#endif

#define DEFSTACK	(16*1024)	/* Default stack size */

/* After having pushed the argument vector, etc., on the stack,
 * there should at least be NEEDSTACK bytes left.
 */
#define NEEDSTACK	(13*1024)

extern char **		environ;
extern struct caplist *	capv;

/* Default argument list */
static char *		argvdefault[2] = {"-", NULL};

static errstat		addstack();
static errstat		real_exec();
static errstat		exec_multi_pd();
static void		setcomment();
static char *		addstr();

int			_stack_required();
unsigned long		buildstack();

/*
** EXEC_FILE
**
** This is a high level interface to the kernel process management.
** The parameters:
**   prog:	Points to a capability for the binary or directory of
**		binaries to execute.  If NULL then it is set to point to
**		the capability for the binary specified	by argv[0].
**   host:	Points to the capability for the process server of the
**		machine on which to run the process.  If NULL then
**		exec_multi_findhost() will be used to select a processor.
**   owner:	Capability for default process owner (which will receive
**		a checkpoint of something happens to the process).
**		If NULL then there is no signal owner.
**   stacksize:	Size of stack in bytes to allocate for the main thread of the
**		process.  If this is 0 it looks in the process descriptor for
**		the stack size.  If this is also 0 it defaults to DEFSTACK.
**   argv:	Argument list for the process.  argv[0] should contain the
**		name of the binary to run if 'prog' is NULL.  If argv is NULL
**		it defaults to {"-", NULL}.
**   envp:	Pointer to string environment to start the process with.  If
**		NULL the string environment of the caller is taken from
**		'environ'.
**   caps:	Pointer to capability environment to start the process with.
**		If NULL the capability environment of the caller is taken from
**		'capv'.
**   retcap:	Capability for the new process which is returned to the caller.
*/

errstat
exec_file(prog, host, owner, stacksize, argv, envp, caps, retcap)
capability *	prog;	   /* In: program (process descriptor) */
capability *	host;	   /* In: host processor (process server) */
capability *	owner;	   /* In: process owner */
int		stacksize; /* In: stack size (bytes) */
char **		argv;	   /* In: program arguments (null-terminated) */
char **		envp;	   /* In: string environment (null-terminated) */
struct caplist *caps;	   /* In: capability environment */
capability *	retcap;	   /* Out: capabilty for running process */
{
    errstat	err;
    process_d **pd;
    int		i, npd;
    capability	progcap;
    
    /* Take default prog capability from argv[0] */
    if (prog == NULL) {
	if (argv == NULL || argv[0] == NULL) {
	    return AMEX_NOPROG;
	}
	if (name_lookup(argv[0], &progcap) != STD_OK) {
	    return AMEX_PDLOOKUP;
	}
	prog = &progcap;
    }
    
    /* pd_read_multiple() returns an array of process descriptors
     * (size 1 if prog is a file, size >= 1 if it's a directory of binaries).
     * The file relative segment descriptors are patched to point
     * to the corresponding binary.
     */
    if ((err = pd_read_multiple(prog, &pd, &npd)) != STD_OK) {
	dbprintf(("pd_read_multiple failed (%s)\n", err_why(err)));
	if (err == STD_NOSPACE) {
	    return AMEX_MALLOC;
	} else {
	    return AMEX_PDREAD;
	}
    }

    err = exec_multi_pd(pd, npd, host, owner, stacksize,
			argv, envp, caps, retcap);

    /* free the process descriptors, and the pointer array */
    for (i = 0; i < npd; i++) {
	free(pd[i]);
    }
    free(pd);

    return err;
}

static errstat
exec_multi_pd(pd_list, npd, host, owner, stacksize, argv, envp, caps, retcap)
process_d     *  pd_list[]; /* In: array of pointers to process descriptors */
int		 npd;	    /* In: length of this array */
capability     * host;      /* In: host processor (process server) */
capability     * owner;     /* In: process owner */
int		 stacksize; /* In: stack size (bytes) */
char	      ** argv;	    /* In: program arguments (null-terminated) */
char	      ** envp;	    /* In: string environment (null-terminated) */
struct caplist * caps;	    /* In: capability environment */
capability     * retcap;    /* Out: capabilty for running process */
{
    register process_d *pd;
    int 		pd_index;
    errstat		err;
    capability		hostcap;
    capability		dummycap;
    
    /*
    ** Compute some defaults for optional arguments.
    ** Default host depends on the process's architecture ID.
    ** Default stack may be taken from process descriptor.
    */
    if (argv == NULL) {
	argv = argvdefault;
    }
    
    if (envp == NULL) {
	envp = environ;
    }

    if (caps == NULL) {
	caps = capv;

	if (caps == NULL) {
	    /* Without capability environment, the process has no output
	     * (not even stdout) and thus it makes no sense to run it.
	     * The default, capv, is normally only NULL on Unix.
	     */
	    return AMEX_NOCAPS;
	}
    }

    if (retcap == NULL) {
	/* Caller doesn't want to know */
	retcap = &dummycap;
    }
    
    if (host == NULL) {
	/* no host prescribed, so use the run server to get a host */
	char hostname[PD_HOSTSIZE], prevhost[PD_HOSTSIZE];
	unsigned int try;

	prevhost[0] = '\0'; /* no previous host yet */
	for (try = 0; try < 6; try++) {
	    err = exec_multi_findhost((capability *)NULL /* runsvr */,
				      (capability *)NULL /* pooldir */,
				      pd_list, npd, &pd_index,
				      &hostcap, hostname);
	    if (err != STD_OK) {
		return AMEX_NOHOST;
	    }

	    if (strncmp(hostname, prevhost, PD_HOSTSIZE) == 0) {
		/* same host selected as in previous iteration,
		 * so give it a chance to free up some memory
		 */
		sleep(try);
	    }

	    /* Call real_exec() to do the real work. 
	     * If it finds insufficient memory, try again.
	     */
	    pd = pd_list[pd_index];
	    err = real_exec(pd, pd_size(pd), &hostcap, owner,
			    (long)stacksize, argv, envp, caps, retcap);

	    if (err == STD_NOMEM || err == STD_NOSPACE) {
		/* try again, but first note which host gave this problem */
		strncpy(prevhost, hostname, PD_HOSTSIZE);
	    } else {
		break;
	    }
	}
    } else {
	/* There is a host prescribed.  We see if the runsvr will tell
	 * us the architecture and the exec capability.  If not then
	 * we try the raw process server cap.  In the case that no run
	 * server has been active it will work.  Otherwise it won't.
	 */
	capability execcap;
	capability runsvr;
	char arch[ARCHSIZE];

	err = exec_get_runsvr(&runsvr);
	if (err == STD_OK) {
	    /* We have a runsvr */
	    err = run_get_exec_cap(&runsvr, host, &execcap, arch);
	}
	if (err == STD_OK) {
	    /* We know the arch and the exec cap so let's run it */
	    for (pd_index = 0; pd_index < npd; pd_index++) {
		pd = pd_list[pd_index];
		if (strncmp(pd->pd_magic, arch, ARCHSIZE) == 0)
		    break;
	    }
	    if (pd_index >= npd) {
		err = AMEX_NOHOST; /* It should be "bad arch" but this'll do */
	    } else {
		err = real_exec(pd, pd_size(pd), &execcap, owner,
				(long)stacksize, argv, envp, caps, retcap);
		if (err == STD_CAPBAD) {
		    /* Try again */
		    err = run_get_exec_cap(&runsvr, host, &execcap, arch);
		    if (err == STD_OK) {
			err = real_exec(pd, pd_size(pd), &execcap, owner,
				(long)stacksize, argv, envp, caps, retcap);
		    }
		}
	    }
	} else {
	    /* There is no runsvr to tell us the architecture of our host
	     * so we just try all the possiblities.
	     * If a pd has the wrong architecture for the host, it will
	     * return STD_ARGBAD (but it might be the case that something
	     * else is wrong with the pd, of course).
	     */
	    for (pd_index = 0; pd_index < npd; pd_index++) {
		pd = pd_list[pd_index];
		err = real_exec(pd, pd_size(pd), host, owner,
				(long)stacksize, argv, envp, caps, retcap);
		if (err != STD_ARGBAD) {
		    /* i.e. STD_OK or some unexpected error.
		     * Lack of memory is also fatal in this case, because
		     * the caller should probably know what it is doing,
		     * when specifying a specific host.
		     */
		    break;
		}
	    }
	}
    }

    return err;
}


errstat
exec_pd(pd, pdlen, host, owner, stacksize, argv, envp, caps, retcap)
process_d *		pd;
int			pdlen;
capability *		host;
capability *		owner;
int			stacksize;
char **			argv;
char **			envp;
struct caplist *	caps;
capability *		retcap;
{
    /* Check process descriptor size */
    if (pdlen != pd_size(pd))
	return AMEX_PDSIZE;
    
    /* executing one pd is special case of executing >= 1 pd: */
    return exec_multi_pd(&pd, 1, host, owner, stacksize,
			 argv, envp, caps, retcap);
}


static errstat
real_exec(pd, pdlen, host, owner, stacksize, argv, envp, caps, retcap)
process_d *	pd;	   /* In: process descriptor */
int		pdlen;	   /* In: size of process descriptor (bytes) */
capability *	host;	   /* In: host processor (process server) */
capability *	owner;     /* In: process owner */
long		stacksize; /* In: stack size (bytes) */
char **		argv;	   /* In: program arguments (null-terminated) */
char **		envp;	   /* In: string environment (nul-terminated) */
struct caplist *caps;	   /* In: capability environment */
capability *	retcap;	   /* Out: capabilty for running process */
{
    errstat		err;
    uint16		istack;
    segment_d *		sd;
    segment_d		save_stack_sd;	/* Save and restore the stack
					 * segment descriptor on error,
					 * so that the pd is not damaged
					 * and retries will work */
    /*
    ** Find the slot for the stack segment.
    ** Heuristic: must have null port, and flags must either have
    ** the MAP_SYSTEM bit set (which is the 'right' way to do it),
    ** or grow down and have type data.
    */
    for (istack = 0, sd = PD_SD(pd); istack < pd->pd_nseg; istack++, sd++) {
	if (NULLPORT(&sd->sd_cap.cap_port) &&
	    ((sd->sd_type & MAP_SYSTEM) ||
		((sd->sd_type & MAP_GROWMASK) == MAP_GROWDOWN &&
		 (sd->sd_type & MAP_TYPEMASK) == MAP_TYPEDATA))) {

	    save_stack_sd = *sd;
	    if ((err = addstack(pd, pdlen, sd, host, stacksize,
					argv, envp, caps)) != STD_OK) {
		*sd = save_stack_sd;
		return err;
	    }
	    break;
	}
    }

    /* Set the default process-signal owner */
    if (owner != NULL)
	pd->pd_owner = *owner;

    /* Get the processor to run the process */
    if ((err = pro_exec(host, pd, retcap)) != STD_OK) {
	/* Destroy the stack segment if we added it: */
	if (istack < pd->pd_nseg) {
		segment_d *stack_sd = &PD_SD(pd)[istack];
		std_destroy(&stack_sd->sd_cap);
		/* Restore the original stack segment descriptor: */
		*stack_sd = save_stack_sd;
	}
	return err;
    }
    
    /* Set the process comment */
    setcomment(retcap, argv, envp);
    
    return STD_OK;
}


/*
** Add a comment string to the process.
** By convention this has the format "<user>: <prog> <arg> ..."
** The username is taken from $LOGNAME; default is "?".
** The program name is the basename of argv[0].
*/

static void
setcomment(cap, argv, envp)
capability *cap;
char **argv, **envp;
{
    char *env_lookup();
    char buf[32];
    char *p, *e;
    char *user;
    
    p = buf;
    e = buf + sizeof buf - 1;

    /* Look up the LOGNAME or USER name in the new environment, not the
     * parent's:
     */
    if ((user = env_lookup(envp, "LOGNAME")) == NULL &&
				(user = env_lookup(envp, "USER")) == NULL)
	    user = "?";

    p = addstr(p, e, user);
    p = addstr(p, e, ":");
    if (*argv != NULL) {
    	char *a = strrchr(argv[0], '/');
    	if (a == NULL)
    	    a = argv[0];
    	else
    	    a++;
    	p = addstr(p, e, " ");
    	p = addstr(p, e, a);
    	argv++;
    }
    for (; *argv != NULL && p < e; ++argv) {
    	p = addstr(p, e, " ");
	p = addstr(p, e, *argv);
    }
    *p = '\0';
    (void) pro_setcomment(cap, buf);
}


static char *
addstr(p, e, str)
char *p;
char *e;
char *str;
{
    while (p < e && *str != '\0')
	*p++ = *str++;
    return p;
}


/* some forward declarations */
static segid		segalloc();
static void		segfree();
static errstat		segstore();

#define HIGH_CLICKSHIFT	13

static errstat
addstack(pd, pdlen, sd, host, stacksize, argv, envp, caps)
process_d       *pd;	    /* In: process descriptor buffer */
int		 pdlen;	    /* In: size of process descriptor (bytes) */
segment_d	*sd;	    /* In: stack segment descriptor */
capability	*host;	    /* In: host processor (process server) */
long		 stacksize; /* In: stack size (bytes) */
char		*argv[];    /* In: program arguments (null-terminated) */
char		*envp[];    /* In: string environment (null-terminated) */
struct caplist 	 caps[];    /* In: capability environment */
{
    capability	   stackcap;	/* Stack segment's capability */
    unsigned long  sp;		/* Receives stack pointer */
    unsigned long  stackend;	/* Start address of stack segment */
    unsigned long  stackstart;	/* Start address of stack segment */
    char 	  *stackbuf;	/* Stack segment buffer */
    segid	   sid;		/* Stack segment id (local) */
    long	   needed;
    thread_d      *td;
    thread_idle   *tdi;
    int		   clickshift;
    int		   stackclicks;
	
    if (stacksize == 0) {
	/* Get default stack size from segment descriptor, if possible */
	if (sd->sd_len > 0) {
	    stacksize = sd->sd_len;
	} else {
	    stacksize = DEFSTACK;
	}
    }

    /* Make sure it is large enough to contain the string environment,
     * capability environment and argument list.
     * It also should leave the process some breathing room.
     */
    needed = _stack_required(argv, envp, caps) + NEEDSTACK;
    if (needed > stacksize) {
	stacksize = needed;
    }
    dbprintf(("add_stack: needed stacksize = %d\n", needed));
    dbprintf(("add_stack: chosen stacksize = %d\n", stacksize));

    if (sd->sd_addr != 0) {	/* Modern stack segment */
	/* To avoid a pro_getdef() call when it is not really required,
	 * round up stacksize to a default page boundary, that will work on
	 * all architectures supported.  HIGH_CLICKSHIFT is used for this.
	 */
	clickshift = HIGH_CLICKSHIFT;
	stackend = sd->sd_addr + sd->sd_len;
    } else {	/* Backwards compatibility */
	int vmlo;
	int vmhi;

	/* In this case we don't know where the stack should be mapped in,
	 * so ask parameters of the machine by means of pro_getdef().
	 */
	if (pro_getdef(host, &clickshift, &vmlo, &vmhi) != STD_OK) {
	    return AMEX_GETDEF;
	}
	stackend = vmhi << clickshift;
    }

    stackclicks = (stacksize + (1 << clickshift) - 1) >> clickshift;
    stacksize = stackclicks << clickshift;
    dbprintf(("add_stack: stackclicks = %d\n", stackclicks));
    dbprintf(("add_stack: stacksize = %d\n", stacksize));

    /* Change stackstart accordingly */
    stackstart = stackend - stacksize;
    
    /* Allocate the stack buffer */
    sid = segalloc(stacksize, &stackbuf);
    if (sid < 0) {
	return AMEX_MALLOC;
    }
    
    /* Build the stack segment */
    sp = buildstack(stackbuf, stacksize, stackstart, argv, envp, caps);
    if (sp == 0) {
	/* this shouldn't happen, when _stack_required tells the truth */
	segfree(sid, stackbuf);
	return AMEX_STACKOV;
    }
    
    /* Store it at a file server (or unmap it, to get a capability for it) */
    if (segstore(sid, stackbuf, stacksize, &stackcap) != STD_OK) {
    	return AMEX_SEGCREATE;
    }
    
    /* Fill in the segment descriptor and the stack pointer in the
       first thread descriptor (in fact, we assume there is only one
       thread).  Because some programs leave the flags for the stack segment
       zero, we set them here.  We must set MAP_AND_DESTROY anyway. */
    
    sd->sd_cap = stackcap;
    sd->sd_addr = stackstart;
    sd->sd_len = stacksize;
    sd->sd_type = MAP_GROWDOWN|MAP_TYPEDATA|MAP_READWRITE|MAP_AND_DESTROY;
    td = PD_TD(pd);
    tdi = (thread_idle *) (td + 1);
    tdi->tdi_sp = sp;

    return STD_OK;
}

/* Primitives to allocate and manipulate the stack segment. */

#ifdef UNIX

#include "bullet/bullet.h"

/* Unix version: allocate with malloc(), store on bullet svr */

static segid
segalloc(size, pstart)
	long size;
	char **pstart;
{
	*pstart = (char *) malloc((unsigned int)size);
	if (*pstart == NULL)
		return STD_NOSPACE;
	else
		return STD_OK;
}

/*ARGSUSED*/
static void
segfree(sid, start)
	segid sid;
	char *start;
{
	free(start);
}

/*ARGSUSED*/
static errstat
segstore(sid, start, size, pcap)
	segid sid;
	char *start;
	long size;
	capability *pcap;
{
	char *bulletname;
	capability svr;
	errstat err;
	
	/*
	** Allow $BULLET to override the default bullet server.
	** This may be handy in the bootstrap environment.
	** (We can't use the capability environment here
	** because the main use for this hack is in 'ax',
	** which runs under Unix.)
	*/
	if ((bulletname = getenv("BULLET")) == NULL)
		bulletname = DEF_BULLETSVR;
	if ((err = name_lookup(bulletname, &svr)) != STD_OK)
		return err;
	return b_create(&svr, start, size, BS_COMMIT, pcap);
}

#else

/* Native Amoeba version: allocate segment, unmap to store */

static segid
segalloc(size, pstart)
	long size;
	char **pstart;
{
	*pstart = findhole(size);
	if (*pstart == NULL)
		return STD_NOSPACE;
	return seg_map((capability *)NULL, (vir_bytes) *pstart,
		(vir_bytes) size, MAP_GROWUP | MAP_TYPEDATA | MAP_READWRITE);
}

/*ARGSUSED*/
static void
segfree(sid, start)
	segid sid;
	char *start;
{
	(void)seg_unmap(sid, (capability *)0);
}

/*ARGSUSED*/
static errstat
segstore(sid, start, size, pcap)
	segid sid;
	char *start;
	long size;
	capability *pcap;
{
	return seg_unmap(sid, pcap);
}

#endif
