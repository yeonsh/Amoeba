/*	@(#)pdump.c	1.9	96/02/27 13:19:50 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** pdump
**   Dump a process descriptor read from a Bullet file or from standard
**   input.  Output goes to standard output.
**   This main program is universal; there are subroutines named
**   <architecture>_dumpfault() in each file pdmp_<architecture>.c.
**   The dump routine is selected on the basis of the architecture in
**   the pd_magic field of the process descriptor.
*/

#include "stdio.h"
#include "stdlib.h"

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "exception.h"
#include "module/proc.h"
#include "bullet/bullet.h"
#include "byteorder.h"		/* Byte order for the machine running pdump */
#include "module/name.h"

#include "pdump.h"


/* Globals needed by getlong() */
process_d *GlobPd;
capability GlobCap;

/*
** XXX should make the list of supported architectures a configuration choice.
*/
extern void i80386_dumpfault();
extern void mc68000_dumpfault();
extern void sparc_dumpfault();
extern void unknown_dumpfault();

static struct dtab {
	char arch[ARCHSIZE];	/* name of architecture */
	void (*ptr)();
} dumptab[] = {
	{	"i80386",	i80386_dumpfault	},
	{	"mc68000",	mc68000_dumpfault	},
	{	"sparc",	sparc_dumpfault		},
	{	"",		unknown_dumpfault	}
};

static void pdump();
static void prstate();

main(argc, argv)
    int		argc;
    char **	argv;
{
    int		size;
    process_d *	pd;
    char *	pdbufend;
    static char pdbuf[MAX_PDSIZE];
    int		nthreads;

    if (argc != 2)
    {
	    fprintf(stderr, "usage: %s processdescriptor\n", argv[0]);
	    exit(2);
    }
    
    if (strcmp(argv[1], "-") == 0)
    {
        if (isatty(fileno(stdin)))
        {
	    fprintf(stderr, "%s: input mustn't be a tty\n", argv[0]);
	    exit(2);
        }
        size = fread(pdbuf, sizeof(char), sizeof(pdbuf), stdin);
        if (size <= 0)
        {
		fprintf(stderr, "%s: empty input\n", argv[0]);
		exit(1);
	}
	pdbufend = pdbuf + size;
	if ((pd = pdmalloc(pdbuf, pdbufend)) == 0)
	{
	    fprintf(stderr, "%s: pd_malloc failed\n", argv[0]);
	    exit(1);
	}
	if (buf_try_get_pd(pdbuf, pdbufend, pd, &nthreads) == 0)
	{
	    fprintf(stderr, "%s: input is not a process descriptor\n",
		    argv[0]);
	    exit(1);
	}
	if (nthreads != pd->pd_nthread) {
	    fprintf(stderr, "%s: warning: truncated process descriptor\n",
		    argv[0]);
	    fprintf(stderr, "%s: got %d thread descriptors of %d\n",
		    argv[0], nthreads, pd->pd_nthread);
	    /* but continue dumping the ones we *did* get */
	    pd->pd_nthread = nthreads;
	}
    }
    else
    {
	errstat err;

	if ((err = name_lookup(argv[1], &GlobCap)) != STD_OK)
	{
	    fprintf(stderr, "%s: can't find %s (%s)\n",
					argv[0], argv[1], err_why(err));
	    exit(1);
	}
	pd = pd_read(&GlobCap);
	if (pd == NULL)
	{
	    fprintf(stderr, "%s: can't read process descriptor from %s\n",
							argv[0], argv[1]);
	    exit(1);
	}
	size = pd_size(pd);
    }
    GlobPd = pd;
    pdump(pd, size);
    exit(0);
    /*NOTREACHED*/
}

