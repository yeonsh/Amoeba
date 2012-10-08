/*	@(#)grpmain.c	1.1	96/02/27 10:02:40 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */
 
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "amoeba.h"
#include "group.h"
#include "caplist.h"
#include "semaphore.h"
#include "exception.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "capset.h"
#include "bullet/bullet.h"
#include "soap/soap.h"
#include "monitor.h"
#include "module/stdcmd.h"
#include "module/tod.h"
#include "module/mutex.h"
#include "module/ar.h"
#include "posix/limits.h"
#include "thread.h"

#include "global.h"
#include "soapsvr.h"
#include "caching.h"
#include "superfile.h"
#include "sp_impl.h"
#include "sp_adm_cmd.h"
#include "misc.h"
#include "modlist.h"
#include "filesvr.h"
#include "dirfile.h"
#include "intentions.h"
#include "seqno.h"
#include "main.h"
#include "dirsvr.h"
#include "watchdog.h"
#include "timer.h"

#define TIMESTACK	5000
#define NCAPCACHE       256

#define MIN_KBYTES      100
#define MAX_KBYTES      64000
#define DEF_MAX_KBYTES  100

/* soap variables */
static capability 	other_supercap;   /* admin capability of other Soap */
capability 		my_supercap;      /* with put port for admin request */
port       		my_super_getport; /* corresponding getport */
port 			_sp_getport;
capability 		_sp_generic;

int Debugging = 0;

capset sp_rootdir;
capset sp_workdir;  /* not used but needed to silence the loader */

static mutex	sp_mutex;

struct sp_table *sp_first_update();
struct sp_table *sp_next_update();

/*
 * Soap glue routines.
 */

errstat
sp_init()
{
    return STD_OK;
}

void
sp_begin()
{
    mu_lock(&sp_mutex);
}

void
sp_end()
{
    mu_unlock(&sp_mutex);
}

void
sp_abort()
{
    struct sp_table *st;

    for (st = sp_first_update(); st != NULL; st = sp_next_update()) {
        sp_undo_mod_dir(st);
    }
    sp_clear_updates();

    sp_end();
}

