/*	@(#)s_onint.c	1.1	91/11/21 11:49:01 */
/* name:
	onint, onint2, onint3

function:
	to take appropriate actions before terminating from a del.

calls:
	exit

called by:
	input

*/

/*
**	R E V I S I O N  H I S T O R Y
**
**	03/31/83  GWH	Split the SEND program into component parts
**			This module contains : onint, onint2,
**			onint3, confirm.
**
**	10/05/83  GWH	Fixed s_exit so that the draft file will not be
**			removed in abnormal terminations.
**
*/

#include "./s.h"
#include "./s_externs.h"

sigtype
onint ()
{
    printf (" XXX\n");
    fflush (stdout);
    signal (SIGINT, onint);
    truncate( tmpdrffile, 0);
    if (inrdwr)
	aborted = TRUE;
    else
	longjmp (savej, 1);
}

sigtype
onint2 ()
{
    printf (" XXX\n");
    fflush (stdout);
    s_exit (-1);
}

sigtype
onint3 ()
{
    signal (SIGINT, onint3);

    if (body)
	longjmp (savej, 1);

    s_exit (0);
}

confirm ()
{
    char answer[32];

    printf (" [Confirm] ");
    gather (answer, sizeof (answer));
    return (prefix ("yes", answer) ? TRUE : FALSE);
}

/* preliminary exit routine to clean up draft file */
s_exit( xcode )
int xcode;
{
	if( xcode == 0 || !body )
		unlink(drffile);
	unlink( tmpdrffile );
	exit( xcode );
}