static void
pdump(pd, pdlen)
    register process_d *pd;
    int pdlen;
{
    register segment_d  *sd;
    register struct thread_d *td;
    register struct thread_idle *tdi;
    register struct thread_kstate *tdk;
    int nt, ns, it;
    char *p;
    void (*dumpfault)();  /* pointer to a function to dump the fault frames */
    struct dtab * dtp;
    FILE *fp = stdout;

    ns = pd->pd_nseg;
    nt = pd->pd_nthread;
    fprintf(fp, "\nProcess descriptor from architecture: %s\n\n",
								pd->pd_magic);
/*
** before we go further we set the pointer to the function that will
** dump the fault frame(s)
*/
    for (dtp = dumptab; dtp->arch[0] != 0; dtp++)
	if (strncmp(dtp->arch, pd->pd_magic, ARCHSIZE) == 0)
	    break;
    dumpfault = dtp->ptr;

/* now we go on and dump the process descriptor */
    fprintf(fp,
      "  offset     addr+     len=     end    flags (%d segments)\n", ns);
    sd = PD_SD(pd);
    while(ns--) {
	if (sd->sd_offset == 0)
	    fprintf(fp, "         ");
	else
	    fprintf(fp, "%8.8x ", sd->sd_offset);
	fprintf(fp, "%8.8x+%8.8x=%8.8x %8.8x",
		sd->sd_addr, sd->sd_len, sd->sd_addr+sd->sd_len, sd->sd_type);
	if (sd->sd_type & MAP_SYMTAB)
	    fprintf(fp, " symbol table");
	else {
	    if (sd->sd_type & MAP_INPLACE)
		fprintf(fp, " inplace");
	    switch (sd->sd_type & MAP_GROWMASK) {
	    case MAP_GROWNOT:	fprintf(fp, " fixed");	break;
	    case MAP_GROWUP:	fprintf(fp, " heap");	break;
	    case MAP_GROWDOWN:	fprintf(fp, " stack");	break;
	    }
	    switch (sd->sd_type & MAP_PROTMASK) {
	    case MAP_READONLY:	fprintf(fp, " read-only");	break;
	    case MAP_READWRITE:	fprintf(fp, " read/write");	break;
	    }
	    switch (sd->sd_type & MAP_TYPEMASK) {
	    case MAP_TYPETEXT:	fprintf(fp, " text");	break;
	    case MAP_TYPEDATA:	fprintf(fp, " data");	break;
	    }
	}
	fprintf(fp, "\n");
	sd++;
    }
    fprintf(fp, "%d threads:\n", nt);
    for (it = 0, td = PD_TD(pd); it < nt; ++it, td = TD_NEXT(td)) {
	if ((char *)td + td->td_len > (char *) pd+pdlen) {
	    fprintf(fp, "...process descriptor truncated...\n");
	    break;
	}
	fprintf(fp, "THREAD %d:\tstate\t%08.8x\t", it, td->td_state);
	prstate(fp, td->td_state);
	fprintf(fp, "\n");
	p = (char *)(td+1); /* p points to the next extra data structure... */
	if (td->td_extra & TDX_IDLE) {
	    tdi = (thread_idle *)p;
	    p = (char *)(tdi+1);
	    fprintf(fp, "\tpc\t%8x\n\tsp\t%8x\n", tdi->tdi_pc, tdi->tdi_sp);
	}
	if (td->td_extra & TDX_KSTATE) {
	    tdk = (thread_kstate *)p;
	    p = (char *)(tdk+1);
	    if( tdk->tdk_signal != 0 )
		fprintf(fp, "\tsignal\t%8d\t%s\n", tdk->tdk_signal,
						exc_name(tdk->tdk_signal));
	}
	if (td->td_extra & TDX_USTATE) {
	    (*dumpfault)(p, (char *)(td) + td->td_len - p);
	    /* ASSUME USTATE IS LAST */
	}
	else if (p != (char *)(td) + td->td_len) {
	    fprintf(fp, "\t... unknown extra bits %x, len %d\n",
						    td->td_extra, td->td_len);
	}
    }
}