errstat
sp_commit()
{
    struct sp_table *st;
    struct sp_save  *firstmod;
    objnum  obj;
    errstat err;
    
    /* When we get an error writing to the (probably local) bulletsvr,
     * we can do nothing but panic().  We cannot abort the transaction
     * because the other members will (presumably?) not have this problem
     * and hence have accepted the modification.
     */

    /* First see if we have multiple modifications as a result of the last
     * command. (This may happen as a result of sp_install()).
     * If so, we must use the intentions module rather than the modlist module
     * in order to guarantee atomicity.
     */
    st = sp_first_update();
    firstmod = (st != NULL) ? st->st_dir->sd_mods : NULL;

    if ((firstmod != NULL && firstmod->ss_next == NULL) /* just 1 mod */
	&& (st->st_dir->sd_update == NULL)		/* just 1 dir */)
    {
	obj = st - _sp_table;

	assert(sp_in_use(st));

	if (st->st_flags & SF_DESTROYING) {
	    MON_EVENT("destroying dir");
	    (void) remove_dir(obj);
	} else {
	    /* Note: we don't increase the current seqno of the directory
	     * itself, but use as the new seqnr the (already incremented)
	     * *global* sequence number.
	     * This avoids the possibility of having different incarnations
	     * of the same directory at the same time.
	     */
	    get_global_seqno(&st->st_dir->sd_seq_no);

	    err = ml_store(obj);
	    if (err != STD_OK && err != STD_NOSPACE) {
		/* Command not handled by modlist module, or no space in the
		 * modlist itself, or not enough memory.
		 */
		if (!ml_in_use()) {
		    /* Try to modify it, when possible and efficient.
		     * Otherwise write it out as a whole.
		     */
		    err = write_new_dir(obj, 1);
		} else {
		    /* Write it as a whole and remove it from the mod list */
		    err = ml_write_dir(obj);
		}
	    }

	    switch (err) {
	    case STD_OK:
		sp_free_mods(st);
		break;

	    case STD_NOSPACE:
		/* The directory would become to big.  This error should be
		 * consistent to all members, because they have the same
		 * data and use the same buffer sizes.
		 */
		scream("dir %ld would become too big", (long) obj); 
		sp_abort();	/* undoes the modification */
		return err;

	    default:
		/* must panic; see above */
		panic("sp_commit: cannot write dir %ld (%s)",
		      (long) obj, err_why(err));
	    }
	}
    } else {
	/* There are multiple updates or no updates at all */
	capability curcaps[NBULLETS];
	capability newcaps[NBULLETS];
	capability filesvrs[NBULLETS];
	struct sp_table *next_st;
	int nintents, avail;

	/* First make sure we have room for all of the intentions. */
	avail = intent_avail();
	nintents = 0;
	for (st = sp_first_update(); st != NULL; st = sp_next_update()) {
	    nintents++;
	}
	if (nintents > avail) {
	    scream("too many intentions: %d (max %d)", nintents, avail);
	    MON_EVENT("too many intentions");
	    sp_abort();
	    return STD_NOSPACE;
	}

	fsvr_get_svrs(filesvrs, NBULLETS);

	for (st = sp_first_update(); st != NULL; st = next_st ) {
	    next_st = sp_next_update();
	    obj = st - _sp_table;

	    /* Add an updated version of this dir to the intentions list.
	     * All dirs for which we have modifications should be in memory:
	     */
	    assert(sp_in_use(st));
	    assert(in_cache(obj));
    
	    /* Note: all directories modified in as a result of the current
	     * modification get the same seqno!
	     */
	    get_global_seqno(&st->st_dir->sd_seq_no);

	    if (!ml_in_use()) {
		/* Try to modify it, when possible and efficient.
		 * Otherwise write it out as a whole.
		 */
		get_dir_caps(obj, curcaps);
		err = dirf_modify(obj, NBULLETS, curcaps, newcaps, filesvrs);
	    } else {
		err = dirf_write(obj, NBULLETS, newcaps, filesvrs);
	    }

	    switch (err) {
	    case STD_OK:
		/* Put the new cap(s) in the intentions list.  Don't install
		 * them in the supertable yet: that'll be done after we have
		 * written the intention.  It'll also give us the opportunity
		 * to destroy the old versions afterwards.
		 */
		intent_add(obj, newcaps);
		MON_EVENT("added intention");
		break;

	    case STD_NOSPACE:
		/* The directory would become to big.  This error should be
		 * consistent to all members, because they have the same
		 * data and use the same buffer sizes.
		 * Remove the intentions and let the transaction fail.
		 */
		intent_undo();
		scream("dir %ld would become too big", (long) obj); 
		sp_abort();	/* undoes the modifications */
		return err;

	    default:	    /* must panic; see above */
		panic("sp_commit: cannot write dir %ld (%s)",
		      (long) obj, err_why(err));
	    }
	}

	if (nintents > 0) {
	    /* Write the intentions created to disk */
	    super_store_intents();

	    /* Install the modified directories in the super table and remove
	     * the intention again.  If we happen to crash between storing the
	     * intention and executing it, we'll complete it the next time
	     * during recovery.
	     */
	    intent_make_permanent();
	    MON_EVENT("executed intention");
	}
    }

    sp_clear_updates();
    sp_end();
    return STD_OK;
}

/* TODO: the number of directories touched per watchdog call, and the
 * number of watchdog calls per hour should be based on the number of
 * directories available and the default aging interval (one hour).
 * Currently we just touch a default number of directories (100) every
 * 5 minutes.
 */
#define WATCHDOG_INTERVAL	5

static void timer(param, psize)
/*ARGSUSED*/
{
    static int watch_count;

    for (;;) {
	sleep(60);
	time_update();

	/* We must keep the time, which is stored with the directory entries
	 * consistent among the members.  Otherwise programs like 'make' might
	 * be confused.
	 */
	broadcast_time();

	if ((watch_count++ % WATCHDOG_INTERVAL) == 0) {
	    if (dirsvr_functioning()) {
		MON_EVENT("watchdog");
		sp_begin();
		watchdog();		/* which calls sp_end() */
	    }
	}
    }
}

/*
 * The initialization procedure.  Initialize the cache, and read the super
 * file.  If in one-copy-mode I can load the directories and start operation.
 * Otherwise I will have to communicate with the other server first.
 */
