/*	@(#)du.c	1.6	94/04/07 15:04:50 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>

#include "amoeba.h"
#include "capset.h"
#include "module/dgwalk.h"
#include "cmdreg.h"
#include "stderr.h"
#include "server/soap/soap.h"
#include "server/bullet/bullet.h"
#include "ampolicy.h"
#include "_ARGS.h"

#include "dir_info.h"
#include "filesvr.h"
#include "du.h"
#include "pathmax.h"

char *prog = "du";

static int
    opt_all = 0,	/* show per file size info */
    opt_local = 0,	/* restrict to local soap server */
    opt_noncumul = 0,	/* don't include accumulated sizes of subdirectories */
    opt_others = 0,
    opt_summary = 0;	/* only show total size of the arguments */

int
    opt_debug = 0,	/* debugging on */
    opt_verbose = 0;	/* be verbose */

extern char *strdup();

/* report the disk usage of "." by default: */
static char *DotPath[] = { ".", NULL };

static void
usage()
{
    fprintf(stderr, "usage: %s [-alnosv] [-f filesvr]* [path ..]\n\n",prog);
    fprintf(stderr, "-a\talso print size per file\n");
    fprintf(stderr, "-l\trestrict search to local soap server\n");
    fprintf(stderr, "-n\tdo not add size of files in subdirectories\n");
    fprintf(stderr, "-o\tadd a column summarising other fileservers\n");
    fprintf(stderr, "-s\tonly print cumulative sizes of the arguments\n");
    fprintf(stderr, "-v\talso print number of files and subdirectories\n");
    fprintf(stderr, "-f fcap\tadd a column for file server fcap\n");
    
    exit(1);
}

static void disk_usage _ARGS((char *dirs[]));

void
main(argc, argv)
int argc;
char *argv[];
{
    /* argument processing: */
    extern int getopt();
    extern int optind;
    extern char *optarg;
    int c;

    prog = argv[0];

    while ((c = getopt(argc, argv, "adlnosvf:?")) != EOF) {
	switch (c) {
	case 'a': opt_all = 1;				break;
	case 's': opt_summary = 1;			break;
	case 'v': opt_verbose = 1;			break;
	case 'n': opt_noncumul = 1;			break;
	case 'd': opt_debug = 1;			break;
	case 'l': opt_local = 1;			break;
	case 'o': opt_others = 1;			break;
	case 'f': add_file_server(optarg, optarg);	break;

	case '?':
	default : usage();				break;
	}
    }

    /*
     * add the servers from the dir containing the default bulletsvr
     */
    {
	char bulletdir[PATH_MAX];
	char *slash;

	strcpy(bulletdir, DEF_BULLETSVR);
	/* remove last component */
	if ((slash = strrchr(bulletdir, '/')) == NULL) {
	    fatal("bad default bullet server name (\"%s\")", DEF_BULLETSVR);
	}
	*slash = '\0';
	lookup_file_servers(bulletdir);
    }
	    
    if (optind < argc) {
	disk_usage(&argv[optind]);
    } else {
	disk_usage(DotPath);
    }

    exit(0);
}

static int add_file_sizes(), must_report(), can_report(), is_dir();

/*ARGSUSED*/
static void
du_object(p, object_cs, parent_cs, base_name)
register char *p, *base_name;
capset *object_cs, *parent_cs;
/*
 * Called by dirgraph module.
 */
{
    struct dir_info *parent, *dirp;

    if (object_cs == NULL) {
	/* why, o why? */
	return;
    }

    parent = search_dir(parent_cs);

    if (is_dir(object_cs)) {
	dirp = search_dir(object_cs);
	if ((dirp->dir_flags & DIR_CHILD) == 0) {
	    dirp->dir_flags |= DIR_CHILD;

	    dirp->dir_parent = parent;
	    dirp->dir_sister = parent->dir_childs;
	    parent->dir_childs = dirp;
	    dirp->dir_name = strdup(p);
	    parent->n_sub_dirs++;
	}
    } else {
	/* For the moment just assume that it is a file.
	 * If so, add_file_sizes() will return 1, in which case we 
	 * increment the parent's n_files field.
	 */
	if (add_file_sizes(p, parent, object_cs, opt_all)) {
	    parent->n_files++;
	}
    }
}

