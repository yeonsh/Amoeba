/*	@(#)rm.c	1.3	96/03/04 14:05:24 */
/* rm - remove files			Author: Adri Koppes */
/* modified to use getopt and give better error messages: gregor */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#ifndef PATH_MAX
#define PATH_MAX	255
#endif

int     errors = 0;
int     fflag = 0;
int     iflag = 0;
int     rflag = 0;
int	exstatus;

extern char *optarg;
extern int optind;

char * Progname;

struct s_list {
	struct stat	*stat_p ;
	struct s_list	*next ;
} ;
char    name[PATH_MAX+2];
int     pathlen;

/* Forward */
void doremove _ARGS(( struct s_list * statlist ));


main(argc, argv)
int     argc;
char   *argv[];
{
    int	c;
    int i;

    Progname = argv[0];
    while ((c = getopt(argc, argv, "fir")) != EOF)
    {
	switch (c)
	{
	    case 'f': 
		fflag++;
		break;
	    case 'i': 
		iflag++;
		break;
	    case 'r': 
		rflag++;
		break;
	    default: 
		fprintf(stderr, "%s: unknown option\n", Progname);
		usage();
		break;
	}
    }

    if (optind >= argc)
	usage();
    for (i = optind; i < argc; i++) {
	(void)strcpy(name,argv[i]) ;
	pathlen= strlen(name) ;
	doremove((struct s_list *)NULL);
    }
    if (fflag)
	exstatus = 0;
    else
	exstatus = (errors == 0 ? 0 : 1);
    exit(exstatus);
    /*NOTREACHED*/
}


usage()
{
    fprintf(stderr, "Usage: %s [-fir] file ...\n", Progname);
    exit(1);
}


void
doremove(statlist)
    struct s_list *statlist;
{
    struct stat s;
    struct s_list newstat;
    struct s_list *travel;
    DIR	   *dirp;
    struct dirent  *dp;
    int    prevpl;

    if (stat(name, &s)) {
	if (!fflag)
	    fprintf(stderr, "%s: %s non-existent\n", Progname, name);
	errors++;
	return;
    }
    if (iflag) {
	fprintf(stderr, "%s: remove %s ? ", Progname, name);
	if (!confirm())
	    return;
    }
    if (S_ISDIR(s.st_mode)) {
	if (rflag) {
	    for ( travel= statlist ; travel ; travel=travel->next ) {
		if ( s.st_ino==travel->stat_p->st_ino &&
		     s.st_dev==travel->stat_p->st_dev ) {
		    if ( !fflag ) fprintf(stderr,"%s: probable recursive directory %s\n",
					Progname, name) ;
		    return ;
		}
	    }
	    newstat.stat_p= &s;
	    newstat.next= statlist;

	    prevpl=pathlen ;
	    if ((dirp = opendir(name)) == 0) {
	    	if (!fflag)
		    fprintf(stderr, "%s: can't open %s\n", Progname, name);
		errors++;
		return;
	    }
	    while ((dp = readdir(dirp)) != 0) {
		if (strcmp("..", dp->d_name) && strcmp(".", dp->d_name)) {
		    name[prevpl]= '/' ;
		    pathlen = prevpl+1+strlen(dp->d_name) ;
		    if ( pathlen>PATH_MAX ) {
			name[prevpl+1]=0 ;
			if ( !fflag ) {
			    fprintf(stderr,"%s: pathname too long: %s%s\n",
				Progname,name,dp->d_name) ;
			}
			pathlen=prevpl ;
			continue ;
		    }
		    (void) strcpy(&name[prevpl+1], dp->d_name);
		    doremove(&newstat);
		    pathlen=prevpl ; name[pathlen]=0 ;
		}
	    }
	    (void)closedir(dirp);
	    rem_dir(name);
	}
	else {
	    if (!fflag)
		fprintf(stderr, "%s: %s is a directory\n", Progname, name);
	    errors++;
	    return;
	}
    }
    else {
	if (access(name, 2) && !fflag) {
	    fprintf(stderr, "%s: remove %s (mode = %o) ? ",
					Progname, name, (s.st_mode & 0777));
	    if (!confirm())
		return;
	}
	if (unlink(name)) {
	    if (!fflag)
		fprintf(stderr, "%s: %s not removed\n", Progname, name);
	    errors++;
	}
    }
}

rem_dir(name)
char   *name;
{
    if (rmdir(name) < 0 && !fflag ) {
	fprintf(stderr, "%s: can't rmdir %s\n", Progname, name);
    	errors++;
    }
}

confirm() {
    char    c, t;

    (void) fflush(stderr);
    (void) read(0, &c, 1);
    t = c;
    while (t != '\n')
	if (read(0, &t, 1) <= 0)
		break;
    return (c == 'y' || c == 'Y');
}
