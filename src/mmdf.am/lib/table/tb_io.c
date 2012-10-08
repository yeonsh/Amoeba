/*	@(#)tb_io.c	1.1	92/05/14 10:38:54 */
#include "util.h"
#include "mmdf.h"
#include "ch.h"                   /* has table state strcture def       */
/*
 *     MULTI-CHANNEL MEMO DISTRIBUTION FACILITY  (MMDF)
 *
 *
 *     Copyright (C) 1979,1980,1981  University of Delaware
 *
 *     Department of Electrical Engineering
 *     University of Delaware
 *     Newark, Delaware  19711
 *
 *     Phone:  (302) 738-1163
 *
 *
 *     This program module was developed as part of the University
 *     of Delaware's Multi-Channel Memo Distribution Facility (MMDF).
 *
 *     Acquisition, use, and distribution of this module and its listings
 *     are subject restricted to the terms of a license agreement.
 *     Documents describing systems using this module must cite its source.
 *
 *     The above statements must be retained with all copies of this
 *     program and may not be removed without the consent of the
 *     University of Delaware.
 *
 *
 *     version  -1    David H. Crocker    March   1979
 *     version   0    David H. Crocker    April   1980
 *     version  v7    David H. Crocker    May     1981
 *     version   1    David H. Crocker    October 1981
 *
 */

/*  READ "NETWORK" HOST NAME TABLES
 *
 *  BNF for the file is:
 *
 *      entries =   entries entry
 *      entry   =   name separator value eol
 *      name    =   {string of chars not containing a <separator>}
 *      separator=  {see the chars in _hkeyend[]}
 *      value   =   {string of chars not containing an <eol>}
 *      eol     =   {see the chars in _hvalend[]}
 *
 *      where
 *
 *          name    is a key
 *          value   is any relevant text, usually the host "address"
 *
 *  There may be any number of entries with the same value string,
 *  thereby allowing name aliases, such as UDel-EE, UDel, and UDEE.
 *
 *  The key of the first entry with a given value is taken as the
 *  "official" "name" of that "address".  (Sorry about all the
 *  quotation marks.)
 *
 *  Turns out that the local machine's alias file can also be accessed
 *  by these routines.
 *
 *  Any number of tables may be supported simultaneously, since a
 *  single table's state information structure is maintained externally,
 *  and passed with each call, through the parameter netinfo.
 *  The state structure's place for the file descriptor MUST
 *  initially be zero.
 *
 *  "Hard" failures are considered not acceptable and hence cause a
 *  call to the external routine err_abrt(); to change this policy,
 *  just change the calls to err_abrt() to be simple failure returns.
 *  The rest of the code should run "transparently".
 */

/*  Sep 81  Dave Crocker      added ch_tclose & ch_tfree, to allow
 *                            handling file descriptor overflow
 *  May 82  Dave Crocker      add ch_tread, to focus input
 *  Jun 82  Dave Crocker      split ch_tio off from ch_table
 *  Dec 82  Doug Kingston     Fixed bug in tb_read() that caused it to
 *                            ignore EOF's.   (DPK@BRL)
 *  Apr 83  Steve Kille       Change it back again
 */

extern struct ll_struct   *logptr;
extern char *tbldfldir;
extern Table **tb_list;         /* all known tables                     */
				/* SEK change syntax                    */
extern int   tb_numtables;      /* how many of them there are           */
extern char  *index();
extern char  *compress();

LOCVAR char _hkeyend[] = ": \t\n\377",
				  /* chars which delimit end of key     */
	    _hvalend[] = "\n\000\377";
				  /* chars which delimit end of value   */
LOCVAR short tb_nopen;            /* number of opened file descriptors  */

/* ******************  BASIC FILE ACTION  *************************** */

tb_open (table, fromstart)        /* gain access to a table     */
    register Table  *table;       /* table's state information          */
    int     fromstart;            /* ok to reset, if already open?      */
{
    extern int   errno;           /* in case open fails                 */
    char temppath[128];


/*  currently, there is one file (descriptor) per table.  for a large
 *  configuration, you will run out of fd's.  until/unless you run
 *  a single-file dbm-based version, there is a hack, below, which
 *  limits the number of open file-descriptors to 6.  Trying to open
 *  a 7th will cause a currently-opened one to be closed.
 */

    switch ((int)table)
    {
	case 0:
	case NOTOK:
	    return (FALSE);       /* can't open what isn't specified    */
    }
    switch ((int)(table -> tb_fp))
    {
	case 0:                   /* not opened yet                     */
/*HACK*/    table -> tb_pos = 0L;
	    if (table -> tb_file != 0)
	    {                     /* we have a path string              */
		if (tb_nopen < 6 || tb_free ())
		{
		    getfpath (table -> tb_file, tbldfldir, temppath);
				  /* add on directory portion           */
#ifdef DEBUG
		    ll_log (logptr, LLOGBTR, "opening '%s'", temppath);
#endif
		    if ((table -> tb_fp = fopen (temppath, "r")) != NULL)
		    {
			tb_nopen++;
			break;    /* all is well                        */
		    }
		}
		ll_err (logptr, (errno == ENOENT) ? LLOGPTR : LLOGTMP,
		    "Error opening %s name table '%s' (numfiles open = %d)",
				table -> tb_show, temppath, tb_nopen);
				  /* major only if file exists          */
	    }
	    table -> tb_fp = (FILE *) EOF;
				  /* no path str -> quietly fail        */
				  /* DROP ON THROUGH                    */
	case NOTOK:
	    return (FALSE);       /* failed earlier try                 */

	default:
	    if (fromstart)        /* always start from beginning?       */
		tb_seek (table, 0L);
    }
    return (TRUE);
}
/**/

