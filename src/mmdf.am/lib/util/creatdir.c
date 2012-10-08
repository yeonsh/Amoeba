/*	@(#)creatdir.c	1.1	92/05/14 10:39:14 */
#include "util.h"
#include <sys/stat.h>

/*              create a directory
 *
 *      build intervening directories, if nonexistant
 *      chown new directories to specified uid
 *
 *      some of the system calls are not checked because they can
 *      fail benignly, as when an intermediate directory is executable,
 *      but not readable or writeable.  the only test for ultimate
 *      success is being able to create and stat the final directory.
 */

creatdir (dirptr, mode, owner, group)
    register char *dirptr;     /* pathname to directory                */
    int mode;
    int owner,                  /* non-zero uid of for dir owner        */
	group;
{
    extern int errno;
    struct stat statbuf;
    int realid,
	effecid;
    int uid,
	gid;
    char shcmd[128];
    register char *partpath;
    register char *nptr;       /* last char in partial pathname        */

    if (dirptr == (char *) 0 || isnull (*dirptr))
	 return (NOTOK);      /* programming error                      */

    if (owner != 0)             /* coerced ownship requested            */
    {                           /* noop it, if under requested id's     */
	getwho (&realid, &effecid);
	uid = realid;
	getgroup (&realid, &effecid);
	gid = realid;
	if( uid && (uid != owner || gid != group ))
	    return( NOTOK );
    }

    (void) strcpy (shcmd, "mkdir ");   /* initialize string with command */
    partpath = &shcmd[strlen (shcmd)];

    for (nptr = partpath, *nptr++ = *dirptr++; ; *nptr++ = *dirptr++)
	switch (*dirptr)
	{
	    case '\0':
	    case '/':
		*nptr = '\0';
		if (stat (partpath, &statbuf) < 0)
		{               /* should we try to creat it?           */
#ifndef V4_2BSD
		    system (shcmd);
				/* don't check if it succeeded          */
#else /* V4_2BSD */
#ifdef AMOEBA
		    mkdir (partpath, mode);
#else
		    mkdir (partpath);
#endif
#endif /* V4_2BSD */
		    if (owner != 0)
			chown (partpath, owner, group);

		    chmod (partpath, mode);
		}
		if (isnull (*dirptr))
		    return ((stat (partpath, &statbuf) < 0) ? NOTOK : OK);
	}
}