static struct flaginfo {
	long mask;
	char *name;
} flagnames[] = {
/*
sed -n '/^#d.* TDS_\([A-Z0-9]*\).*$/s//	{TDS_\1,	"\1"},/p' </usr/amoeba/src/h/proc.h
*/
	{TDS_RUN,	"RUN"},
	{TDS_DEAD,	"DEAD"},
	{TDS_EXC,	"EXC"},
	{TDS_INT,	"INT"},
	{TDS_FIRST,	"FIRST"},
	{TDS_NOSYS,	"NOSYS"},
	{TDS_START,	"START"},
	{TDS_SRET,	"SRET"},
	{TDS_LSIG,	"LSIG"},
	{TDS_STUN,	"STUN"},
	{TDS_HSIG,	"HSIG"},
	{TDS_SHAND,	"SHAND"},
	{TDS_DIE,	"DIE"},
	{TDS_STOP,	"STOP"},
	{TDS_USIG,	"USIG"},
	{0,		NULL}
};

static void
prstate(fp, state)
	FILE *fp;
	long state;
{
    struct flaginfo *f;
    char *sep = "";
    
    for (f = flagnames; f->mask != 0; f++) {
	if (state & f->mask) {
	    fprintf(fp, "%s%s", sep, f->name);
	    sep = " ";
	}
    }
}


/*
** getlong - Get a long from the core image.
** XXX This prints to stdout since that's where all the other output
** goes except usage messages.  It really shouldn't print at all...
*/
static int
getlong(addr, plong)
    unsigned long addr;
    long *plong;
{
    long result;
    int ns;
    segment_d *sp;
    b_fsize numread;
    errstat err;
    capability *cap;

    ns = GlobPd->pd_nseg;
    sp = PD_SD(GlobPd);
    while (ns--) {
	if ((unsigned)sp->sd_addr <= addr && 
			      (unsigned)(sp->sd_addr+sp->sd_len) > addr+3) {
	    cap = &sp->sd_cap;
	    if (NULLPORT(&cap->cap_port))
	    	cap = &GlobCap;
	    if (NULLPORT(&cap->cap_port)) {
		fprintf(stdout,"\nCannot read from segment using STDIN as proces descriptor\n");
		return 0;
	    }

	    /* XXX Of course, this really only works if sizeof(long) == 4 */
	    err = b_read(cap,
	    		(b_fsize) (addr - sp->sd_addr + sp->sd_offset),
	    		(char *) &result,
	    		(b_fsize) sizeof (long),
	    		&numread);
	    if (err != STD_OK || numread != sizeof (long)) {
		fprintf(stdout, "\nb_read (address %x) failed: %s\n",
							addr, err_why(err));
		return 0;
	    }
	    *plong = result;
	    return 1;
	}
	sp++;
    }
    fprintf(stdout, "\nUndefined address (%x)\n", addr);
    return 0;
}

int
getlong_be(addr, plong)
    unsigned long addr;
    long *plong;
{
    if (!getlong(addr, plong))
	return 0;
    dec_l_be(plong);
    return 1;
}

int
getlong_le(addr, plong)
    unsigned long addr;
    long *plong;
{
    if (!getlong(addr, plong))
	return 0;
    dec_l_le(plong);
    return 1;
}

getstruct( what, where, size, bigguy )
unsigned long what;
long *where;
int size;
int bigguy;
{
    if ( size & ( sizeof( long ) - 1 )) {
	fprintf( stdout, "getstruct called with unaligned size %x\n", size );
	return 0;
    }

    if ( what & ( sizeof( long ) - 1 )) {
	fprintf( stdout, "getstruct called with unaligned address %x\n",
		what );
	return 0;
    }

    while ( size > 0 ) {
	if ( bigguy ) {
	    if ( getlong_be( what, where ) == 0 ) {
		fprintf( stdout,
			"getstruct had trouble from getlong (%x)\n", what );
		return 0;
	    }
	} else {
	    if ( getlong_le( what, where ) == 0 ) {
		fprintf( stdout,
			"getstruct had trouble from getlong (%x)\n", what );
		return 0;
	    }
	}

	where += 1;
	what += sizeof( long );
	size  -= sizeof( long );
    }
    return 1;
}
