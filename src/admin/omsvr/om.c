/*	@(#)om.c	1.9	96/02/27 10:17:26 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 *    om  -  an object manager for replication and garbage collection
 *
 *
 * Takes a list of pathnames as args or, if no args,
 * reads standard input for a list of path names.  Touchs and/or replicates
 * each specified object.  Replication is done by consulting a file that
 * specifies one or more sets of equivalent servers (e.g., a set of Bullet
 * servers).  It is assumed that the servers in each set are to be used to
 * replicate any object that is owned by one of the servers in the set.  The
 * ports of these equivalent servers are called a replication port-set.
 *
 * For each object whose capability set contains a port that is in one of the
 * replication port-sets, the object is replicated (if necessary) by using
 * STD_COPY to the ports in the replication port-set that are not yet in the
 * object's capability set.
 *
 * In the process, every specified object, whether replicated or not, is
 * touched, so this program is suitable for use in the regular touching/aging
 * process, to perform garbage collection.
 */

#include <amoeba.h>
#include <capset.h>
#include <cmdreg.h>
#include <stderr.h>
#include <stdcom.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <ampolicy.h>
#include <server/soap/soap.h>
#include <server/bullet/bullet.h>
#include <module/dgwalk.h>
#include <module/goodport.h>
#include <module/name.h>
#include <module/tod.h>
#include <module/rnd.h>
#include <module/prv.h>
#include <module/ar.h>
#include <thread.h>
#include <stdlib.h>
#include <string.h>
#define _POSIX_SOURCE
#include <limits.h>

#if !defined(PATH_MAX)
#define	PATH_MAX	255
#endif

/* Default name of file in which `om' super capability is stored */
#define		DEF_OMCAP	"om"

/* Default name of file in which the data is stored */
#define		DEF_OMDATA	"startup"

/* If no -t is specified with -s then mintouch defaults to this */
#define		DEF_MINTOUCH	10000

#define NILCAP (capability *)NULL

#define STACKSIZE 8192
#define MAX_SET_SIZE 10
unsigned int    num_repl_sets;
unsigned int    max_repl_sets;
struct Repl_Set {
    unsigned int size;
    capability   server[MAX_SET_SIZE];
    char *       path[MAX_SET_SIZE];
}	*repl_set;

unsigned int    num_toage;
unsigned int    max_toage;
unsigned int    num_soaps;
unsigned int    max_soaps;
unsigned int    num_prefs;
unsigned int    max_prefs;
unsigned int    num_touch;
unsigned int    max_touch;
struct Server_Set {
    capability   server;
    char *       path;
}	*age, *soaps, *prefs,*touchlist;

capset		**soaplist;

int		touch_all=1;

/* The admin data to read the startup file */
#define	BLKSIZE	1024
capability start_cap ;
b_fsize	start_readpos;
int	start_curpos, start_lastpos ;
int	start_eof ;
char	start_blk[BLKSIZE] ;

/* The admin block for comparisons */
/* These point to the file buffers */
#define	CMPSIZE	32768
char	*cmp_f1, *cmp_f2;
size_t	cmp_size= CMPSIZE;

/* Flags and arguments: */
int	debug = 0;			/* -x flag */
int	remove_replicas = 0 ;		/* -d flag */
int	force_replicas = 0;		/* -f flag */
int	recursive = 0;			/* -r flag */
int	server_mode = 0;		/* -s flag */
int	delay_between_start_of_passes = 0;	/* -s flag argument */
long	min_touch = 10;			/* -t flag argument */
int	verbose = 0;			/* -v flag */
int	check_replicas = 0;		/* -c and -C flag */
char	*startfile=DEF_OMDATA;
char	*startpath;			/* -i flag */
char	*capfile=DEF_OMCAP;		/* -p flag */
int	capsrc;				/* 0=>default, 1=>params, 2=>startup */
char	*progname;
char	**pathlist;			/* Used when pathnames given as args. */
char	**global_argv;			/* Globally available copy of argv */
int	use_pathlist;

#ifdef NTOUCH_DEBUG
#define ntouch_verbose	1
#else
#define ntouch_verbose	verbose
#endif

/* Getopt interface */
extern int	optind;
extern char *	optarg;

/* Statistics: */
int number_passes = 0;
long replicas_added = 0;
long replicas_replaced = 0;
long replicas_removed = 0;
long replicas_touched = 0;
long replicas_n_touched = 0;
long entries_changed = 0;
long entries_reordered = 0;
long objects_checked = 0;
long replicas_read = 0;
long objects_ok = 0;
long objects_repl = 0;
long objects_single = 0;
long objects_notok = 0;
long suites_notok = 0;
long installs_clashed = 0;
long secs_last_pass = 0;
int  last_pass = 0;
int last_aging = 0;

/* Status: */
char *state_fmt;
int	stopit=0 ;

/* forward definition */
long		getsecs();
void		setpref();
void		pr_elapsed();


usage()
{
    fprintf(stderr, "\nUsage: %s [ -dfrvc ] [-i startup] [ -s delay ] [ -t min_touch ]\n",
								progname);
    fprintf(stderr, "%s%s%s%s%s%s%s%s%s%s%s%s",
	"\t-x\t\tdebug mode\n",
	"\t-f\t\tforce full replication\n",
	"\t-d\t\tdelete superfluous replicas\n",
	"\t-r\t\trecursively descend through directories\n",
	"\t-s delay\tserver mode: touch/replicate; age; delay; repeat\n",
	"\t-t min_touch\tminimum number objects to touch, else abort\n",
	"\t-v\t\tverbose mode: report each touch/replication\n",
	"\t-c\t\tcheck whether replicas belong to same replica set\n",
	"\t-C\t\tcheck whether replicas are identical\n",
	"\t-p path\t\tpath name under which server capability is stored\n",
	"\t-b server\treorder capsets to get preferred servers\n",
	"\t-i file\t\tuse alternative startup file\n"
    );
    exit(2);
}

/* Forward declarations of functions in this file: */
void		do_one_object();
int		visit_dir();
void	 	touch();
void 		replicate();
char 		*next_path();
void 		server_loop();
errstat		touch_flush();