static void report_dir _ARGS((struct dir_info *dirp));

static int
find_dir(paths)
dgw_paths *paths;
{
    errstat          err;
    struct dir_info *this;
    rights_bits	     dir_rights;

    /* first check if we have sufficient rights,
     * i.e., access to at least one column.
     * This will be enough for the most common case, where bullet files
     * are created with mode "ff 4 2".
     */
    dir_rights = paths->cap.cap_priv.prv_rights;
    if ((dir_rights & SP_COLMASK) == 0) {
	verbose(opt_debug, "entry `%s' doesn't have enough rights",
		paths->entry);
	return DGW_STOP;
    }

    err = dgwexpand(paths, du_object);
    if (err == STD_NOMEM) {
	fatal("Out of memory");
    }

    this = search_dir_by_cap(&paths->cap);
    if (this == NULL) {
	/* this should never happen */
	fatal("could not find entry `%s' in dir list\n", paths->entry);
    }

    this->dir_flags |= DIR_EXPANDED;

    if (can_report(this)) {
	report_dir(this);
    }

    return DGW_OK;
}

static int
failed_dir(paths)
dgw_paths *paths;
{
    verbose(opt_debug, "failed to examine entry `%s'", paths->entry);

    return DGW_STOP;
}

static void
disk_usage(dirs)
char *dirs[];
{
    int		     i;
    dgw_params	     dgwp;
    dgw_paths       *current;
    dgw_paths	    *last;
    errstat	     err;

    static capset    null_capset;
    struct dir_info *null_dirp;

    dgwp.mode = 0;	/* i.e. ad-hoc mode */
    dgwp.testdir = (int (*)())NULL;
    dgwp.dodir = find_dir;
    dgwp.doagain = failed_dir;

    if (opt_local) {
	/* restrict the dirgraph walk to the local soap server */
	char *local_dir = "/";
	static capset soap_svr;
	static capset *serverlist[2] = { &soap_svr, NULL };

	if (sp_lookup(SP_DEFAULT, local_dir, &soap_svr) != STD_OK) {
	    fatal("cannot look up ``%s''", local_dir);
	}
	dgwp.servers = serverlist;
    } else {
	dgwp.servers = (capset **)NULL;
    }

    dgwp.entry = last = (dgw_paths *)NULL;

    null_dirp = search_dir(&null_capset);
    null_dirp->dir_flags |= DIR_NULLDIR;

    for (i = 0; dirs[i] != NULL; i++) {
	char *p = dirs[i];
	capset start_cs;

	if (sp_lookup(SP_DEFAULT, p, &start_cs) != STD_OK) {
	    fprintf(stderr, "cannot look up ``%s''\n", p);
	    continue;
	}

	if (is_dir(&start_cs)) {
	    struct dir_info *dirp;

	    if ((current = (dgw_paths *)malloc(sizeof(dgw_paths))) == NULL) {
		fatal("Out of memory");
	    }
	    if ((err = cs_to_cap(&start_cs, &current->cap)) != STD_OK) {
		fatal("cs_to_cap error");
	    }
	    current->entry = p;
	    current->next = (dgw_paths *)NULL;
	    if (last != NULL) {
		last->next = current;
	    } else {
		dgwp.entry = current;
	    }
	    last = current;

	    /* First do a `du' on the top level directories */
	    du_object(current->entry, &start_cs, &null_capset, "");
	    dirp = search_dir(&start_cs);
	    dirp->dir_flags |= DIR_ARGUMENT;
	} else {
	    /* for file arguments, just add the sizes to the
	     * (fake) null_dirp directory, and print them.
	     */
	    (void) add_file_sizes(p, null_dirp, &start_cs, 1);
	}

	cs_free(&start_cs);
    }

    if ((err = dgwalk(&dgwp)) != STD_OK) {
	fatal("Graph walk error: %s", err_why(err));
    }
}

