/*	@(#)s_drfile.c	1.1	91/11/21 11:46:52 */
/*
**		S _ D R F I L E
**
**   This file contains routines for opening and closing the draft file
**   and for truncating the draft file.
**
**
**	R E V I S I O N  H I S T O R Y
**
**	03/31/83  GWH	Split the SEND program into component parts
**			This module contains dropen and drclose.
**
*/
#define NOCHMOD
#include "./s.h"
#include "./s_externs.h"

dropen (seektype)
    int seektype;
{
    extern long lseek ();

    if (drffd <= 0)
    {
	if ((drffd = open (drffile, 2)) < 0)
	{
	    printf ("Unable to open draft '%s'.\n", drffile);
	    fflush (stdout);
	    s_exit (-1);
	}
#ifndef NOCHMOD
	if (chmod (drffile, 0400) < 0)
	{                         /* protect against crashes            */
	    printf ("Unable to protect draft '%s'.\n", drffile);
	    fflush (stdout);
	    s_exit (-1);
	}
#endif
    }
    lseek (drffd, 0L, seektype);
}

drclose ()
{
    if (drffd > 0)
    {
	close (drffd);
	drffd = 0;
#ifndef NOCHMOD
	if (chmod (drffile, sentprotect) < 0)
	{                         /* protect against crashes            */
	    printf ("Unable to unlock draft '%s'.\n", drffile);
	    fflush (stdout);
	}
#endif
    }
}

drempty ()
{
    drclose ();
    if ((drffd = creat(drffile, sentprotect)) >= 0)
        drclose ();
    else
    {
	printf(" Can't open draft file '%s'.\n", drffile);
	fflush (stdout);
	s_exit( NOTOK );
    }
}