main(argc, argv)
    int argc;
    char **argv;
{
    int		i;
    char	*path;
    capset	object_cs;
    char	opt;
    errstat	err;
    dgw_params	dgwp;
    long	start_time;
    long	end_time;
    dgw_paths	*current;
    dgw_paths	*last;
    long	t_arg= -1;


    progname = argv[0];
    global_argv = argv;

    while ((opt = getopt(argc, argv, "xdfrs:t:vi:cCp:b:")) != EOF) {
	switch (opt) {
	    case 'x':
		debug++;
		break;
	    case 'f':
		force_replicas = 1;
		break;
	    case 'd':
		remove_replicas = 1;
		break;
	    case 'r':
		recursive = 1;
		break;
	    case 's':
#ifdef UNIX
		fprintf(stderr, "%s: server mode not allowed under UNIX\n",
								    progname);
		exit(2);
#else
		server_mode = 1;
		delay_between_start_of_passes = atoi(optarg);
		if (!isdigit(*optarg) || delay_between_start_of_passes < 1) {
		    fprintf(stderr,
		    "%s: delay specified with -s must be positive int\n",
				progname);
		    exit(2);
		}
		break;
#endif
	    case 't':
		if ( t_arg!= -1 ) usage();
		t_arg = atoi(optarg);
		if (!isdigit(*optarg) || t_arg < 1) {
		    fprintf(stderr,
		    "%s: minimum specified with -t must be positive int\n",
				progname);
		    exit(2);
		}
		break;
	    case 'v':
		verbose = 1;
		break;
	    case 'i':
		startpath =optarg ;
		break;
	    case 'p':
		capfile =optarg ;
		capsrc=1;
		break;
	    case 'b':
		setpref(optarg) ;
		break;
	    case 'c':
		if ( check_replicas ) usage() ;
		check_replicas =1 ;
		break;
	    case 'C':
		if ( check_replicas ) usage() ;
		check_replicas =2 ;
		break;
	    default:
		usage();
		/*NOTREACHED*/
	}
    }

    if (optind < argc) {
	pathlist = argv + optind;
	use_pathlist = 1;
    }

    if ( (force_replicas || remove_replicas || max_prefs) && check_replicas )
	usage() ;

    if ( capsrc && !server_mode ) usage() ;
    if ( t_arg!= -1 ) {
	if ( !server_mode ) usage();
	min_touch=t_arg ;
    } else { 
	/* No -t option was given, if server mode then aging will happen
	 * so we set a "safe" minimum to protect the gullible.
	 */
	if (server_mode)
	    min_touch = DEF_MINTOUCH;
    }
    if (!get_startup())
	exit(1);
    if (server_mode || verbose)
	print_startup(stdout);

    /* Limit timeout to 2 secs: */
    (void)timeout((interval)2000);

    state_fmt="Initializing\n";

    /* Allocate replica checking space at the start */
    if ( check_replicas ) {
	cmp_f1 = (char *) malloc(cmp_size) ;
	cmp_f2 = (char *) malloc(cmp_size) ;
	if ( !cmp_f1 || !cmp_f2 ) {
	    fprintf(stderr,"%s: Not enough memory\n",progname);
	    exit(1) ;
	}
    }

    /* Start a thread taking info requests: */
    if (server_mode &&
		    !thread_newthread(server_loop, STACKSIZE, (char *)0, 0)) {
	fprintf(stderr, "%s: failed to start server thread.\n", progname);
	exit(1);
    }

    err = STD_OK;
    dgwp.mode=DGW_ALL;
    dgwp.dodir=visit_dir;
    dgwp.testdir=NULL;
    dgwp.doagain=NULL;
    dgwp.servers=soaplist;
    do {
	++number_passes;
	stopit=0;
	replicas_added =
	replicas_replaced =
	replicas_removed =
	replicas_touched =
	replicas_n_touched =
	entries_reordered =
	entries_changed = 0;
	state_fmt="Active In Pass:            %6d\n";

	start_time=getsecs();

	if (recursive) {
	    dgwp.entry=last= (dgw_paths *)0;
	    while ((path = next_path(&object_cs)) != NULL && !stopit ) {
		current=(dgw_paths *)malloc(sizeof(dgw_paths));
		if ( !current ) {
		    (void)fprintf(stderr, "%s: Out of memory\n",
				progname);
		    exit(1);
		}
    		err=cs_to_cap(&object_cs,&current->cap);
		cs_free(&object_cs);
		if ( err!=STD_OK ) {
		    (void)fprintf(stderr, "%s: Error \"%s\" while accessing %s\n",
				progname,err_why(err),path);
		    exit(1);
		}
    		current->entry=path;
		current->next= (dgw_paths *)0;
		if ( last ) last->next=current ; else dgwp.entry=current ;
		last=current;
	    }
	    err=dgwalk(&dgwp);
	    for ( current=dgwp.entry ; current ; current=last ) {
		last=current->next;
		free((_VOIDSTAR) current);
	    }
	    if ( stopit && err==STD_INTR ) {
		break ;
	    }
    	    if (err!=STD_OK) {
		(void)fprintf(stderr, "%s: dg_walk error \"%s\"\n",
				progname,err_why(err));
		server_mode = 0;	/* Abort touching/aging loop */
		break;
	    }
	} else {
		while ((path = next_path(&object_cs)) != NULL && !stopit ) {
		    do_one_object(path, &object_cs,(capset *)NULL,path);
		    cs_free(&object_cs);
		}
	}
	if ((err = touch_flush(1)) != STD_OK) {
	    if (ntouch_verbose) {
		fprintf(stderr, "%s: touch_flush failed (%s)",
			progname, err_why(err));
	    }
	}
	(void) fflush(stderr);
	(void) fflush(stdout);

	end_time=getsecs();
	secs_last_pass=end_time-start_time;
	if ( end_time && start_time &&
	     secs_last_pass>=0 ) last_pass=number_passes; else last_pass=0 ;
	/* Print statistics: */
	if (verbose || server_mode) {
	    printf("----------\n");
	    if (server_mode)
		printf("End of Pass:               %6ld\n", number_passes);
	    if (last_pass) pr_elapsed(stdout,secs_last_pass);
	    if ( check_replicas ) {
		printf("Objects Checked:           %6ld\n", objects_checked);
		printf("Replicated Objects:        %6ld\n", objects_repl);
		if ( check_replicas>1 ) {
		    printf("Replicas Read:             %6ld\n", replicas_read);
		    printf("Usable Replicated Objects: %6ld\n", objects_ok);
		}
		printf("Not Replicated Objects:    %6ld\n", objects_single);
		printf("Suspect Objects:           %6ld\n", objects_notok);
		printf("Inconsistent Suites:       %6ld\n", suites_notok);
	    } else {
		printf("Missing Replicas Added:    %6ld\n", replicas_added);
		printf("Invalid Replicas Replaced: %6ld\n", replicas_replaced);
		printf("Bad-Port Replicas Removed: %6ld\n", replicas_removed);
		printf("Install Clashes (total):   %6ld\n", installs_clashed);
		if ( prefs ) {
		    printf("Dir. Entries Reordered:    %6ld\n", entries_reordered);
		}
		printf("Directory Entries Changed: %6ld\n", entries_changed);
	    }
	    printf("Replicas Touched:          %6ld\n", replicas_touched);
	    printf("Touched with std_ntouch:   %6ld\n", replicas_n_touched);
	    (void) fflush(stdout);
	}
	/* A little safety check, to ensure that we don't keep aging, if
	 * touching is failing:
	 */
	if (server_mode ) {
	    if ( !stopit && num_toage && replicas_touched >= min_touch ) {
		state_fmt="Aging After Pass:	   %6d\n";
		for (i = 0; i < num_toage; i++) {
		    if ((err = std_age(&age[i].server)) == STD_OK) {
			if (verbose)
			    fprintf(stderr, "std_age: %s\n", age[i].path);
		    } else {
			fprintf(stderr, "%s: std_age %s failed (%s)\n",
						progname, age[i].path,
						err_why(err));
		    }
		}
		last_aging=number_passes;
	    } else if ( stopit ) {
		/* Was stopped, don't age */
		fprintf(stderr,"%s: pass %d was stopped, no aging done\n",
						progname,number_passes);
	    } else if ( num_toage ) {
		/* replicas_touched<min_touch must be true */
		fprintf(stderr, "%s: less than %d objects touched; no aging done.\n",
						progname, min_touch);
	    }
		
	    if (!reset_paths()) {
		    (void)fprintf(stderr, "%s: reset_paths failed\n", progname);
		    exit(1);
	    }
	    state_fmt="Sleeping After Pass:       %6d\n";
	    end_time=getsecs() ;
	    if ( delay_between_start_of_passes>end_time-start_time )
	    	(void) sleep((unsigned)(delay_between_start_of_passes-(end_time-start_time)));
	}
    } while (server_mode);

    exit(err != STD_OK);
}

static int NTouch_failures = 0;
static int NTouch_last_failed = -2;
static int Show_parent = 0;

/*ARGSUSED*/
void
show_one_parent(path, object_cs, parent_cs, entry)
	char		*path;
	capset		*object_cs;
	capset		*parent_cs;
	char		*entry;
{
    static char parentbuf[PATH_MAX];

    if (Show_parent) {
	char *last_slash = strrchr(path, '/');

	if (last_slash != NULL) {
	    int parent_len = last_slash - path;

	    if (parent_len >= sizeof(parentbuf)) {
		parent_len = sizeof(parentbuf) - 1;
	    }
	    strncpy(parentbuf, path, parent_len);
	    parentbuf[parent_len] = '\0';
	    fprintf(stderr, "in directory: %s\n", parentbuf);
	}

	Show_parent = 0;
    }
}

int
visit_dir(paths)
	dgw_paths *paths;
{
	dgw_paths *cur;
	errstat	  err;

	if ( stopit ) return DGW_QUIT;
	/* We want at least MODRGT */
	for ( cur=paths ; cur ; cur=cur->next ) {
		if ( cur->cap.cap_priv.prv_rights&SP_MODRGT || !cur->next ) {
			err=dgwexpand(paths,do_one_object);
			if ( err==STD_NOMEM ) {
				fprintf(stderr,"%s: Out of memory\n",progname);
				exit(3);
			}
			break ;
		}
	}

	/* If std_ntouch failed in the previous round, we do the
	 * std_ntouch()es for the complaining servers per directory,
	 * so that we can provide a sensible diagnostic.
	 */
	if ((err = touch_flush(0)) != STD_OK) {
	    fprintf(stderr, "%s: touch_flush failed (%s)\n",
		    progname, err_why(err));
	    Show_parent = 1;
	    (void) dgwexpand(paths, show_one_parent);
	}

	return DGW_OK;
}


