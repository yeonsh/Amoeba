/*	@(#)acv.c	1.6	96/02/27 13:27:32 */
/*
 * Copyright 1996 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "acv.h"
#include "namelist.h"
#include "defarch.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "module/proc.h"

#ifdef DEBUG
#define dbprintf(list)	printf list
#else
#define dbprintf(list)	/**/
#endif

/* Option defaults and other parametrizations. */
#define MAX_SIZE	8192

#ifndef BSSEXTRA
#define BSSEXTRA	16		/* Default extra bss */
#endif
#define DEFSTACK	(8*1024)

/* Getopt interface */

extern char *	optarg;
extern int	optind;

/* Option variables */

static char *	architecture;
static int	bssextra;
static int	stacksize;
static int	textoffset = -1;

char *		progname = "ACV";
				/* Program name -- overridden by argv[0] */

/* Forward declarations */

void		convert();
void		fatal();
void		fatalperror();
void		losefile();
void		usage();
void		writefile();

/* Main program -- parse command line, set defaults, and pass the job on */

main(argc, argv)
int	argc;
char **	argv;
{
    int	c;

    if (argc > 0 && argv[0] != NULL) {
	/* Try to set progname to basename(argv[0]) */
	char *p;

	if ((p = strrchr(argv[0], '/')) == NULL)
	    p = argv[0];
	else
	    ++p;
	if (*p != '\0')
	    progname = p;
    }
    
    while ((c = getopt(argc, argv, "b:s:T:")) != EOF) {
	switch (c) {
	case 'b':
		bssextra = atoi(optarg);
		break;
	
	case 's':
		stacksize = atoi(optarg);
		break;
	
	case 'T':
		textoffset = strtol(optarg, (char **)NULL, 16);
		break;
	
	default:
		usage();
		/*NOTREACHED*/
	}
    }
    
    /* Check if number of the other arguments is correct*/

    if (argc - optind == 3) {
	/* Remaing args are <unixfile> <amoebafile> <architecture> */
	architecture = argv[optind+2];
    } else if (argc - optind == 2) {
	/* Remaing args are <unixfile> <amoebafile>
	 * Assume the architecture from "defarch.h" is meant.
	 */
	architecture = ARCHITECTURE;
    } else {
	usage();
    }

    /* Set default options */
    if (bssextra <= 0)
	bssextra = BSSEXTRA;
    bssextra *= 1024;	/* bssextra is in kbytes! */

    if (stacksize <= 0)
	stacksize = 0; /* Default will be chosen runtime */
    else
	stacksize *= 1024;	/* stacksize is specified in kbytes */
    
    convert(argv[optind], argv[optind + 1]);

    exit(0); /* Convert will call exit(1) on failure */
}

void	mc68000_info();
void	i80386_info();
void	sparc_info();

struct arch_info {
    char *arch_name;
    void (*arch_infofunc)( /* vir_high, click_shift, click_size */ );
} arch_table[] = {
    { "mc68000", mc68000_info },
    { "i80386",  i80386_info  },
    { "sparc",   sparc_info   },
    { NULL,	 (void (*)()) NULL }
};

#define NTHREAD	1

static union proc_info {
    process_d	pd;
    char	padding[1024];
} u;

struct aoutinfo	ai;

#define SYMSIZE(symsiz, strsiz)	(3*sizeof(long) + (symsiz) + (strsiz))