void
sp_doinit(max_Kbytes)
unsigned int max_Kbytes;
{
    char    info[40];
    errstat err;
    int	    size;
    
    mu_init(&sp_mutex);

    if (!cc_init(NCAPCACHE)) {
	panic("doinit: cc_init failed (not enough memory)");
    }

    cache_set_size(max_Kbytes);
    /* This computes the maximum number of directories to be kept in-cache,
     * which is also used by super_init to determine how many blocks of
     * the super file should be kept in-core.
     */

    /* Allocate and read super file.  The capability for it is fetched
     * from the capability environment.
     * Also initialises variables stored in block 0,
     * such as ports to listen to, file servers to be used etc.
     */
    super_init(&_sp_generic, &my_supercap, &other_supercap);

#ifdef DEBUG
    message("_sp_generic: %s", ar_cap(&_sp_generic));
    message("my_supercap: %s", ar_cap(&my_supercap));
    message("other_supercap: %s", ar_cap(&other_supercap));
#endif

    /*
     * Create public versions of the capabilities retrieved
     * from the super block
     */
    _sp_getport = _sp_generic.cap_port;
    priv2pub(&_sp_getport, &_sp_generic.cap_port);

    my_super_getport = my_supercap.cap_port;
    priv2pub(&my_super_getport, &my_supercap.cap_port);
    
    /*
     * Make sure there is no other server listening to my port.
     */
    err = std_info(&my_supercap, info, (int) sizeof(info), &size);
    if (err == STD_OK || err == STD_OVERFLOW) {
	/* Oops, a SOAP server is already listening to this port: */
	panic("server %d is already active", sp_whoami());
    }

    /*
     * Initialise other modules.
     */
    cache_init((int) max_Kbytes);
    time_update();

    if(!thread_newthread(timer, TIMESTACK, (char *) NULL, 0)) {
        panic("cannot start the timer thread");
    }

    /* If there are still intentions on the super block, execute them now. */
    intent_make_permanent();

    init_sp_table(1);
}

static void
usage(progname)
char *progname;
{
    printf("Usage: %s [flags] member nmembers\n", progname);

    printf("\nflags:\n");
    printf("-a         : autoformat modification list when uninitialised\n");
    printf("-d         : activate debugging code\n");
    printf("-x         : reformat (and overwrite!) current modication list\n");
    printf("-k rowmem  : allowed memory for rows in cache\n");
    printf("-r nmember : minimum #members needed to allow updates\n");
    printf("-M         : use the modification-list module\n");

    exit(1);
}

void
main(argc, argv)
int argc;
char *argv[];
{
    extern int getopt();
    extern int optind;
    extern char *optarg;
    int optchar;

    char *progname = "soap-group";
    unsigned int max_Kbytes = DEF_MAX_KBYTES;   /* for cache size */
    int init_modlist = 0;
    int forced = 0;
    int min_copies = 0;
    int autoformat = 0;
    int forceformat = 0;
    int ncpu;

    progname = argv[0];
    while ((optchar = getopt(argc, argv, "FMadk:r:x?")) != EOF) {
        switch (optchar) {
	case 'a':
	    autoformat = 1;
	    break;
	case 'd':
	    Debugging = 1;
	    break;
	case 'x':
	    forceformat = 1;
	    break;
        case 'k':
            max_Kbytes = atoi(optarg);
            if (max_Kbytes < MIN_KBYTES || max_Kbytes > MAX_KBYTES) {
                fprintf(stderr, "%s: -k %d: bad size (allowed %d..%d)\n",
                        progname, (int)max_Kbytes, MIN_KBYTES, MAX_KBYTES);
                exit(1);
            }
            break;
	case 'r':
	    min_copies = atoi(optarg);
	    break;
	case 'F':
	    forced = 1;
	    break;
	case 'M':
	    init_modlist = 1;
	    break;
        case '?':
        default:
            usage(progname);
            break;
        }
    }

    if (optind != argc - 2) {
        usage(progname);
    }

    sp_set_whoami(atoi(argv[optind]));
    ncpu = atoi(argv[optind+1]);

    sp_doinit(max_Kbytes);

    if (init_modlist) {
	/* Initialise the modifications list module.
	 * The exact place of the modification list is fetched from the 
	 * string & capability environment.
	 */
	errstat err;

	if ((err = ml_init(autoformat, forceformat)) != STD_OK) {
	    /* We cannot ignore this error code, because otherwise
	     * we might lose a few (possibly vital) directory updates.
	     */
	    panic("modlist initialisation failed (%s)", err_why(err));
	}
    }

    startgroup(min_copies, ncpu);
    /* startgroup only returns when the server is supposed to shut down */

    exit(0);
}