/*ARGSUSED*/
void
do_one_object(path, object_cs,parent_cs,entry)
	char		*path;
	capset		*object_cs;
	capset		*parent_cs;
	char		*entry;
{
    int			done_repl;
    int			i, j;
    capset		copy_cset;

    if ( !object_cs ) return ;
    if ( check_replicas ) {
	for (i = 0; i < num_repl_sets; i++) {
    	    for (j = 0; j < repl_set[i].size; j++) {
    		if (debug > 0)
    		    fprintf(stderr, "any port in cs %x == port of %s?\n",
    				    object_cs, repl_set[i].path[j]);
    		if ( member_port(object_cs,
    			 &repl_set[i].server[j].cap_port)) {
		    /* NOTE: This will touch everything as well */
		    do_check(path,object_cs,&repl_set[i]);
    		    return ;
    		}
	    }
	}
	return ;
    }
    /* If any of the capabilities in the object set have a port
     * that is also in one of the replication sets, ensure that
     * the object is fully replicated and touched:
     */
    done_repl = 0;
    for (i = 0; i < num_repl_sets; i++) {
	for (j = 0; j < repl_set[i].size; j++) {
	    if (debug > 0)
		    fprintf(stderr, "any port in cs %x == port of %s?\n",
				    object_cs, repl_set[i].path[j]);
	    if (member_port(object_cs,
			 &repl_set[i].server[j].cap_port)) {
		/* NOTE: This will touch everything as well */
		if ( !cs_copy(&copy_cset,object_cs) ) {
			fprintf(stderr,"%s: Out of memory\n",progname) ;
			exit(3);
		}
		replicate(path, &copy_cset, parent_cs, entry, &repl_set[i]);
		cs_free(&copy_cset);
		done_repl = 1;
		break;
	    }
	}
	if (done_repl) break;
    }
    if (!done_repl) {		/* We didn't replicate, so touch: */
	if ( do_order(object_cs) )
	    entr_replace(parent_cs,entry,object_cs,object_cs);	    
	if ( num_touch || touch_all ) touch(path, object_cs);
    }
}

/*
 * Get next pathname and associated capset from the arg list or from stdin:
 */
char *
next_path(cs)
    capset *cs;
{
    char *path; 
    static char buff[512];
    errstat status;
    do {
	if (use_pathlist) {
	    if (!*pathlist)
		return NULL;
	    path = *pathlist++;
	} else {
	    int len;

	    if (fgets(buff, sizeof buff, stdin) == NULL)
		return NULL;
	    path = buff;

	    /* Eliminate trailing newline: */
	    len = strlen(path);
	    if (path[len-1] == '\n')
		    path[len-1] = '\0';
	}

	status = sp_lookup(SP_DEFAULT, path, cs);
	if (status != STD_OK) {
	    fprintf(stderr, "%s: cannot lookup %s (%s)\n",
		    progname, path, err_why(status));
	    if (status == RPC_NOTFOUND) {
		fprintf(stderr, "%s: panic -- no directory server!\n", path);
		exit(2);
	    }
	}
    } while (status != STD_OK);
    if (debug > 0)
	fprintf(stderr, "next_path -> %s\n", path);
    return path;
}

/*
 * Reset list of paths, in server mode.  Requires that the list of paths
 * be specified as arguments to the program, or that stdin be a seekable
 * file:
 */
int
reset_paths()
{
    if (use_pathlist) {
	pathlist = global_argv + optind;
    } else if (fseek(stdin, 0L, 0) < 0) {
	return 0;
    }
    return 1;
}

/* Do any of the caps in the capset have the same port as the given port?
 */
int
member_port(cs, portp)
    capset *cs;
    port *portp;
{
    int i;
    suite *s;
    for (i = 0; i < cs->cs_final; i++) {
	s = &cs->cs_suite[i];
	if (s->s_current && PORTCMP(&s->s_object.cap_port, portp)) {
	    return 1;
	}
    }
    return 0;
}

int
member_servers(cap, servset)
    capability		*cap;
    struct Repl_Set	*servset;
{
    int i;

    for (i = 0; i < servset->size; i++) {
	if ( PORTCMP(&cap->cap_port, &servset->server[i].cap_port)) {
	    return 1;
	}
    }
    return 0;
}

void
add_cap(object_cs,new_cap)
    capset *object_cs;
    capability *new_cap;
{
    int		i;
    suite	*new_suite; 

    for (i = 0; i < object_cs->cs_final; i++) {
	if (!object_cs->cs_suite[i].s_current) {
	    object_cs->cs_suite[i].s_current = 1;
    	    object_cs->cs_suite[i].s_object = *new_cap;
	    replicas_added++;
	    return ;
	}
    }
    new_suite = (suite *)realloc(
			    (_VOIDSTAR)(object_cs->cs_suite),
			    (size_t) (object_cs->cs_final+1) * sizeof(suite));
    if (!new_suite) {
	fprintf(stderr, "%s: Out of memory in replicate.\n",
				progname);
	exit(2);
    }
    new_suite[object_cs->cs_final].s_current = 1;
    new_suite[object_cs->cs_final].s_object = *new_cap;
		    replicas_added++;
    object_cs->cs_final++;
    object_cs->cs_suite = new_suite;
    return ;
}

/* replace an entry */
entr_replace(parent_cs,name,old_cs,new_cs)
	capset	*parent_cs;
	char	*name;
	capset	*old_cs;
	capset	*new_cs;
{
    sp_entry	ent;
    capability	*cap_ptr;
    int		i;
    errstat	err;

    ent.se_name=name;
    ent.se_capset= *parent_cs;
    for (i = 0; i < old_cs->cs_final; i++) {
	if (old_cs->cs_suite[i].s_current) {
    	    cap_ptr= &old_cs->cs_suite[i].s_object;
	    break;
	}
    }
    err = sp_install(1,&ent,new_cs,&cap_ptr);
    switch(err) {
    case RPC_NOTFOUND:
    case SP_UNAVAIL:
	fprintf(stderr, "%s: panic -- no directory server!\n", name);
	exit(2);
    case SP_CLASH:
	/* Simply forget about it, reordering will come next time */
	installs_clashed++;
	break;
    case STD_OK:
	entries_changed++;
	entries_reordered++;
	if ( verbose ) {
	    fprintf(stderr, "reordered %s: %d\n",name,entries_reordered);
	}
	break;
    default:
	if (verbose)
	    fprintf(stderr, "%s: sp_install failed (%s)\n",
					name, err_why(err));
	break;
    }
}

get_startup()
{
	char	**terms;
	int	result;
	int	i, j, k, l;
	errstat status;
	char	spath[257];

	if ( !startpath ) {
		/* No runtime argument, paste path together */
		if ( *startfile=='/' ) {
			startpath=startfile;
		} else {
			sprintf(spath,"%s/%s",DEF_OMDATADIR,startfile);
			startpath=spath;
		}
	}
	status=name_lookup(startpath,&start_cap) ;
	if ( status!=STD_OK ) {
		fprintf(stderr,"%s: cannot lookup startupfile \"%s\"\n",
			progname, startpath);
		return 0 ;
	}
	while( (result=getterms(&terms))==1 ) {
		if ( !*terms ) continue ; /* skip empty lines */
		if ( strcmp(*terms,"replicas")==0 ) {
			if ( !get_replicas(terms) ) return 0 ;
		} else
		if ( strcmp(*terms,"aging")==0 ) {
			if ( !get_aging(terms) ) return 0 ;
		} else
		if ( strcmp(*terms,"soap_servers")==0 ) {
			if ( !get_soaps(terms) ) return 0 ;
		} else
		if ( strcmp(*terms,"publish")==0 ) {
			if ( !get_capfile(terms) ) return 0 ;
		} else
		if ( strcmp(*terms,"touch")==0 ) {
			touch_all=0; /* restrict touching */
			if ( !get_touch(terms) ) return 0 ;
		} else
		{	fprintf(stderr,
				"%s: unknown keyword \"%s\" in startupfile\n",
				progname,*terms);
			return 0 ;
		}
	}
	if ( result== -1 ) return 0 ; /* W've seen an error */

	/* Check availability */
	for (i = 0; i < num_repl_sets; i++) {
		for (j = 0; j < repl_set[i].size; j++) {
			status = name_lookup(repl_set[i].path[j],
					&repl_set[i].server[j]);
			if (status != STD_OK) {
				fprintf(stderr,
					"%s: cannot lookup %s (%s)\n",
					progname, repl_set[i].path[j],
					err_why(status));
				return 0;
			}
		}
	}
	/* Check consistency */
	for (i = 0; i < num_repl_sets; i++) {
		for (j = 0; j < repl_set[i].size; j++) {
			for (k = i+1; k < num_repl_sets; k++) {
				for (l = 0; l < repl_set[k].size; l++) {
					if ( PORTCMP(&repl_set[i].server[j].cap_port,
						     &repl_set[k].server[l].cap_port)){
fprintf(stderr,"%s: warning, two different replication sets contain capabilities\n",progname);
fprintf(stderr,"     for the same port (%s and %s)\n",repl_set[i].path[j],
		repl_set[k].path[l]);
					}
				}
			}
		}
	}

	if ( num_soaps ) {
		soaplist=(capset **)malloc((num_soaps + 1)*sizeof(capset *)) ;
		if ( !soaplist ) {
			fprintf(stderr,"%s: Out of memory in get_startup\n",progname);
			return 0 ;
		}
		soaplist[num_soaps]= (capset *)0;
		for (i = 0; i < num_soaps; i++)  {
			capset *next ;
			next=(capset *)malloc(sizeof(capset)) ;
			if ( !next ) {
				fprintf(stderr,"%s: Out of memory in get_startup\n",progname);
				return 0 ;
			}
			status = sp_lookup(SP_DEFAULT,soaps[i].path, next);
			if (status != STD_OK) {
				fprintf(stderr,
					"%s: cannot lookup %s (%s)\n",
					progname, soaps[i].path, err_why(status));
				return 0;
			}
			soaplist[i]=next;
		}
	}
	if ( server_mode ) {
		/* Servers that should be told to age everything (requires access
		 * to the /super directory, as it certainly should: */
		for (i = 0; i < num_toage; i++)  {
			status = name_lookup(age[i].path, &age[i].server);
			if (status != STD_OK) {
				fprintf(stderr,
					"%s: cannot lookup %s (%s)\n",
					progname, age[i].path, err_why(status));
				return 0;
			}
		}
	}
	for (i = 0; i < num_touch; i++)  {
		status = name_lookup(touchlist[i].path, &touchlist[i].server);
		if (status != STD_OK) {
			fprintf(stderr,
				"%s: cannot lookup %s (%s)\n",
				progname, touchlist[i].path, err_why(status));
			return 0;
		}
	}

	return 1;
}