static void
report_header()
/*
 * print the du header fields
 */
{
    int filesvr;
    static int been_here = 0;

    /* don't do it more than once */
    if (been_here) {
	return;
    } else {
	been_here = 1;
    }

    /* print file svr names */
    for (filesvr = 0; filesvr < n_file_svrs; filesvr++) {
	char *svr_name = filesvr_list[filesvr].fsvr_name;
	int len = strlen(svr_name);

	if (len >= 8) {
	    int i;

	    printf("%s\n", svr_name);
	    for (i = 0; i <= filesvr; i++) {
		printf("\t");
	    }
	} else {
	    printf("%7s|", svr_name);
	}
    }

    /* print option dependent headers */
    if (opt_others) printf(" others|");
    if (opt_verbose) printf("#fil|#sub|");

    printf("\n");
}


static void
report_dir(dirp)
struct dir_info *dirp;
/*
 * write a line containing information gathered for directory dirp
 */
{
    register int i;

    /* only report a directory once */
    if ((dirp->dir_flags & DIR_REPORTED) != 0) {
	return;
    }

    report_header();

    /*
     * Print file sizes on respective servers.  Also add the filesize
     * statitistics to the parent directory, if there is one.
     */
    for (i = 0; i <= n_file_svrs; i++) {
	long dirsize = dirp->fsizes[i],
	     cumsize = dirsize + dirp->cumfsizes[i],
	     repsize = opt_noncumul ? dirsize : cumsize;

	if (((i < n_file_svrs) || opt_others) && must_report(dirp)) {
	    if (repsize != 0) {
		printf("%7ld|", repsize);
	    } else {
		printf("       |");
	    }
	}

	if (dirp->dir_parent != NULL) {
	    /* migrate file sizes to parent directory */
	    dirp->dir_parent->cumfsizes[i] += cumsize;
	}
    }

    if (!opt_noncumul && dirp->dir_parent != NULL) {
	/* migrate file/subdir statistics to parent directory */
	dirp->dir_parent->n_files += dirp->n_files;
	dirp->dir_parent->n_sub_dirs += dirp->n_sub_dirs;
    }

    if (must_report(dirp)) {
	if (opt_verbose) printf("%4d|%4d|", dirp->n_files, dirp->n_sub_dirs);
	printf(" %s\n", dirp->dir_name);
    }

    dirp->dir_flags |= DIR_REPORTED;

    if (dirp->dir_parent != NULL && can_report(dirp->dir_parent)) {
	report_dir(dirp->dir_parent);
    }
}

#define BLOCK_SIZE	1024
#define BLOCKS(size)	(((size) + BLOCK_SIZE-1) / BLOCK_SIZE)