void
convert(uxfile, amfile)
char *	uxfile;		/* unix file to convert */
char *	amfile;		/* name of amoeba file to be created */
{
    char *		aout_getinfo();

    int			fd;
    segment_d *		sd;
    thread_d *		td;
    thread_idle *	tdi;
    char *		errstr;
    int			nseg;
    int			pdsize;
    long		filloff;
    /* machine dependent info: */
    struct arch_info   *archp;
    long		vir_high, click_shift, click_size;
    
    /* Open the file, read the header, and check format */
    if ((fd = open(uxfile, 0)) < 0)
	fatalperror("can't open %s", uxfile);
    if ((errstr = aout_getinfo(fd, &ai)) != 0)
	fatal(errstr);
    
    /*
    ** Calculate the text offset.  This is the difference of the -T option
    ** and the text addr from the file.  Note that it is not an error for
    ** this difference to be negative (think about forcing -T 0).
    */
    if (textoffset == -1)
	textoffset = 0;
    else
	textoffset -= ai.ai_txt_virtual;

    /*
    ** Check that the entry point is not lower that the text address;
    ** if this is so, the user probably forgot the -T flag to ld.
    */
    if (ai.ai_entrypoint < ai.ai_txt_virtual + textoffset)
	fatal("entry point (0x%lx) before text start (0x%x); use -T?",
		ai.ai_entrypoint, ai.ai_txt_virtual + textoffset);
    
    /*
    ** If architecture is not specified by binary file set it here
    **/
    if (ai.ai_architecture == 0)
	ai.ai_architecture = architecture;
    /*
    ** Fill in the process descriptor.
    ** This is rather boring.  We can't loop over the segments,
    ** since each is a little different.
    */
    nseg = 4;		/* at least */
    if (ai.ai_sym_tab != NULL || ai.ai_str_tab != NULL)
	nseg++;
    pdsize = (sizeof(process_d) + nseg*sizeof(segment_d) +
		 NTHREAD*(sizeof(thread_d) + sizeof(thread_idle)));
    
    /* Process descriptor header fields */
    (void) strcpy(u.pd.pd_magic, ai.ai_architecture);
    u.pd.pd_nseg = nseg;
    u.pd.pd_nthread = NTHREAD;

    /* the segments start right after the process descriptor */
    filloff = pdsize;
    
    /* Text  + RO data segment */
    sd = PD_SD(&u.pd);
    sd->sd_offset = filloff;
    sd->sd_addr = ai.ai_txt_virtual + textoffset;
    if (ai.ai_rom_size != 0 &&
	ai.ai_txt_virtual + ai.ai_txt_size != ai.ai_rom_virtual)
    {
	fatal("ROM doesn't follow text in address space");
    }
    sd->sd_len = ai.ai_txt_size + ai.ai_rom_size;
    dbprintf(("text at offset %ld, addr 0x%lx, length %ld\n",
	      sd->sd_offset, sd->sd_addr, sd->sd_len));
    filloff += sd->sd_len;
    sd->sd_type = MAP_GROWNOT | MAP_TYPETEXT | MAP_READONLY;
    
    /* Data segment */
    ++sd;
    sd->sd_offset = filloff;
    sd->sd_addr = ai.ai_dat_virtual + textoffset;
    sd->sd_len = ai.ai_dat_size;
    filloff += sd->sd_len;
    dbprintf(("data at offset %ld, addr 0x%lx, length %ld\n",
	      sd->sd_offset, sd->sd_addr, sd->sd_len));
    sd->sd_type = MAP_GROWNOT | MAP_TYPEDATA | MAP_READWRITE;
    
    /* BSS segment */
    ++sd;
    sd->sd_offset = 0; /* unused */
    sd->sd_addr = ai.ai_bss_virtual + textoffset;
    sd->sd_len = ai.ai_bss_size + bssextra;
    dbprintf(("bss addr 0x%lx, length %ld\n", sd->sd_addr, sd->sd_len));
    sd->sd_type = MAP_GROWUP | MAP_TYPEDATA | MAP_READWRITE;
    
    /* Stack segment; convert stacksize to clicks */
    ++sd;

    /* get machine dependent info */
    for (archp = &arch_table[0]; archp->arch_name != NULL; archp++) {
	if (strcmp(ai.ai_architecture, archp->arch_name) == 0) {
	    break;
	}
    }
    if (archp->arch_name != NULL) {
	(*archp->arch_infofunc)(&vir_high, &click_shift, &click_size);
    } else {
	fatal("unknown architecture ``%s''", ai.ai_architecture);
    }

    /* Taking the default stack size by putting 0 in the process
     * descriptor only works (currently) when the machine's pagesize
     * is not bigger than the default stack size (8K).
     */
    if ((stacksize == 0) && ((1 << click_shift) > DEFSTACK)) {
	stacksize = (1 << click_shift);
	dbprintf(("architecture ``%s'' needs stack size 0x%lx\n",
		  ai.ai_architecture, stacksize));
    }

    stacksize = ((stacksize + click_size - 1) >> click_shift);
    sd->sd_offset = 0; /* unused */
    sd->sd_addr = (vir_high - stacksize) << click_shift;
    sd->sd_len = stacksize << click_shift;
    sd->sd_type = MAP_GROWDOWN | MAP_TYPEDATA | MAP_READWRITE | MAP_SYSTEM;
    dbprintf(("stack addr 0x%lx, length %ld\n", sd->sd_addr, sd->sd_len));

    /* Symbol table segment */
    if (ai.ai_sym_tab != NULL || ai.ai_str_tab != NULL) {
	++sd;
	sd->sd_offset = filloff;
	sd->sd_type = MAP_SYMTAB | MAP_NEW_SYMTAB; /* new style symtab */
	sd->sd_len = SYMSIZE(ai.ai_sym_size, ai.ai_str_size);
	dbprintf(("symbols offset %ld, addr 0x%lx, length %ld\n",
		  sd->sd_offset, sd->sd_addr, sd->sd_len));
    }
    
    /* Thread */
    td = PD_TD(&u.pd);
    td->td_state = 0;
    td->td_extra = TDX_IDLE;
    td->td_len = sizeof(thread_d) + sizeof(thread_idle);
    tdi = (thread_idle *)(td+1);
    tdi->tdi_pc = ai.ai_entrypoint;
    tdi->tdi_sp = 0;
    
    /*
    ** Concatenate process descriptor with Unix file and store result on
    ** Amoeba file server.
    */
    writefile(&u.pd, pdsize, fd, amfile, &ai);

    (void) close(fd);
    
}