int get_replicas(terms)
	char **terms ;
{
	char	**next ;
	char	*cmem ;
	int	i ;
	int	temp ;

	if ( num_repl_sets>=max_repl_sets ) {
		temp=max_repl_sets+50 ;
		repl_set=(struct Repl_Set *)(max_repl_sets?
			    realloc((_VOIDSTAR) repl_set,sizeof(*repl_set)):
			    malloc((size_t) temp*sizeof(*repl_set))
			 ) ;
		if ( !repl_set ) {
			fprintf(stderr,"%s: Out of memory\n", progname) ;
			num_repl_sets=max_repl_sets=0 ;
			return 0 ;
		}
		max_repl_sets=temp ;
	}

	for ( i=0, next=terms+1 ; *next ; next++ ) {
		if ( i>=MAX_SET_SIZE ) {
			fprintf(stderr,"%s: replication set is too large (>%s)\n",
				progname,MAX_SET_SIZE);
			return 0 ;
		}
		cmem = (char *) malloc(strlen(*next) + 1) ;
		if ( !cmem ) {
			fprintf(stderr,"%s: Out of memory\n", progname) ;
			return -1 ;
		}
		(void) strcpy(cmem,*next) ;
		repl_set[num_repl_sets].path[i++]=cmem ;
	}
	if ( i ) {
		repl_set[num_repl_sets].size=i ;
		num_repl_sets++ ;
	}
	return 1 ;
}

int get_aging(terms)
	char **terms ;
{
	char	**next ;
	char	*cmem ;
	int	temp ;

	for ( next=terms+1 ; *next ; next++ ) {
		if ( num_toage>=max_toage ) {
			temp=max_toage+50 ;
			age=(struct Server_Set *)(max_toage?
			    realloc((_VOIDSTAR) age,(size_t) temp*sizeof(*age)):
			    malloc((size_t) temp*sizeof(*age))
				 ) ;
			if ( !age ) {
				fprintf(stderr,"%s: Out of memory\n",
					progname) ;
				num_toage=max_toage=0 ;
				return 0 ;
			}
			max_toage=temp ;
		}

		cmem = (char *) malloc(strlen(*next) + 1) ;
		if ( !cmem ) {
			fprintf(stderr,"%s: Out of memory\n", progname) ;
			return -1 ;
		}
		(void) strcpy(cmem,*next) ;
		age[num_toage++].path=cmem ;
	}
	return 1 ;
}

int get_touch(terms)
	char **terms ;
{
	char	**next ;
	char	*cmem ;
	int	temp ;

	for ( next=terms+1 ; *next ; next++ ) {
		if ( num_touch>=max_touch ) {
			temp=max_touch+50 ;
			touchlist=(struct Server_Set *)(max_touch?
			    realloc((_VOIDSTAR) touchlist,
					    (size_t) temp*sizeof(*touchlist)):
			    malloc((size_t) temp*sizeof(*touchlist))
			     ) ;
			if ( !touchlist ) {
				fprintf(stderr,"%s: Out of memory\n",
					progname) ;
				num_touch=max_touch=0 ;
				return 0 ;
			}
			max_touch=temp ;
		}

		cmem = (char *) malloc(strlen(*next) + 1) ;
		if ( !cmem ) {
			fprintf(stderr,"%s: Out of memory\n", progname) ;
			return -1 ;
		}
		(void) strcpy(cmem,*next) ;
		touchlist[num_touch++].path=cmem ;
	}
	return 1 ;
}

int get_soaps(terms)
	char **terms ;
{
	char	**next ;
	char	*cmem ;
	int	temp ;

	for ( next=terms+1 ; *next ; next++ ) {
		if ( num_soaps>=max_soaps ) {
			temp=max_soaps+50 ;
			soaps=(struct Server_Set *)(max_soaps?
				    realloc((_VOIDSTAR) soaps,
					    (size_t) temp*sizeof(*soaps)):
				    malloc((size_t) temp*sizeof(*soaps))
				 ) ;
			if ( !soaps ) {
				fprintf(stderr,"%s: Out of memory\n",
					progname) ;
				num_soaps=max_soaps=0 ;
				return 0 ;
			}
			max_soaps=temp ;
		}

		cmem = (char *) malloc(strlen(*next) + 1) ;
		if ( !cmem ) {
			fprintf(stderr,"%s: Out of memory\n", progname) ;
			return -1 ;
		}
		(void) strcpy(cmem,*next) ;
		soaps[num_soaps++].path=cmem ;
	}
	if ( num_soaps==0 ) {
		fprintf(stderr,"%s: Warning: the set of directory servers is empty(ignored)\n");
	}
	return 1 ;
}

int get_capfile(terms)
	char **terms ;
{
	char	**next ;
	char	*cmem ;

	for ( next=terms+1 ; *next ; next++ ) {
		if ( strlen(*next)==0 ) continue ;
		cmem = (char *) malloc(strlen(*next) + 1) ;
		if ( !cmem ) {
			fprintf(stderr,"%s: Out of memory\n", progname) ;
			return -1 ;
		}
		(void) strcpy(cmem,*next) ;
		switch( capsrc) {
		case 0 :
			/* We still have the default cap */
			capfile=cmem;
			capsrc=2;
			break;
		case 1 :
			/* Was specified at command line, ignore this entry */
			break;
		case 2 :
			/* Already specified in startup file */
			fprintf(stderr,"%s: Warning: double specification of publish file (%s)\n",
				progname,next);
			break;
		}
	}
	if ( capsrc==0 ) {
		fprintf(stderr,"%s: Warning: illegal empty publish line\n",
			progname);
		return 1;
	}
		
	return 1 ;
}

/* Read a file, do '\' processing, delete comment started by # and
   split into tokens separated by whitespace
   The storage reachable through terms are released at every new call
   to getterms
   returns:
	-1 on error
	 0 on eof
	 1 if ok
*/
#define Starttoken(c) { *tokenp++=linep ; *linep++=(c) ; }
int getterms(terms)
	char ***terms ;
{
	static char	*line,**tokens ;
	static size_t	ll, tl ;
	char		*prev_line,**prev_tokens ;
	enum { PLAIN, ESC1, TOKEN, ESC2, COMMENT, END} state = PLAIN ;
	int		nextc;
	size_t		temp ;
	char		*linep,**tokenp;

	if ( !line ) {
		ll=150 ; tl=20 ;
		line = (char *) malloc(ll) ;
		tokens = (char **) malloc(tl*sizeof(char *));
		if ( !line || !tokens ) {
			fprintf(stderr,"%s: Out of memory\n",progname) ;
			return -1 ;
		}
		
	}
	linep=line ; tokenp=tokens ;
	do {
		nextc=getch() ;
		if ( tokenp>tokens+tl-5 ) {
			temp=tl+1000 ;
			prev_tokens=tokens ;
			tokens = (char **) realloc((_VOIDSTAR) tokens,temp);
			if ( !tokens ) {
				fprintf(stderr,"%s: Out of memory\n",
					progname) ;
				line= (char *)0 ;
				return -1 ;
			}
			tl=temp;
			/* Adjust tokenp */
			tokenp= tokens + (tokenp-prev_tokens) ;
		}
		if ( linep > line+ll-5 ) {
			temp = ll+1000 ;
			prev_line=line ;
			line = (char *) realloc((_VOIDSTAR) line,temp);
			if ( !line ) {
				fprintf(stderr,"%s: Out of memory\n",
					progname) ;
				line= (char *)0 ;
				return -1 ;
			}
			/* Adjust contents of tokenp */
			for (prev_tokens= tokens ; prev_tokens<tokenp ;
		             prev_tokens++ ) {
				*prev_tokens = line + ( *prev_tokens-prev_line);
			}
			linep= line + (linep-prev_line) ;
			ll=temp;
		}
		switch ( state ) {
		case PLAIN:
			switch ( nextc ) {
			case '\\' :
				state=ESC1 ;
				break ;
			case '#' :
				state=COMMENT ;
				break ;
			case EOF:
			case '\n' :
				state=END ;
				break ;
			default:
				if ( !isspace(nextc) ) {
					 Starttoken(nextc); state=TOKEN ;
				}
				break ;
			}
			break ;
		case ESC1: /* Escape seen in PLAIN state */
			switch ( nextc ) {
			case EOF:
				state=END ;
				break ;
			case '\n' :
				state=PLAIN ;
				break ;
			default:
				Starttoken(nextc); state=TOKEN ;
				break ;
			}
			break ;
		case TOKEN:
			switch ( nextc ) {
			case '\\' :
				state=ESC2 ;
				break ;
			case '#' :
				*linep++=0 ;
				state=COMMENT ;
				break ;
			case EOF:
			case '\n' :
				*linep++=0 ;
				state=END ;
				break ;
			default:
				if ( isgraph(nextc) ) {
					*linep++=nextc ; state=TOKEN ;
				} else
				if ( isspace(nextc) ) {
					*linep++=0 ;
					state= PLAIN ;
				} else {
					fprintf(stderr,
						"%s: control characters in startupfile\n");
					return -1 ;
				}
			}
			break ;
		case ESC2: /* Escape seen in TOKEN state */
			switch ( nextc ) {
			case '\n' :
				state=TOKEN ;
				break ;
			case EOF:
				state=END ;
				break ;
			default:
				*linep++=nextc ; state=TOKEN ;
				break ;
			}
			break ;
		case COMMENT:
			if ( nextc=='\n' || nextc==EOF) state=END ;
		}
	} while ( state!=END ) ;		

	*tokenp= 0;
	*terms= tokens ;
	return (nextc==EOF?0:1) ;
}