static int
add_file_sizes(name, parent, object, printit)
char *name;
struct dir_info *parent;
capset *object;
int printit;
/*
 * Get filesize information, and add them to the parent directory.
 * The paremeter printit tells whether the file size info should be shown.
 * Returns 1 when at least one b_size() call succeeded, else 0.
 */
{
    register int i;
    int ok = 0;		    /* return value */
    interval old;
    static long *file_sizes = NULL; /* sizes of file on the various servers */

    /*
     * allocate/initialise file_sizes array.
     */
    if (file_sizes == NULL) {
	file_sizes = (long *)calloc((size_t)(n_file_svrs + 1), sizeof(long));
	if (file_sizes == NULL) {
	    fatal("out of memory");
	}
    }
    for (i = 0; i <= n_file_svrs; i++) {
	file_sizes[i] = -1;	/* not 0, in order to recognise empty files */
    }

    /*
     * get file sizes using b_size on valid caps in the capset
     */
    old = timeout(SMALL_TIMEOUT);
    for (i = 0; i < object->cs_final; i++) {
	if (object->cs_suite[i].s_current) {
	    capability *filecap = &object->cs_suite[i].s_object;
	    int fsvr = file_svr_index(filecap);
	    
	    if (fsvr < n_file_svrs || opt_others) {
		if (cache_cap(filecap) == 1) {
		    /* We haven't encountered this object before */
		    b_fsize size;
		    errstat err = b_size(filecap, &size);

		    if (err == STD_OK) {
			file_sizes[fsvr] = BLOCKS(size);
			ok = 1;
		    } else {
			verbose(opt_debug,
				"%s: could not get size of replica #%d (%s)",
				name, i, err_why(err));
		    }
		}
	    }
	}
    }
    (void) timeout(old);

    if (ok) {
	/*
	 * add the sizes to the parent directory
	 * and report them when printit is true.
	 */
	report_header();

	for (i = 0; i <= n_file_svrs; i++) {
	    b_fsize size = file_sizes[i];

	    if (size >= 0) {
		parent->fsizes[i] += size;
	    }

	    if (printit && ((i < n_file_svrs) || opt_others)) {
		if (size >= 0) {
		    printf("%7ld|", size);
		} else {
		    printf("       |");
		}
	    }
	}
	if (printit) {
	    if (opt_verbose) {
		printf("    |    |");
	    }
	    printf(" %s\n", name);
	}
    }

    return ok;
}

static int
must_report(dirp)
struct dir_info *dirp;
/*
 * Return 1 if directory dirp should be reported, else 0.
 */
{
    if ((dirp->dir_flags & DIR_NULLDIR) != 0) {
	return 0;	/* as this is a phoney directory */
    } else {
	/* when opt_summary is on, only print the directory if it was
	 * given as argument:
	 */
	return !opt_summary || ((dirp->dir_flags & DIR_ARGUMENT) != 0);
    }
}

static int
can_report(parent)
struct dir_info *parent;
/*
 * Return 1 if we've gathered enough information of directory "parent"
 * in order to print it.
 */
{
    register struct dir_info *dirp;


    /* check that it has been fully expanded already */
    if ((parent->dir_flags & DIR_EXPANDED) == 0) {
	return 0;
    }

    /*
     * Also make sure all child directories have been reported,
     * which implies that their file sizes have bin added to parent.
     */
    for (dirp = parent->dir_childs; dirp != NULL; dirp = dirp->dir_sister) {
	if ((dirp->dir_flags & DIR_REPORTED) == 0) {
	    return 0;
	}
    }

    /* passed all checks */
    return 1;
}

static int
is_dir(object_cs)
capset *object_cs;
{
    /*
     * This is in fact a slightly modified _ajax_isdir().
     */
    capset x_capset;	/* we'll try to look up the dummy name "x" */
    interval savetout;	/* to avoid hanging on nonresponding server */
    errstat err;

    savetout = timeout(SMALL_TIMEOUT);
    err = sp_lookup(object_cs, "x", &x_capset);
    if (err == STD_OK) {
	cs_free(&x_capset);
    }
    (void) timeout(savetout);
    return (err == STD_OK) || (err == STD_NOTFOUND);
}

/*
 * Various varargs printing routines
 */

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#ifdef __STDC__
void fatal(char *format, ...)
{
    va_list ap; va_start(ap, format);
#else
/*VARARGS1*/
void fatal(format, va_alist) char *format; va_dcl
{
    va_list ap; va_start(ap);
#endif

    vfprintf(stderr, format, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

#ifdef __STDC__
void verbose(int doprint, char *format, ...)
{
    va_list ap; va_start(ap, format);
#else
/*VARARGS2*/
void verbose(doprint, format, va_alist) int doprint; char *format; va_dcl
{
    va_list ap; va_start(ap);
#endif

    if (doprint) {
	vfprintf(stdout, format, ap);
	fprintf(stdout, "\n");
    }
    va_end(ap);
}