tb_close (table)                  /* close  a table                     */
register Table *table;
{
    switch ((int)table)
    {
	case 0:
	case NOTOK:
	    return (FALSE);       /* can't close what isn't specified   */
    }
    switch ((int)(table -> tb_fp))
    {
	case NOTOK:
	    table -> tb_fp = 0; /* mark as free                       */
	case 0:                   /* not opened yet                     */
	    return (FALSE);       /* no close performed                 */

	default:
	    fclose (table -> tb_fp);
	    table -> tb_fp = 0; /* mark as free                       */
	    tb_nopen--;
	    return (TRUE);
    }
    /* NOTREACHED */
}

tb_free ()                       /* create a free file descriptor      */
{
/*  step from the lowest search priority to the highest, looking for a
 *  channel to close.  return success as soon as one is done.
 */
    register Table **tableptr;
#ifdef DEBUG
    ll_log (logptr, LLOGFTR, "tb_free ()");
#endif
    for (tableptr = tb_list + tb_numtables - 1;
					tableptr >=  tb_list; tableptr--)
    {
	if (tb_close (*tableptr))
	    return (TRUE);        /* got a channel closed               */
    }
    return (FALSE);               /* couldn't get any closed            */
}

tb_seek (table, thepos)           /* reset table position               */
register Table *table;
long thepos;
{
    switch ((int)table)
    {
	case 0:
	case NOTOK:
	    return (NOTOK);       /* can't open what isn't specified    */
    }
    switch ((int)(table -> tb_fp))
    {                             /* not opened yet                     */
	case 0:
	case EOF:
	    return (NOTOK);
    }
    fseek (table -> tb_fp, thepos, 0);
/*HACK*/ table -> tb_pos = thepos;
    return ((ferror (table -> tb_fp)) ? NOTOK : OK);
}
/* *****************  READ THE NAME TABLE  ************************** */

/*
 *  This routine used not to recognise comments, this meant that dbmbuild
 *  could skip real records in the following case.
 *  #<NL>
 *  nott:nott
 *  would be read as key = '#' value = 'nott:nott' and dbmbuild would
 *  throw it away as a comment. (JPO)
 */
tb_read (table, key, value)       /* read a table's record        */
    register Table *table;
    char *key,
	 *value;
{
    register int len;
    register FILE *fp;
    char terminator;

    fp = table -> tb_fp;

    if (feof (fp))
	return (FALSE);

    while ((len = qread (fp, key, LINESIZE, _hkeyend, '\\')) > 0)
    {
/*HACK*/    table -> tb_pos += len;
	if( key[0] == '\n' )
	    continue;	    /* blank line */
	if( key[0] == '#' )
	    if( key[len - 1] == '\n' )
		continue;   /* have read up to the newline */
	    else
	    {
		if((len = gcread(fp, value, LINESIZE, _hvalend)) > 0)
		{
/*HACK*/	    table -> tb_pos += len;
		    continue;	/* skip to end of line */
		}
		else return (FALSE);	/* EOF I guess */
	    }

    	terminator = key[len - 1];
	key[len - 1] = '\0';  /* get rid of terminator character */
	if (len > 1)
	    compress (key, key);

	if (index(_hvalend, terminator) > (char *)NULL)
 	    value[0] = '\0';		/* whole line was a key */
        else {
	    if ((len = gcread (fp, value, LINESIZE, _hvalend)) > 0)
	    {
/*HACK*/	table -> tb_pos += len;
		value[len - 1] = '\0';
		if (len > 1)
		    compress (value, value);
	    }
	    else               /* Don't worry about errors here... */
		value[0] = '\0';
        }
	return (TRUE);

    }
    if( len < 0 )        /* (len < 0) indicating IO error */
	err_abrt (RP_FIO, "Error reading %s name table '%s'",
		table -> tb_show, table -> tb_file);

    return (FALSE);             /* EOF on entry boundary,  DPK@BRL */
}