/* A routine to read the next character from the startup file */
int getch() {
	b_fsize	didread ;
	errstat	status ;

	if ( start_eof ) return EOF ;
	if ( start_curpos>=start_lastpos ) {
		status=b_read(&start_cap,start_readpos,start_blk,(b_fsize) BLKSIZE,&didread);
		if ( status!=STD_OK || didread==0) {
			start_eof=1;
			return EOF ;
		}
		start_lastpos=didread ; start_curpos=0 ;
		start_readpos+=didread ;
	}
	return start_blk[start_curpos++]&0xFF ;
}

print_startup(f)
	FILE *f;
{
    int i, j;
    fprintf(f, "Replication Sets:\n");
    for (i = 0; i < num_repl_sets; i++) {
	fprintf(f, "\t{");
	for (j = 0; j < repl_set[i].size; j++) {
	    fprintf(f, "%s%s", (j == 0 ? "" : ", "), repl_set[i].path[j]);
	}
	fprintf(f, "}\n");
    }
    if ( num_soaps ) {
	fprintf(f, "\nSoap servers:\n");
	for (i = 0; i < num_soaps; i++) {
	    fprintf(f, "\t%s\n", soaps[i].path);
	}
    }
    if ( !touch_all ) {
	fprintf(f, "\nTouch capabilities from servers:\n");
	for (i = 0; i < num_touch; i++) {
	    fprintf(f, "\t%s\n", touchlist[i].path);
	}
    }
    if (server_mode) {
	fprintf(f, "\nAge:\n");
	for (i = 0; i < num_toage; i++) {
	    fprintf(f, "\t%s\n", age[i].path);
	}
	if ( capsrc==2 ) {
	    fprintf(f,"\nName used for publishing capability: %s\n",capfile);
	}
    }
    fprintf(f, "\n");
}

/* Add element to list of preferred file servers */
void
setpref(name)
	char *name ;
{
	int	temp ;
	int	err;

	if ( num_prefs>=max_prefs ) {
		temp=max_prefs+5 ;
		prefs=(struct Server_Set *)(max_prefs?
			    realloc((_VOIDSTAR) prefs,
						(size_t) temp*sizeof(*prefs)):
			    malloc((size_t) temp*sizeof(*prefs))
			 ) ;
		if ( !prefs ) {
			fprintf(stderr,"%s: Out of memory\n",
				progname) ;
			exit(1) ;
		}
		max_prefs=temp ;
	}

	prefs[num_prefs].path=name ;
	err= name_lookup(name,&prefs[num_prefs++].server);
	if ( err!=STD_OK ) {
		fprintf(stderr,"%s: can not find %s\n",progname,name);
		exit(1);
	}
}

/*
 * Order a capset according to the preferences of the caller
 * returns 1 if capset has changed, zero otherwise
 */

int
do_order(cs)
	capset *cs;
{
    int		i,j;
    int		top=0;
    suite	*s;
    capability	temp;
    int		changed=0;

    for (i = 0; i < num_prefs; i++) {
	for (j = top; j < cs->cs_final; j++) {
	    s = &cs->cs_suite[j];
	    if (s->s_current &&
			PORTCMP(&s->s_object.cap_port, &prefs[i].server.cap_port)) {
		if (j!=top) {
		    temp=cs->cs_suite[top].s_object;
		    cs->cs_suite[top].s_object=s->s_object;
		    s->s_object=temp;
		    changed=1;
		}
		top++;
	    }
	}
    }
    return changed;
}
	

/* Support for std_ntouch:
 *
 * To avoid thousands of std_touch RPC's floating over the network
 * in each OM run, we try to batch the std_touch() commands for a
 * each server into a single std_ntouch().
 *
 * In order to deal with the possibility of old servers that don't
 * support STD_NTOUCH, the first touch_later() for a server will do
 * a std_ntouch() right away.  If that fails (probably with STD_COMBAD)
 * it will instead use STD_TOUCH for that server.
 */

typedef struct svr_touch_state touch_state;

struct svr_touch_state {
    port     ts_port;		/* port of the server */
    int      ts_dont_ntouch;	/* if 1, don't use STD_NTOUCH at all */
    int      ts_num_ntouch;	/* number of std_ntouch()es performed */
    int      ts_last_failed;	/* last round in which a STD_NTOUCH failed */
    private *ts_touch_list;	/* array of private parts still to touch */
    int      ts_touch_max;	/* maximum number of privates in the list */
    int      ts_touch_num;	/* current number of privs in the list */
    touch_state *ts_next;	/* next server state */
};

static touch_state *Svr_touch_list = NULL;

/* Take the current directory server transaction buffer size into account
 * when determining the number privates to touch with a single std_ntouch.
 * We could determine it dynamically per server, of course, but it probably
 * isn't worth the hassle.
 */
#define MAX_NPRIVS	(((SP_BUFSIZE - 100) / sizeof(private)) - 1)

static touch_state *
new_touch_state(p)
port *p;
{
    int max_nprivs;
    touch_state *ts;
    private *priv_array;

    ts = (touch_state *) malloc(sizeof(touch_state));
    if (ts == NULL) {
	return NULL;
    }

    max_nprivs = MAX_NPRIVS;
    priv_array = (private *) calloc((size_t) max_nprivs, sizeof(private));
    if (priv_array == NULL) {
	free((_VOIDSTAR) ts);
	return NULL;
    }
    
    ts->ts_port = *p;
    ts->ts_dont_ntouch = 0;
    ts->ts_num_ntouch = 0;
    ts->ts_last_failed = -2;	/* see touch_recently_failed() */
    ts->ts_touch_list = priv_array;
    ts->ts_touch_max = max_nprivs;
    ts->ts_touch_num = 0;
    ts->ts_next = Svr_touch_list;
    Svr_touch_list = ts;

    if (ntouch_verbose) {
	fprintf(stderr, "%s: new touch state for server '%s'\n",
		progname, ar_port(&ts->ts_port));
    }

    return ts;
}

static touch_state *
find_touch_state(p)
port *p;
{
    register touch_state *ts;

    /* simple linear lookup */
    for (ts = Svr_touch_list; ts != NULL; ts = ts->ts_next) {
	if (PORTCMP(p, &ts->ts_port)) {
	    return ts;
	}
    }

    return NULL;
}

static errstat
touch_svr_flush(ts)
touch_state *ts;
{
    errstat err;
    int     n_touched = 0;

    err = std_ntouch(&ts->ts_port, ts->ts_touch_num,
		     ts->ts_touch_list, &n_touched);
    ts->ts_num_ntouch++;

    if (err != STD_OK || n_touched != ts->ts_touch_num) {
	/* Oops, we have to be careful in the next pass. */
	ts->ts_last_failed = number_passes;
	NTouch_last_failed = number_passes;

	/* Correct number of objects actually touched. Obviously it must
	 * be within bounds, and when we got an error, it should not be
	 * equal to the number requested.
	 */
	if (n_touched < 0 || n_touched > ts->ts_touch_num ||
	    (err != STD_OK && n_touched == ts->ts_touch_num))
	{
	    n_touched = 0;
	}
	replicas_touched -= (ts->ts_touch_num - n_touched);

	fprintf(stderr, "%s: std_ntouch failed for port '%s' (%s)\n",
		progname, ar_port(&ts->ts_port), err_why(err));
	if (err != STD_COMBAD && err != STD_ARGBAD) {
	    fprintf(stderr, "%s: only touched %d (expected %d)\n",
		    progname, n_touched, ts->ts_touch_num);
	}
    }

    replicas_n_touched += n_touched;

    /* clear buffer */
    ts->ts_touch_num = 0;

    return err;
}

