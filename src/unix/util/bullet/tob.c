/*	@(#)tob.c	1.3	94/04/07 14:08:34 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** tob
**	Sends the file from unix called 'unixfile' to the amoeba bullet server
**	and registers the capability for the bullet file in the directory
**	server under the name 'bulletfile'.  If the third parameter is
**	specified it is the place to look up the bullet server capability.
**	Otherwise it sends it to the bullet server registered in public/bullet.
*/

#include "amoeba.h"
#include "ampolicy.h"
#include "cmdreg.h"
#include "stderr.h"
#include "bullet/bullet.h"

#include "module/name.h"
#include "module/stdcmd.h"

#include "stdio.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "stdlib.h"


main(argc, argv)
int		argc;
char **		argv;
{
    errstat	ux_lookup();
    errstat	ux_append();

    capability	bcap;
    capability	dcap;
    capability	fcap;
    errstat	status;
    int		fd;
    int		commit;
    struct stat	sbuf;
    char *	buf;
    char *	bulldir;
    errstat	(*do_lookup)() = name_lookup;
    errstat	(*do_append)() = name_append;
	    
    if (argc > 1 && strcmp(argv[1], "-n") == 0)
    {
	do_lookup = ux_lookup;
	do_append = ux_append;
	argv[1] = argv[0];
	argv++;
	argc--;
    }
    if (argc < 3 || argc > 4)
    {
	fprintf(stderr, "Usage: %s [-n] unixfile bulletfile [bulletserver]\n",
								    argv[0]);
	exit(1);
    }

/*
** Open the unix file, get its size, malloc a buffer for it and read it in
*/
    if ((fd = open(argv[1], 0)) < 0)
    {
	fprintf(stderr, "%s: open failed for `%s': ", argv[0], argv[1]);
	perror("");
	exit(1);
    }
    if (fstat(fd, &sbuf) < 0)
    {
	fprintf(stderr, "%s: fstat failed for `%s': ", argv[0], argv[1]);
	perror("");
	exit(1);
    }
    if ((buf = malloc((unsigned) sbuf.st_size)) == (char *) 0)
    {
	fprintf(stderr, "%s: Cannot allocate %d bytes of memory\n",
							argv[0], sbuf.st_size);
	exit(1);
    }

    if (read(fd, buf, (int) sbuf.st_size) != sbuf.st_size)
    {
	fprintf(stderr, "%s: Read error on `%s': ", argv[0], argv[1]);
	perror("");
	exit(1);
    }
    (void) close(fd);

/*
** Find the capability for the bullet server
*/
    bulldir = argc == 3 ? DEF_BULLETSVR : argv[3];
    if ((status = (*do_lookup)(bulldir, &bcap)) != STD_OK)
    {
	fprintf(stderr, "%s: directory lookup of '%s' failed (%s)\n",
					    argv[0], bulldir, err_why(status));
	exit(1);
    }

/*
** Create the bullet file with safety (ie. wait till it is on disk before
** returning.
*/
    commit = BS_SAFETY | BS_COMMIT;
    if ((status = b_create(&bcap, buf, sbuf.st_size, commit, &fcap)) < 0)
    {
	fprintf(stderr, "%s: Cannot create bulletfile (%s)\n",
						    argv[0], err_why(status));
	exit(1);
    }

/*
** Don't continue if there is an extant directory entry with the same name
** (if it is there).
*/
    status = (*do_lookup)(argv[2], &dcap);
    switch (status)
    {
    case STD_OK:
	fprintf(stderr, "%s: file name '%s' already exists\n",
							    argv[0], argv[2]);
	(void) std_destroy(&bcap);
	exit(1);
    case STD_NOTFOUND:
	break;
    default:
	fprintf(stderr, "%s: directory lookup of '%s' failed (%s)\n",
					    argv[0], argv[2], err_why(status));
	(void) std_destroy(&bcap);
	exit(1);
    }

/*
** Install new capability for bullet file in directory server
*/
    if ((status = (*do_append)(argv[2], &fcap)) != STD_OK)
    {
	fprintf(stderr, "%s: Cannot store bullet file capability (%s)\n",
						    argv[0], err_why(status));
	(void) std_destroy(&bcap);
	exit(1);
    }
    exit(0);
}


errstat
ux_lookup(name, cap)
char *		name;
capability *	cap;
{
    int fd;
    int err = STD_OK;

    if ((fd = open(name, 0)) < 0)
	return STD_NOTFOUND;
    if (read(fd, cap, sizeof(capability)) != sizeof(capability))
	err = STD_SYSERR;
    close(fd);
    return err;
}


errstat
ux_append(name, cap)
char *		name;
capability *	cap;
{
    int fd;
    int err = STD_OK;

    if ((fd = creat(name, 0666)) < 0)
	return STD_SYSERR;
    if (write(fd, cap, sizeof(capability)) != sizeof(capability))
	err = STD_SYSERR;
    close(fd);
    return err;
}
