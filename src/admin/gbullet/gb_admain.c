/*	@(#)gb_admain.c	1.1	96/02/27 10:00:59 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Author: Ed Keizer
 */
 

/*
** badmin
**	Bullet server ommands to change member states.
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/name.h"
#include "module/mutex.h"
#include "module/stdcmd.h"
#include "ampolicy.h"

#include "module/disk.h"
#include "gbullet/bullet.h"
#include "gbullet/inode.h"
#include "gbullet/superblk.h"

#include "stdio.h"
#include "stdlib.h"


static char *progname ;

extern	char *optarg ;
extern	int  optind;

static void
usage(name)
char *	name;
{
    fprintf(stderr,
	"Usage: %s [-m member] [-[tepdD]] [-f] [-w] ... diskcap\n", name);
    exit(1);
}

void do_wait(cap,member)
capability	*cap;
int		member;
{
	mutex	blocker ;

	mu_init(&blocker) ;
	mu_lock(&blocker) ;

	while(gb_state(cap,member,BS_TESTSTABLE|BS_STATE_FORCE)!=STD_OK) {
		mu_trylock(&blocker,(interval)4000) ; /* Four seconds */
	}
}
		

errstat
detach_server(cap,member,no_wait,flags)
capability	*cap;
int		member;
int		no_wait;
int		flags;
{
    errstat	err ;

    if ((err = gb_state(cap,member,BS_PASSIVE|flags)) != STD_OK)
    {
	fprintf(stderr, "%s: failed to go into passive mode: %s\n",
				progname, err_why(err));
	return 1;
    }
    if ((err = gb_repchk(cap)) != STD_OK)
    {
	fprintf(stderr, "%s: replication check failed: %s\n",
				progname, err_why(err));
	return 1;
    }
    if ( !no_wait ) do_wait(cap,member) ;
    if ((err = gb_state(cap,member,BS_DETACH|flags)) != STD_OK)
    {
	fprintf(stderr, "%s: failed to die: %s\n",
				progname, err_why(err));
	return 1;
    }
    if ((err = gb_repchk(cap)) != STD_OK)
    {
	fprintf(stderr, "%s: server was detached, but replication check failed: %s\n",
				progname, err_why(err));
	return 1;
    }
    return 0 ;
}

main(argc, argv)
int	argc;
char *	argv[];
{
    char *	diskname;
    capability	cap;
    errstat	err;
    int		opt;
    int		action;
    int		action_flag=0;
    int		full_detach=1 ;
    int		force_flag=0;
    int		member= -1;
    int		no_wait = 0 ;
    int		action_flags ;


   progname=argv[0] ;
   /* use diskname as a temporary */
   diskname=progname ;
   if ( diskname ) {
	while ( *diskname ) {
		if ( *diskname=='/' && *(diskname+1) ) {
			progname=diskname+1 ;
		}
		diskname++ ;
	}
   }
/* process arguments */
    while ((opt = getopt(argc, argv, "m:pfedDqtw")) != EOF)
	switch (opt)
	{
	case 'p':
	    if ( action_flag ) usage(progname);
	    action_flag=1 ;
	    action=BS_PASSIVE ;
	    break;
	case 'D':
	    if ( action_flag ) usage(progname);
	    action_flag=1 ;
	    full_detach=0 ;
	    action=BS_DETACH ;
	    break;
	case 'd':
	    if ( action_flag ) usage(progname);
	    action_flag=1 ;
	    action=BS_DETACH ;
	    break;
	case 'e':
	    if ( action_flag ) usage(progname);
	    action_flag=1 ;
	    action=BS_EXIT ;
	    break;
	case 't':
	    if ( action_flag ) usage(progname);
	    action_flag=1 ;
	    action=BS_TESTSTABLE ;
	    break;
	case 'f':
	    if ( force_flag ) usage(progname);
	    force_flag=1 ;
	    break;
	case 'w':
	    if ( no_wait ) usage(progname);
	    no_wait=1 ;
	    break;
	case 'm':
	    if ( member!= -1 ) usage(progname);
	    member= atoi(optarg) ;
	    break;
	default:
	    usage(progname);
	    /*NOTREACHED*/
	}

    if (optind != argc - 1)
    {
	usage(progname);
	/*NOTREACHED*/
    }
    if ( !action_flag )
    {
	usage(progname);
	/*NOTREACHED*/
    }
    action_flags=0 ;
    if ( force_flag ) action_flags |= BS_STATE_FORCE ;
    diskname = argv[optind];

    if ((err = name_lookup(diskname, &cap)) != STD_OK)
    {
	fprintf(stderr, "%s: directory lookup of '%s' failed: %s\n",
					argv[0], diskname, err_why(err));
	exit(1);
    }
    if ( action==BS_DETACH && full_detach ) {
	return detach_server(&cap,member,no_wait,action_flags);
    }

    if ( no_wait ) {
	fprintf(stderr,"%s: Warning: -w flag is only useful with -D\n",
		progname);
    }

    if ((err = gb_state(&cap,member,action|action_flags)) != STD_OK)
    {
	fprintf(stderr, "%s: failed: %s\n", argv[0], err_why(err));
	exit(1);
    }
    return 0;
}