errstat
touch_later(cap)
capability *cap;
{
    touch_state *ts;

    ts = find_touch_state(&cap->cap_port);
    if (ts == NULL) {
	/* No touch state for this server present yet; try to allocate one */
	ts = new_touch_state(&cap->cap_port);
    }
    if (ts == NULL || ts->ts_dont_ntouch) {
	/* cannot use std_ntouch for this server */
	return std_touch(cap);
    }

    /* Add it to the buffer */
    ts->ts_touch_list[ts->ts_touch_num] = cap->cap_priv;
    ts->ts_touch_num++;

    /* If there's not enough room for another private part,
     * or if we're doing it for the first time, flush it.
     */
    if (ts->ts_num_ntouch == 0 || ts->ts_touch_num == ts->ts_touch_max) {
	int     firsttime = (ts->ts_num_ntouch == 0);
	errstat err = touch_svr_flush(ts);

	if ((firsttime && err != STD_OK) ||
	    (err == STD_COMBAD || err == STD_ARGBAD))
	{
	    /* From now on, don't use STD_NTOUCH for this server at all */
	    ts->ts_dont_ntouch = 1;
	}

	/* The error may well be for another object than "cap",
	 * so just std_touch it again to be sure.
	 */
	return std_touch(cap);
    } else {
	return STD_OK;
    }
}

errstat
touch_flush(touch_all)
int touch_all;
{
    /* Flush pending std_touch commands to the servers.
     * If "touch_all" is 1, flush all touches (typically done at the end of
     * each run).  Else only flush touches for a server that complained
     * recently.
     */
    errstat err, retval;
    touch_state *ts;

    retval = STD_OK;
    for (ts = Svr_touch_list; ts != NULL; ts = ts->ts_next) {
	if (ts->ts_touch_num > 0) {
	    if (touch_all || (ts->ts_last_failed >= number_passes - 1)) {
		err = touch_svr_flush(ts);
		if (err != STD_OK) {
		    retval = err;
		}
	    }
	}
    }

    return retval;
}

int
touch_last_failed(cap)
capability *cap;
{
    touch_state *ts;

    ts = find_touch_state(&cap->cap_port);
    if (ts != NULL) {
	return ts->ts_last_failed;
    } else {	/* not tried this server at all yet */
	return -1;
    }
}

/*
 * Touch each object in the cap set.
 */
void
touch(path, object_cs)
    char *path;
    capset *object_cs;
{
    int i,j;
    suite *s;
    errstat err;
    int		domess   =0;
    int		dotouch;
    int		didtouch =0;

    for (i = 0; i < object_cs->cs_final; i++) {
	s = &object_cs->cs_suite[i];
	if (s->s_current) {
	    dotouch=touch_all ;
	    for (j = 0; !dotouch && j < num_touch; j++) {
		if ( PORTCMP(&s->s_object.cap_port,&touchlist[j].server.cap_port)){
		    dotouch=1 ; break ;
		}
	    }
	    if ( dotouch ) {
                if ( debug>0 && !domess ) {
                    fprintf(stderr, "---- touch(\"%s\")\n", path);
                    domess=1;
                }
                if ((err = touch_later(&s->s_object)) == STD_OK) {
                    replicas_touched++;
		    didtouch=1 ;
                } else if (verbose) {
                    fprintf(stderr, "%s: std_touch failed (%s)\n",
                                            path, err_why(err));
                }
	    }
	}
    }
    if (verbose && didtouch) {
	fprintf(stderr, "touch %s: %d\n", path, replicas_touched);
    }

    return ;
}

/* The repl_set is a set of equivalent servers across which the replication is
 * to be performed.  For each server port that cannot be found in the object's
 * capset, or that is in the capset but is invalid, use the first working cap
 * found in the capset to create a copy of the object in the missing server
 * and then add it to the object capset.
 *
 * If force_replicas was specified, ensure that the replication set has at least
 * one capability for each server in the replication set, even if cs_final
 * isn't large enough, adding missing replicas
 *
 * NOTE: This function will touch everything in the cap set as well.
 */
void
replicate(path, object_cs,parent_cs, entry, repl_set)
    char		*path;
    capset		*object_cs;
    capset		*parent_cs;
    char		*entry;
    struct Repl_Set	*repl_set;
{
    capability source_cap;
    capability new_cap;
    capability *cap_ptr;
    enum { UNKNOWN,TOUCHED,ERROR} servstat[MAX_SET_SIZE];
    int  repl_no;
    capability *newcaps[MAX_SET_SIZE];
    int	       n_newcaps=0;
    errstat err;
    sp_entry ent;
    int i, j;
    suite *s;
    int add_cs     = 0;
    int del_cs     = 0;
    int replace_cs = 0;
    int empty_cs   = 1;
    int good_cap   = 0;
    int re_ordered = 0;

    if (debug > 0)
	fprintf(stderr, "---- replicate(\"%s\")\n", path);

    /* Touch everything and get a working cap out of the capset,
       if possible: */
    for (j = 0; j < repl_set->size; j++) {
	servstat[j]=UNKNOWN;
    }
    for (i = 0; i < object_cs->cs_final; i++) {
	int last_failed;

	s = &object_cs->cs_suite[i];
	if (!s->s_current)
	    continue;
	repl_no= -1;
	for (j = 0; j < repl_set->size; j++) {
	    if (PORTCMP(&repl_set->server[j].cap_port,&s->s_object.cap_port)){
		repl_no=j;
		break;
	    }
	}

	/* Note: here we should only batch std_touch requests if all the
	 * replicas are there (otherwise we need std_touch to find a good
	 * source replica).  But this could mean that we don't directly
	 * notice the situations where a fully replicated set contains
	 * an invalid replica.  When we see this happening, we will just
	 * use std_touch every third run.
	 */
	last_failed = touch_last_failed(&s->s_object);

	if (!server_mode ||
	    ((last_failed >= 0) && ((number_passes - last_failed) <= 1) &&
	     (number_passes % 3 == 0)))
	{
	    err = std_touch(&s->s_object);
	}
	else
	{
	    err = touch_later(&s->s_object);
	}

	if (err == STD_OK) {
	    source_cap = s->s_object;
	    replicas_touched++;
	    good_cap=1;
	    if ( repl_no>=0 ) {
		servstat[repl_no]=TOUCHED;
		empty_cs=0;
	    }
	} else {
	    if ( repl_no>=0 ) {
		if ( err==RPC_NOTFOUND ) {
		    /* This is rather crude, actually we should
			switch off this replication set and not age the
			bullet server.
		     */
		    fprintf(stderr,"%s: server %s unreachable, aborted pass\n",
			progname,repl_set->path[j]);
		    stopit=1 ;
		    return ;
		}
		servstat[repl_no]=ERROR;
	    }
	}
    }
    if (!good_cap)
	return;		/* No working caps -- cannot do replication */

    if (remove_replicas && cleanup_suite(object_cs) ) {
	/* Don't make it final yet, we are not sure that
	   after deletion there will be anything left */
	del_cs++;
    }