void
fcopy(ifd, off, siz, ofd)
int ifd, ofd;
long off, siz;
{
#define COPYSIZ	8192
    char buf[COPYSIZ];
    int writesiz;

    dbprintf(("fcopy(%d, %ld, %ld, %d)\n", ifd, off, siz, ofd));
    if (lseek(ifd, off, 0)<0)
	fatal("seek failed");

    while (siz > 0) {
	writesiz = (siz >= COPYSIZ) ? COPYSIZ : siz;
	if (read(ifd, buf, writesiz) != writesiz)
	    fatal("short read");
	if (write(ofd, buf, writesiz) != writesiz)
	    fatal("write error");
	siz -= writesiz;
    }
}


void
write_symtab(ofd, aip)
int ofd;
struct aoutinfo *aip;
/* Convert the symbol & string table to a combined Amoeba symbol table
 * (which is in network byte order) and write it on ofd.
 */
{
    char *whole_symtab;
    long symsize = SYMSIZE(aip->ai_sym_size, aip->ai_str_size);

    dbprintf(("symtab: %ld names, stringtab: %ld bytes, total: %ld\n",
	      aip->ai_sym_size / sizeof(struct nlist),
	      aip->ai_str_size, symsize));

    /* allocate space for entire converted symbol table */
    if ((whole_symtab = (char *) malloc((size_t) symsize)) == NULL) {
	fatal("cannot allocate symbol table");
    }

    /* put it in the buffer in network byte order */
    if (buf_put_symtab(whole_symtab, whole_symtab + symsize,
		       (struct nlist *) aip->ai_sym_tab, aip->ai_sym_size,
		       aip->ai_str_tab, aip->ai_str_size) != 0) {
	fatal("cannot convert symbol table");
    }

    /* apppend it to the amoeba binary on ofd */
    if (write(ofd, whole_symtab, (int) symsize) != symsize) {
	fatal("cannot append symbol table");
    }
    free(whole_symtab);
}

/*
** Create the amoeba binary file
*/

void
writefile(pd, len, fd, amfile, ai)
process_d *	 pd;		/* In: process descriptor */
int		 len;		/* In: size of 'pd' (must fit in one trans) */
int		 fd;		/* In: file descriptor for Unix file */
char *		 amfile;	/* In: name of amoeba a.out file to produce */
struct aoutinfo	*ai;		/* In: structure with info */
{
    static char	buf[MAX_SIZE];	/* buffer for converted process descriptor */
    int		ofd;		/* File descriptor of Amoeba file */
    
    if ((ofd = creat(amfile, 0666))<0)
	fatal("Can't create %s", amfile);
    /*
    ** start by copying the process descriptor into the buffer
    ** in network byte-order.
    */
    if (buf_put_pd(buf, buf+len, pd) == (char *) 0)
	fatal("process descriptor too big");

    if (write(ofd, buf, len) != len)
	fatal("write error");

    fcopy(fd, ai->ai_txt_offset, ai->ai_txt_size, ofd);
    fcopy(fd, ai->ai_rom_offset, ai->ai_rom_size, ofd);
    fcopy(fd, ai->ai_dat_offset, ai->ai_dat_size, ofd);

    /* append symbol table if we've got one */
    if (ai->ai_sym_tab != NULL || ai->ai_str_tab != NULL) {
	write_symtab(ofd, ai);
    }

    (void) close(ofd);
}


/*VARARGS2*/
void
fatalperror(fmt, a1, a2, a3, a4)
char *	fmt;
char *	a1;
{
    perror(a1);
    fatal(fmt, a1, a2, a3, a4);
    /*NOTREACHED*/
}

/*VARARGS1*/
void
fatal(fmt, a1, a2, a3, a4)
char *	fmt;
{
    fprintf(stderr, "%s: ", progname);
    fprintf(stderr, fmt, a1, a2, a3, a4);
    fprintf(stderr, "\n");
    exit(1);
    /*NOTREACHED*/
}


/* Print usage message and exit */

void
usage()
{
    fprintf(stderr, "usage: %s [options] unixfile amoebafile architecture\n",
				progname);
    fprintf(stderr, "-b number: extra bss in kbytes [%d]\n", BSSEXTRA);
    fprintf(stderr, "-s number: stack size in kbytes [machdep]\n");
    fprintf(stderr, "-T addr:   start text segment at addr(hex)\n");
    exit(2);
    /*NOTREACHED*/
}