    /* For each server in replication set: */
    for (j = 0; j < repl_set->size; j++) {
	capability *server;

	if ( servstat[j]==TOUCHED ) continue ;
	/* Check whether one of the capabilities is owned by this server:
	 */
	server = &repl_set->server[j];
	if (servstat[j]==UNKNOWN) {
	    /* Capset doesn't contain an object owned by
	     * this server, so make one:
	     */
	    if ( force_replicas ) {
	
		/* Caller wants us to put one replica in the capset for
		 * each server:
		 */

		if ((err = std_copy(server, &source_cap, &new_cap)) == STD_OK) {
		    /* Add the cap to the capset*/
		    add_cap(object_cs,&new_cap);
		    newcaps[n_newcaps++]= &new_cap;
		    add_cs++;
		    empty_cs=0;
		} else if ( err==RPC_NOTFOUND ) {
		    /* This is rather crude, actually we should
			switch off this replication set and not age the
			bullet server.
		     */
		    fprintf(stderr,"%s: server %s unreachable, aborted pass\n",
			progname,repl_set->path[j]);
		    stopit=1 ;
		    return ;
	        } else if (verbose) {
		    fprintf(stderr, "%s: std_copy failed (%s)\n",
						path, err_why(err));
	        }
	    }
	} else {
	    /* We have seen an error on this one, retry */
	    for (i = 0; i < object_cs->cs_final; i++) {
		s = &object_cs->cs_suite[i];
		if (s->s_current &&
		    PORTCMP(&server->cap_port, &s->s_object.cap_port))
		    break;
	    }
	    /* The s->s_object cap is for this server;
	     * do a std_touch right away (no std_ntouch wanted here).
	     */
	    if ((err = std_touch(&s->s_object)) == STD_OK) {
		replicas_touched++;
		empty_cs=0;
		continue;
	    } else if (err == STD_CAPBAD) {
		/* The server is alive, but the cap is invalid,
		 * so replace it with a new valid cap:
		 */
		if ((err = std_copy(server, &source_cap, &new_cap))
								== STD_OK) {
		    /* Replace the old cap in the capset: */
		    s->s_object = new_cap;
		    replicas_replaced++;
		    newcaps[n_newcaps++]= &new_cap;
		    add_cs++;
		    replace_cs++;
		    empty_cs=0;
		} else if ( err==RPC_NOTFOUND ) {
		    /* This is rather crude, actually we should
			switch off this replication set and not age the
			bullet server.
		     */
		    fprintf(stderr,"%s: server %s unreachable, aborted pass\n",
			progname,repl_set->path[j]);
		    stopit=1 ;
		    return ;
		} else if (verbose) {
		    fprintf(stderr, "%s: std_copy failed (%s)\n",
							path, err_why(err));
		}
	    } else if ( err==RPC_NOTFOUND ) {
		/* This is rather crude, actually we should
		   switch off this replication set and not age the
		   bullet server.
		 */
		fprintf(stderr,"%s: server %s unreachable, aborted pass\n",
			progname,repl_set->path[j]);
		stopit=1 ;
		return ;
	    } else if (verbose) {
		fprintf(stderr, "%s: std_touch failed (%s)\n",
						path, err_why(err));
	    }
	}
    }
    if (verbose)
	fprintf(stderr, "touch %s: %d\n", path, replicas_touched);
    if ( prefs ) {
	re_ordered=do_order(object_cs);
	if ( re_ordered) entries_reordered++;
    }
    if (add_cs || (del_cs && !empty_cs) || re_ordered ) {
	ent.se_name=entry;
	ent.se_capset= *parent_cs;
	cap_ptr= &source_cap;
    	err = sp_install(1,&ent,object_cs,&cap_ptr);
	if ( err!=STD_OK ) {
	    /* Destroy all we have created */
	    for(i=0;i<n_newcaps;i++){
		(void) std_destroy(newcaps[i]);
	    }
	}
	switch(err) {
	case RPC_NOTFOUND:
	case STD_NOTFOUND:
	case SP_UNAVAIL:
	    /* This is rather crude, actually we should
		switch off this replication set and not age this
		soap server.
	     */
	    fprintf(stderr,"%s: soap server unreachable, aborted pass\n",
		progname);
	    stopit=1 ;
	    return ;
	case SP_CLASH:
	    /* Simply forget about it, replication will come next time */
	    installs_clashed++;
	    break;
	case STD_OK:
	    entries_changed++;
	    if ( verbose ) {
		if ( del_cs ) fprintf(stderr,"remove replica(s) %s: %d\n",
		    		path,replicas_removed);						if ( add_cs-replace_cs>0 )
			fprintf(stderr, "add replica %s: %d\n", path,
								replicas_added);
		if ( replace_cs ) fprintf(stderr, "replace replica %s: %d\n",
				path,replicas_replaced);
		if ( re_ordered ) fprintf(stderr, "reordered %s: %d\n",
				path,entries_reordered);
	    }
	    break;
	default:
	    if ( verbose )
		fprintf(stderr, "%s: sp_install failed (%s)\n",
					path, err_why(err));
	    break;
	}
    }
}

/*
 * If remove_replicas is specified replicas with a wrong port or not
 * in the replica set or are removed.
 * returns 1 if suite has changed, 0 otherwise
 */
int
cleanup_suite(object_cs)
	capset *object_cs;
{
    int		i,j,remcount;
    suite	*s;

    remcount=replicas_removed;
    /* Remove replicas that have a port not in the repl set: */
    for (i = 0; i < object_cs->cs_final; i++) {
        s = &object_cs->cs_suite[i];
        if (s->s_current) {
	    /* find ports not in set */
	    for (j = 0; j < repl_set->size; j++) {
    		capability *server = &repl_set->server[j];
    		if (PORTCMP(&server->cap_port, &s->s_object.cap_port))
    		    break;
    	    }
    	    if (j == repl_set->size) {
    		/* The port is not in the repl set -- delete the cap: */
		replicas_removed++;
    		s->s_current = 0;
    	    }
	    /* find entries for same port */
	    for (j = i+1; j < object_cs->cs_final; j++) {
    		capability *other = &object_cs->cs_suite[j].s_object;
    		if (PORTCMP(&other->cap_port, &s->s_object.cap_port)) {
    		    object_cs->cs_suite[j].s_current=0;
		    replicas_removed++;
		}
    	    }
        }
    }
    return remcount!=replicas_removed ;
}

/* A simple routine to decrease the size of files read, whenever the
   server announces that it can not read that big size buffers
 */
int
decr_cmps() {
	if ( cmp_size <= BLKSIZE ) {
		return 0 ;
	}
	cmp_size /= 2 ;
	/* round up to nearest multiple of 1024 */
	cmp_size = (( cmp_size + BLKSIZE-1 ) / BLKSIZE) * BLKSIZE ;
	/* The result of this computation is smaller than the original value */
	return 1 ;
}

/* Check whether all replicas are identical */
do_check(path, cs, servset)
	char	*path;	
	capset	*cs;
	struct Repl_Set *servset;
{
	b_fsize		read_next, did_read1, did_read2 ;
	int		i;
	capability	firstcap, cap;
	int		firstnum= -1;
	int		repl_count=0 ;
	b_fsize		firstsize=0 ;
	int		firstread=0 ;
	int		firsttouched=0 ;
	errstat		status;
	int		ok=0;
	
	if (verbose) fprintf(stderr,"checking %s\n",path);
	objects_checked++;
	for ( i=0; i<cs->cs_final && i<MAX_SET_SIZE; i++ ) {
		if ( !cs->cs_suite[i].s_current ) continue ;
		cap=cs->cs_suite[i].s_object;
		if ( !member_servers(&cap,servset) ) {
			fprintf(stderr,"%s: not all capabilities for %s belong to the same set of replication servers\n",
				progname,path);
			suites_notok++ ;
		}
		repl_count++;
		if ( check_replicas<2 ) { ok=1 ; continue ; }
		if ( firstnum==-1 ) {
			firstcap=cap ;
			firstnum=i ;
			continue ;
		}
		/* Did we previously read the file in one blow? */
		/* If so, we still have it */
		if ( firstread ) {
		    status=b_read(&cap,(b_fsize) 0,cmp_f2,(b_fsize) cmp_size,&did_read2);
		    if ( status!=STD_NOMEM ) {
			if ( status!=STD_OK ) {
				fprintf(stderr,"%s: error \"%s\" while reading from capability %d in %s\n",
				    progname,err_why(status),i,path);
			    break ;
			}
			replicas_read++;
			replicas_touched++;
			if ( did_read2==firstsize &&
				memcmp((_VOIDSTAR) cmp_f1, (_VOIDSTAR) cmp_f2,
						    (size_t)did_read2)==0 ) {
			    /* Everything is ok */
			    ok=1;
			    break ;
			}
			fprintf(stderr,"%s: the files under capabilities %d and %d for %s differ\n",
			    progname, firstnum, i, path) ;
			break ;
		    } /* Fall through on STD_NOMEM, simply retry */
		}
		read_next=0;
		replicas_read+=2 ;
		replicas_touched+=2 ;
		for(;;) {
			/* Now compare the two files
			   we did not read anything yet */	
				
			/* Read the first file */
			do {
			    status=b_read(&firstcap,read_next,cmp_f1,(b_fsize)cmp_size,&did_read1);
			    if ( status!=STD_NOMEM ) break ;
			} while ( decr_cmps() ) ;
			if ( status!=STD_OK ) {
				/* Failure: forget this one */
				fprintf(stderr,"%s: error \"%s\" while reading from capability %d in %s\n",
					progname,err_why(status),firstnum,path);
				firstcap=cs->cs_suite[i].s_object ;
				firstnum=i ;
				firsttouched=0;
				break ;
			}
			firsttouched=1;
				
			/* Read the second file */
			do {
			    status=b_read(&cap,read_next,cmp_f2,(b_fsize)cmp_size,&did_read2);
			    if ( status!=STD_NOMEM ) break ;
			} while ( decr_cmps() ) ;
			if ( status!=STD_OK ) {
				/* Failure: forget this one */
				fprintf(stderr,"%s: error \"%s\" while reading from capability %d in %s\n",
					progname,err_why(status),i,path);
				break ;
			}

			/* Now check whether we have eof on first read */
			if ( did_read1<cmp_size ) {
				/* if so, comparing becomes easy next time */
				firstread=1 ;
				firstsize=did_read1 ;
			}
			if ( did_read1>cmp_size ) did_read1=cmp_size;
			if ( did_read1!=did_read2 ) {
				fprintf(stderr,
"%s: the files under capabilities %d and %d for %s differ in size\n",
					progname,firstnum,i,path) ;
				break ;
			}
			if ( did_read1==0 ) {
				ok=1;
				break ;
			}
			read_next += did_read1 ;
			if ( memcmp((_VOIDSTAR) cmp_f1, (_VOIDSTAR) cmp_f2,
						    (size_t)did_read1)!=0 ) {
				fprintf(stderr,"%s: the files under capabilities %d and %d for %s differ\n",
					progname,firstnum,i,path);
				break ;
			}
			if ( did_read1<cmp_size ) {
				ok=1;
				break ;
			} ;			
		}
	}
	if ( check_replicas>=2 && !firsttouched && firstnum>=0) {
		/* No sense in caching the std_touch here.
		 * We already had a problem reading the object.
		 */
		status = std_touch(&firstcap);
		if ( status==STD_OK ) {
			replicas_touched++ ;
			if ( verbose ) {
				fprintf(stderr,
					"touch %s: %d\n",path,replicas_touched);
			}
		} else {
			fprintf(stderr,"%s: %s std_touch failed (%s)\n",
				progname,path, err_why(status));
		}
	}
	if ( repl_count==1 ) objects_single++ ;
	if ( repl_count>1 ) objects_repl++ ;
	if ( ok ) objects_ok++ ;
	else if ( repl_count>1 ) objects_notok++ ; /* Empty suites don't count */
}
			    

long
getsecs()
{
	long	secs;
	int	msec, tz, dst;

	if ( tod_gettime(&secs,&msec,&tz,&dst)!= STD_OK ) return 0 ;
	return secs;
}

void
pr_elapsed(f,elapsed)
	FILE	*f;
	long	elapsed;
{
	char	buf[256];
	char	*cp;
	int	len;

	sprintf(buf, "Time Spent in Pass %d:",last_pass);
	len= strlen(buf) ;
	cp= buf+len ;
	for ( ; len<=31 ; len++ ) { *cp++=' ' ; }
	*cp=0;
	(void) fputs(buf,f);
	len=str_elapsed(buf,elapsed);
	if ( len>=sizeof buf ) {
		fprintf(stderr,"%s: internal buffer overflow in pr_elapsed\n",
			progname);
		exit(9);
	}
	buf[len]=0;
	(void) fputs(buf,f);
	(void) putc('\n',f);
}

/* Take STD_STATUS/STD_INFO requests on a port, just so that the bootserver
 * can know that we are running:
 */

int
str_elapsed(cp,elapsed)
	char	*cp;
	long	elapsed;
{
	int	days, hours, mins, secs ;
	int	len;

	secs= elapsed%60 ;
	mins= elapsed/60 ;
	hours= mins/60 ;
	mins %= 60 ;
	days= hours/24 ;
	hours %= 24 ;
	len=0 ;
	if ( days ) {
		if ( days>1 ) sprintf(cp, "%d days",days ) ; else
			      sprintf(cp, "one day");
		len+= strlen(cp) ;
		if ( secs==0 && mins==0 && hours==0 ) return len ;
		*(cp+len)=',' ; len++ ;
		*(cp+len)=' ' ; len++ ;
	}
	if ( hours || len ) {
		if ( hours!=1 ) sprintf(cp+len, "%d hours",hours ) ; else
			        sprintf(cp+len, "1 hour");
		len+= strlen(cp+len) ;
		if ( secs==0 && mins==0 ) return len ;
		*(cp+len)=',' ; len++ ;
		*(cp+len)=' ' ; len++ ;
	}
	if ( mins || len ) {
		if ( mins!=1 ) sprintf(cp+len, "%d minutes",mins ) ; else
			       sprintf(cp+len, "1 minute");
		len+= strlen(cp+len) ;
		if ( secs==0 ) return len ;
		*(cp+len)=',' ; len++ ;
		*(cp+len)=' ' ; len++ ;
	}
	if ( secs!=1 ) sprintf(cp+len, "%d seconds",secs ) ; else
		       sprintf(cp+len, "1 second");
	len+= strlen(cp+len) ;
	return len ;
}
	
/*ARGSUSED*/
void
server_loop(arg, len)
    char *arg;
    int len;
{
    char	*cp, buf[400];
    header	h;
    bufsize	n, reply_size;
    errstat	err;
    static capability mycap;   /* Is static just for initialization to 0 */
    capability	pubcap;
    capability	oldcap;
    port	check;
    char	pathbuf[257];
    char	*mycap_path;

    /* On startup, create a unique cap and store it.
     */
    uniqport(&mycap.cap_port);
    uniqport(&check);
    priv2pub(&mycap.cap_port,&pubcap.cap_port);
    if (prv_encode(&pubcap.cap_priv, (objnum)0, PRV_ALL_RIGHTS, &check) < 0){
	fprintf(stderr, "%s: can not prv_encode server cap\n",
		    progname);
	exit(1);
    }

    if ( *capfile=='/' ) {
	mycap_path=capfile;
    } else {
    	sprintf(pathbuf,"%s/%s",DEF_OMSVRDIR,capfile);
	mycap_path=pathbuf;
    }
    if ((err = name_lookup(mycap_path, &oldcap)) == STD_OK) {
	err= name_replace(mycap_path,&pubcap);
	if ( err != STD_OK && err != STD_NOTFOUND ) {
	    fprintf(stderr, "%s: name_replace %s failed (%s)\n",
				    progname, mycap_path, err_why(err));
	    exit(1);
	}
	/* Fall through on NOTFOUND & OK */
    }
    switch( err ) {
    case STD_NOTFOUND:
	if ((err = name_append(mycap_path, &pubcap)) != STD_OK) {
	    fprintf(stderr, "%s: name_append %s failed (%s)\n",
				    progname, mycap_path, err_why(err));
	    exit(1);
	}
    case STD_OK:
	break ;
    default:
	fprintf(stderr, "%s: name_lookup %s failed (%s)\n",
				progname, mycap_path, err_why(err));
	exit(1);
    } 

    /* Server loop: */
    for (;;) {
	h.h_port = mycap.cap_port;
	reply_size = 0;
	n = getreq(&h, buf, sizeof buf);
	if (ERR_STATUS(n)) {
		err = ERR_CONVERT(n);
		if (err == RPC_ABORTED) {
			continue;
		}
		fprintf(stderr, "%s: getreq failed!\n");
		exit(1);
	}
	switch (h.h_command) {
	    case STD_INFO:
		(void) strcpy(buf, "object manager");
		h.h_size = reply_size = strlen(buf);
		h.h_status = STD_OK;
		break;
	    case STD_STATUS:
		cp = buf;
		(void) strcpy(cp, "Object Manager:\n\n");
		cp += strlen(cp);
		sprintf(cp, state_fmt, number_passes);
		cp += strlen(cp);
		if ( last_pass ) {
		    int i ;
		    sprintf(cp, "Time Spent in Pass %d:",last_pass);
		    i= strlen(cp) ;
		    cp += i ;
		    for ( ; i<=31 ; i++ ) { *cp++=' ' ; }
		    cp+= str_elapsed(cp, secs_last_pass) ;
		    *cp++='\n';
		}
		sprintf(cp, "Missing Replicas Added:    %6d\n", replicas_added);
		cp += strlen(cp);
		sprintf(cp, "Invalid Replicas Replaced: %6d\n",
							    replicas_replaced);
		cp += strlen(cp);
		sprintf(cp, "Bad-Port Replicas Removed: %6d\n",
							    replicas_removed);
		cp += strlen(cp);
		sprintf(cp, "Install Clashes (total):   %6ld\n",
							    installs_clashed);
		cp += strlen(cp);
		sprintf(cp, "Objects Touched:           %6d\n",
							    replicas_touched);
		cp += strlen(cp);
		if ( prefs ) {
		    sprintf(cp,"Dir. Entries Reordered:    %6ld\n", entries_reordered);
		    cp += strlen(cp);
		}
		sprintf(cp, "Directory Entries Changed: %6d\n",
							    entries_changed);
		cp += strlen(cp);
		if ( last_aging ) {
		    sprintf(cp,"Last Aging Done in Pass:   %6d\n",
							    last_aging);
		} else if ( num_toage ) {
			sprintf(cp, "No Aging Done Yet\n");
		}
		if ((reply_size = strlen(buf)) > sizeof(buf))
			reply_size = sizeof(buf);
		h.h_size = reply_size;
		h.h_status = STD_OK;
		break;
	    default:
		h.h_status = STD_COMBAD;
		break;
	}
	putrep(&h, buf, reply_size);
    }
}
