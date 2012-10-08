/*	@(#)lo_wtmail.c	1.3	93/12/15 14:47:30 */
/*
 *	Deliver to the users mailbox
 *	Does most of the interesting, non-mechanical work. It tries hard
 *	not to parse the message unless needed.
 *
 *	Severe hacking done this file reconstructed from bits of old
 *	qu2lo_send.c and a lot of new stuff.
 *
 *	Julian Onions <jpo@cs.nott.ac.uk>	August 1985
 */

#include "util.h"
#include "mmdf.h"
#include "phs.h"
#include "ap.h"
#include "ch.h"
#include <pwd.h>
#include <sys/stat.h>
#include <signal.h>
#include <sgtty.h>
#include "adr_queue.h"
#include "hdr.h"

extern Chan	*chanptr;
extern LLog	*logptr;

extern int	errno;

extern int	sentprotect;
extern int	flgtrest;
extern int	numfds;
extern long	qu_msglen;
extern jmp_buf  timerest;

extern char     *strcpy();

extern char *mldflfil;		/* name of local mailbox file		*/
extern char *mldfldir;		/* name of local mailbox directory	*/
extern char *delim1;		/* prefix and suffix lines for		*/
extern char *delim2;		/*	delivery mailbox/msg		*/
extern char *dlvfile;		/* name of user deliver control file	*/
extern char *sysdlvfile;	/* name of system deliver control file  */
extern char *lckdfldir;		/* directory to lock in			*/

LOCVAR int	mbx_fd = -1;	/* handle on recipient mailbox		*/

extern char lo_info[];		/* type of delivery		*/
extern char lo_sender[];	/* return address for message	*/
extern char lo_adr[];		/* destination address from deliver */
extern char lo_replyto[];	/* the reply address		*/
extern char lo_size[];
extern char *lo_parm;		/* parameter portion of address */
extern struct passwd *lo_pw;	/* passwd struct for recipient  */

extern  long	lseek();
extern	char	*strdup();
extern	char	*expand();

int	sigpipe;		/* has pipe gone bad? */
sigtype	onpipe();		/* catch that pipe write failure */
LOCVAR char mbx_wasclear;	/* this message the first in mbox?	*/

LOCFUN lo_dofile(), qu2lo_txtcpy(), lo_pwait(), mbx_create(), mbx_close(),
	mbx_chkdelim(), hdr_line(), hdr_parse(), lookup(), setupenv();

/*	Structure used in lookup routines */

typedef struct {
	char	*l_key;
	short	l_val;
} Lookup;

# define	MAXLINES	200	/* max no. of lines in .maildelivery */

/* the action types */
# define	M_FILE		1
# define	M_PIPE		2
# define	M_DESTROY	4

/* the result types */
# define	M_ACCEPT	01
# define	M_REJECT	02
# define	M_CONDACC	03

/* the special headers */
# define	M_DEFAULT	1
# define	M_TRUE		2
# define	M_ADDRESS	3
# define	M_SENDER	4

typedef struct mdlvry {
	char	*m_header;		/* the header string	*/
	char	*m_pattern;		/* the pattern to match */
	char	*m_options;		/* the options string	*/
	unsigned short m_parse	  : 1;  /* header type		*/
	unsigned short m_special  : 1;  /* special actions?	*/
	unsigned short m_hit	  : 1;  /* have we hit		*/
	unsigned short m_ar	  : 2;  /* accept reject bits	*/
	unsigned short m_dollar   : 3;  /* the dollar field	*/
	unsigned short m_action   : 4;  /* the action to do	*/
	unsigned short m_hdrtype  : 4;  /* the special hdr type */
} Mdlvry;

Lookup  spec_hdrs[] = { /* the table of special headers */
	"default",	M_DEFAULT,
	"true",		M_TRUE,
	"addr",		M_ADDRESS,
	"source",	M_SENDER,
	"*",		M_TRUE,
	0,		0,
};

Lookup  actions[] = {	/* the table of actions */
	"pipe",		M_PIPE,
	"qpipe",	M_PIPE,
	"file",		M_FILE,
	"destroy",	M_DESTROY,
	"|",		M_PIPE,
	"^",		M_PIPE,
	">",		M_FILE,
	0,	0,
};

Lookup  dolfields[] = {
	"$(reply-to)",  1,
	0,		0,
};

/*
 *	Give the user something nice in the environment.
 */
LOCVAR char *envp[] = {
	(char *) 0,		/* HOME=xxx  */
	(char *) 0,		/* SHELL=xxx */
	(char *) 0,		/* USER=xxx  */
	0
};

/*
 *	Some things that can be macro expanded.
 */
LOCVAR char *vararray[] = {
	"sender",	lo_sender,	/* 0/1 */
	"address",	lo_adr,		/* 2/3 */
	"info",		lo_info,	/* 4/5 */
	"reply-to",	lo_replyto,	/* 6/7 */
	"size",		lo_size,	/* 8/9 */
	0,		0
};

/*
 * characters that need to be padded to pass to a shell
 */

LOCVAR char *padchar = " \\\"'%$@#?!*|<>()&^~=+-;";


/*  */

lo_slave ()
{
	char	buf[2*LINESIZE];
	register int	  result;
	Mdlvry	mdlv;	/* A fake construct */
	Mdlvry	*mp;

#ifdef DEBUG
	ll_log (logptr, LLOGFTR, "lo_slave()");
#endif
	mp = &mdlv;
	printx ("\n");
	/* The production system of delivery attempts */

	/* FIRST Attempt: alias file specified file or pipe delivery */

	switch (*lo_parm) {
	case '|':	/* send to special process	    */
		printx ("(sending message to piped process)\r\n\t");
		sprintf (buf, "default - | A \"%s\"", lo_parm+1);
		if( parse_line(mp, buf) != OK)
			ll_log (logptr, LLOGTMP, "problem parsing '%s'", buf);
		return ( lo_dorules(mp, mp+1) );
	case '^':
		printx ("(sending message to process unformatted)\r\n\t");
		sprintf (buf, "default - ^ A \"%s\"", lo_parm + 1);
		if( parse_line (mp, buf) != OK)
			ll_log (logptr, LLOGTMP, "problem parsing '%s'", buf);
		return ( lo_dorules(mp, mp+1) );
	case '/':
		printx ("(placing into mail file '%s')\r\n\t", lo_parm+1);
		sprintf (buf, "default - > A \"%s\"", lo_parm +1 );
		if( parse_line (mp, buf) != OK)
			ll_log (logptr, LLOGTMP, "problem parsing '%s'", buf);
		return (lo_dorules (mp, mp + 1));
	}

	/*
	 * SECOND Attempt: user's .maildelivery file
	 */
	sprintf(buf,"%s/%s/%s",mldfldir,lo_pw->pw_name,dlvfile);
	if (rp_isgood (result = lo_dlvfile(buf))
			|| result != RP_MECH)
		return (result);

	/* THIRD Attempt: system delivery file */

	if (rp_isgood (result = lo_dlvfile(sysdlvfile))
			|| result != RP_MECH)
		return (result);

	/* FOURTH Attempt: regular deliver to the mailbox */

	printx ("trying normal delivery\r\n");
/*	sprintf (buf, "%s/%s",
		(mldfldir == 0 || isnull(mldfldir[0])) ? "." : mldfldir,
		(mldflfil == 0 || isnull(mldflfil[0])) ? lo_pw->pw_name : mldflfil);*/
	sprintf(buf,"%s/%s/%s",mldfldir,lo_pw->pw_name,mldflfil);

	return (lo_dofile (buf));
}
/**************** FILE DELIVERY ROUTINES *************** */

LOCFUN
lo_dofile(mboxname)
char	*mboxname;
{
	register int	retval;

#ifdef	DEBUG
	ll_log (logptr, LLOGBTR, "lo_dofile(%s)", mboxname);
#endif /* DEBUG */
	printx ("\tdelivering to file '%s'", mboxname);

	if (rp_gval(retval = mbx_open (mboxname)) == RP_LOCK) {
		printx (", locked out\r\n");
		return (RP_LOCK);
	} else if (rp_isbad(retval)) {
		printx (", failed\r\n");
		return (retval);
	}
	retval = qu2lo_txtcpy (mbx_fd, TRUE);
	mbx_close (mboxname);
	printx (rp_isgood (retval) ? ", succeeded\r\n" : ", failed\r\n");
	return (retval);
}

LOCFUN
qu2lo_txtcpy (ofd, tstdelim)	/* copy the text of the message	      */
register int ofd;		/* where to send the text */
register int tstdelim;		/* should we check for message delimiters? */
{
	register int offset;
	int	len;
	int	result;
	char	buffer[BUFSIZ];

#ifdef DEBUG
	ll_log (logptr, LLOGBTR, "qu2lo_txtcpy()");
#endif

	qu_rtinit (0L);
	len = sizeof(buffer);
	while (rp_gval (result = qu_rtxt (buffer, &len)) == RP_OK) {
		if (tstdelim) {	   /* text may not contain a delimiter	 */
			/*
			 *  Check for occurences of the message delimiter.
			 *  If we find any, change the string by changing
			 *  the first char to a space.
			 */
			if( isstr(delim1) )
				for (offset = 0;
				     (offset = strindex (delim1, buffer)) >= 0;
					buffer[offset] = ' ');

			if( isstr(delim2) )
				for (offset = 0;
				     (offset = strindex (delim2, buffer)) >= 0;
					buffer[offset] = ' ');
		}
		if (write (ofd, buffer, len) != len) {
			ll_err (logptr, LLOGTMP, "error writing out text");
			return (RP_LIO);
		}
		len = sizeof(buffer);
	}

	if (rp_gval (result) != RP_DONE)
		return (RP_LIO);	 /* didn't get it all across? */

	return (RP_MOK);	      /* got the text out		    */
}
/**************** PIPE DELIVERY ROUTINES *************** */

LOCFUN
lo_dopipe(prog)
char	*prog;
{
	int	childid;		/* child does the real work */
	int	ofd;			/* what to write on */
	char	tmpbuf[2*LINESIZE];
	char	buffer[BUFSIZ];
	sigtype	(*savepipe)();
	Pip	pipdes;
	int	result;
	int	len;

#ifdef	DEBUG
	ll_log (logptr, LLOGBTR, "lo_dopipe(%s)", prog);
#endif /* DEBUG */


	if( pipe(pipdes.pipcall) == NOTOK)
		return (RP_LIO);

	ll_close(logptr);
	switch (childid = fork ()) {
	case NOTOK:
		(void) close(pipdes.pip.prd);
		(void) close(pipdes.pip.pwrt);
		return (RP_LIO);	/* problem with system */

	case OK:
		/* Child */
		lo_padadr();
		/* first - expand out any '$' keywords */
		expand (tmpbuf, prog, vararray);
		printx ("\tdelivering to pipe '%s'", tmpbuf);
		fflush (stdout);
		(void) close(0);
		dup (pipdes.pip.prd);

		setupenv();

		execle ("/bin/sh", "sh", "-c", tmpbuf, (char *)0, envp);
		ll_log( logptr, LLOGFAT, "Can't execl /bin/sh (%d)",errno);
		exit (RP_MECH);
	}

	/* Parent */
	/* our job is to pass on the message to the process */
	(void) close (pipdes.pip.prd);
	ofd = pipdes.pip.pwrt;  /* nicer name */
	qu_rtinit(0L);		/* init the message */
	savepipe = signal(SIGPIPE, onpipe);
	sigpipe = FALSE;
	len = sizeof(buffer);
	while (rp_gval(result = qu_rtxt(buffer, &len)) == RP_OK)
	{
		if( sigpipe )	/* childs had enough */
			break;
		if( write(ofd, buffer, len) != len)
		{
			if( errno != EPIPE)
				ll_err (logptr, LLOGTMP, "error on pipe %d",
					errno);
			break;
		}
		len = sizeof(buffer);
	}
	(void) close(ofd);	/* finished with pipe */
	signal(SIGPIPE, savepipe);	/* restore the dying signal */
	if(rp_gval(result) != RP_OK && rp_gval(result) != RP_DONE)
		return result;
	return (lo_pwait (childid, prog)); /* .... and wait */
}

LOCFUN
lo_pwait (procid, prog)		/* wait for child to complete	      */
int	procid;			/* id of child process		      */
char	*prog;
{
	int status;

#ifdef DEBUG
	ll_log (logptr, LLOGBTR, "lo_pwait ()");
#endif

	if (setjmp (timerest) == 0)
	{			/* alarm is in case user's pgm hangs	*/
		flgtrest = TRUE;
		/*NOSTRICT*/
		s_alarm ((unsigned) (qu_msglen / 20) + 300);
		errno = 0;
		status = pgmwait (procid);
		s_alarm (0);
		if (status == NOTOK) {
			/* system killed the process */
			ll_err (logptr, LLOGTMP, "bad return: (%s) %s [wait=NOTOK]",
				lo_pw->pw_name, prog);
			return (RP_LIO);
		}
		ll_log (logptr, LLOGBTR, "(%s) %s [%s]",
			lo_pw->pw_name, prog, rp_valstr (status));
		if( status == 0)	/* A-OK */
		{
			printx (", succeeded\r\n");
			return (RP_MOK);
		}
		else if(status == 1)	/* command not found - ECB */
		{
			printx (", failed (PATH)\r\n");
			return (RP_MECH);
		}
		else if( rp_gbval(status) == RP_BNO)	/* permanent error */
		{
			printx (", failed (perm)\r\n");
			return (RP_NO);
		}
		else	/* temp error */
		{
			printx (", failed (temp)\r\n");
			return (RP_MECH);
		}
		/* NOTREACHED */
	}
	else
	{
		flgtrest = FALSE;
		printx (", user program taking too long");
		ll_log (logptr, LLOGGEN, "user program taking too long - killing");
#ifndef V4_2BSD
		kill (procid, SIGKILL); /* we're superuser, so always works */
#else /* V4_2BSD */
		killpg (procid, SIGKILL); /* we're superuser, so always works*/
#endif /* V4_2BSD */
		return (RP_TIME);
	}
}

/* ensure that all addresses can be passed to shell */
lo_padadr()
{
    static done_pad = 0;
    register int i;
    register char *c1, *c2, *p;
    char tmp[2 * LINESIZE];

    if (done_pad)
	return;

    for(i=0; vararray[i] != 0; i += 2)
    {
	(void) strcpy(tmp,vararray[i+1]);

	for(c1=tmp,c2=vararray[i+1]; *c1 != '\0'; c1++)
	{
	    for(p=padchar; *p != '\0';  p++)
	    {
		if (*p == *c1)
		{
		    *c2++ = '\\';
		    break;
		}
	    }

	    *c2++ = *c1;
	}
	*c2 = '\0';
    }
}

/********  (mbx_)  STANDARD DELIVERY TO MAILBOX STYLE FILE  *********** */

mbx_open (file)			/* stuff into mailbox		      */
char	*file;
{
	struct stat	mbxstat;
	register int	count;
	short	 retval;
	int	mbxmade;		/* Infinite loop preventer */

#ifdef DEBUG
	ll_log (logptr, LLOGBTR, "mbx_open (%s)", file);
#endif
	mbxmade = mbx_wasclear = FALSE;
reopen:
	if ((mbx_fd = lk_open (file, 2, (char *) 0, (char *) 0, 10)) < 0)
	{
		switch (errno) {
#ifdef EWOULDBLOCK
		case EWOULDBLOCK:
#endif /* EWOULDBLOCK */
		case EBUSY:
			printx (", mail file is locked");
			return (RP_LOCK);

		case ENOENT:	  /* doesn't exist */
			if (!mbxmade && rp_isgood (mbx_create (file))) {
				printx (", mail file created");
				mbxmade = mbx_wasclear = TRUE;
				goto reopen;
			}
			/* DROP THROUGH */
		default:
			ll_err (logptr, LLOGTMP, "can't open mailbox '%s'",
								file);
			return (RP_LIO);
		}
	}
	else
	{
		if (fstat (mbx_fd, &mbxstat) < 0)
		{
			ll_err (logptr, LLOGTMP, "can't fstat %s", file);
			return (RP_LIO);
		}
		mbx_wasclear = (st_gsize (&mbxstat) == 0L);
	}

	if (!mbx_wasclear && rp_isbad (retval = mbx_chkdelim ()))
	    return (retval);	      /* mbox has bad terminator	    */

	count = strlen (delim1);      /* write prefatory separator	    */
	if (write (mbx_fd, delim1, count) != count)
	{
		ll_err (logptr, LLOGTMP, "error writing delim1");
		return (RP_LIO);
	}

	return (RP_OK);
}
/**/

LOCFUN
mbx_create (mboxname)	/* create receiver's mailbox file     */
char	*mboxname;
{
  char temp[512];
#ifdef DEBUG
  ll_log (logptr, LLOGBTR, "mbx_create (%s) %o", mboxname,sentprotect);
#endif

  if ((mbx_fd = creat (mboxname, sentprotect)) < 0)
    {
      ll_err (logptr, LLOGFAT, "can't create mailbox '%s'",
	      mboxname);
      return (RP_LIO);
    }
  (void) close (mbx_fd);	/* unix create() forces wrong modes	*/

  sprintf(temp,"chm ff:0:0 %s",mboxname);
  system(temp);
  
  return (RP_OK);
}

LOCFUN
mbx_close (file)	/* done with mailbox	*/
char	*file;
{
	short	  count;

#ifdef DEBUG
	ll_log (logptr, LLOGBTR, "mbx_close ()");
#endif
	if (mbx_fd < 0)
	{
#ifdef DEBUG
		ll_log (logptr, LLOGFTR, "not open");
#endif
		return;
	}

	count = strlen (delim2);
	if (write (mbx_fd, delim2, count) != count)
	{	/* couldn't put ending delimiter on   */
		ll_err (logptr, LLOGTMP, "error writing delim2");
	}
	lk_close (mbx_fd, file, (char *) 0, (char *) 0);
	mbx_fd = -1;
}
/**/

LOCFUN
mbx_chkdelim () /* last msg delimited properly?	      */
{		/* check ending delimiter of last msg */
	int	count;
	char	ldelim[LINESIZE];
	struct stat statb;

#ifdef DEBUG
	ll_log (logptr, LLOGBTR, "mbx_chkdelim ()");
#endif

	count = strlen (delim2);

	if (lseek (mbx_fd, (long)(-count), 2) == (long) NOTOK ||
	    read (mbx_fd, ldelim, count) != count) {
	    printx(", mailbox is missing delimiter");
	    if (fstat(mbx_fd, &statb) == 0 && statb.st_size < count) {
	    	lseek(mbx_fd, 0L, 2);
	    	goto patch;
	    }
	    printx(", mailbox lseek/read errors");
	    return (RP_LIO);
        }

	if (count == 0)
		return (RP_OK);
	if (!initstr (delim2, ldelim, count))
	{	/* previous didn't end cleanly. patch */
		ll_err (logptr, LLOGTMP, "bad delim; patching");
patch:		printx("\n\tpatching in missing delimiter");

		if ((write (mbx_fd, "\n", 1) != 1) ||
		    (write (mbx_fd, delim2, count) != count))
		{			/* write separator	*/
			ll_err (logptr, LLOGTMP, "unable to patch");
			return (RP_LIO);
		}
	}
	return (RP_OK);
}
/******************* MAILDELIVERY HANDLING ROUTINES ***************/

/*
 *	Given a maildelivery format file, this routine parses the
 *	file into an array - then calls dorules to follow the actions.
 *	The file is either personal
 *		e.g. .maildelvery
 *	or else system wide
 *		i.e. /usr/lib/maildlvry
 */

lo_dlvfile (dlv_file)
char	*dlv_file;		/* the file to scan */
{
	register FILE *fp;
	struct stat sb;
	Mdlvry  thefile[MAXLINES];	/* the representation of the md file */
	register Mdlvry *mptr, *maxptr;	/* some handy pointers  */
	char	dlvline[LINESIZE];
	int	len;

#ifdef	DEBUG
	ll_log (logptr, LLOGBTR, "lo_dlvfile(%s)", seenull(dlv_file));
#endif /* DEBUG */

	if ((!isstr(dlv_file)) || (fp = fopen(dlv_file, "r")) == NULL)
		return (RP_MECH);

	/*		Security Check!
	 *
	 *  The file must owned by the person we are delivering to
	 *  or the superuser.  In addition, the file must not have
	 *  write permission to anyone accept the owner.   (DPK)
	 *  In the case of system wide file, this is constrained to
	 *  be root generally. (JPO)
	 */

	if (fstat (fileno(fp), &sb) < 0
#ifndef AMOEBA
		|| (sb.st_uid != lo_pw->pw_uid && sb.st_uid != 0)
#endif
/*		|| sb.st_mode & 022) {*/
	    ){
		printx("ownership problems on '%s'\n", dlv_file);
		ll_log (logptr, LLOGBTR, "ownership problems on '%s'",
				dlv_file);
		fclose(fp);
		return (RP_MECH);
	}

	for (mptr = thefile;
		(len = gcread(fp, dlvline, LINESIZE, "\n\377")) > 0
			&& mptr < &thefile[MAXLINES];)
	{
		if( dlvline[0] == '\n' || dlvline[0] == '#' )
			continue;	/* skip this line */

		dlvline[len - 1] = '\0'; /* zap the lf */

		if( parse_line(mptr, dlvline) == OK)
			mptr ++;
	}
	if ( mptr >= &thefile[MAXLINES] )
		ll_log (logptr, LLOGFST, "more than %d lines in %s (%s)",
			MAXLINES, dlv_file, lo_pw->pw_name);

	maxptr = mptr;
	fclose (fp);	/* OK, file finished, take a deep breath and ... */
	return lo_dorules(thefile, maxptr);
}

/*
 * apply the given rules. mptr is the begining of an array of
 * rules, mpmax is the last rule in the array + 1.
 */

lo_dorules(mptr, mpmax)	/* apply the maildelivwery rules */
Mdlvry	*mptr, *mpmax;
{
	int	doneparse = FALSE;	/* a couple of bools */
	int	delivered = FALSE;
	int	retval;
/*
 *	Now comes the bit where we try and be fearfully clever.
 *	We proceed down the array of md lines, whistling nochalantly,
 *	until we hit a situation where parsing must take place.
 *	At this point (with a sheepish grin) we then go and dig into
 *	the message and see what we can find, and then continue on as
 *	if nothing had happened.
 */

	for(; mptr < mpmax; mptr ++)
	{
		if( delivered && mptr->m_ar == M_CONDACC)
			continue;	/* this line can be ignored */
		if( mptr->m_parse && !doneparse)	/* arggh caught out! */
		{
			if(rp_isgood( dotheparse(mptr, mpmax)))
				doneparse = TRUE;
			else	return RP_NO;
		}
		if(! mptr->m_special )  /* this is a real header */
		{
			if( !mptr->m_hit)
				continue;	/* but no hit */
#ifdef	DEBUG
			ll_log (logptr, LLOGFTR, "hit with %s (%s)",
				mptr->m_pattern, mptr->m_header);
#endif /* DEBUG */
			/* else we drop through to beyond the switch */
		}	/* beware the dangling else !! */
		else	/* do the switch */
		{
/* Run through the special case headers, break if hit, continue if miss */
			switch( mptr->m_hdrtype)
			{
			case	M_DEFAULT:
				if( ! delivered ) /* this applys */
					break;
				continue;
			case	M_ADDRESS:
				if( strindex(mptr->m_pattern, lo_adr) >= 0)
					break;
				continue;
			case	M_TRUE: /* always true */
				break;
			case	M_SENDER:
				if( strindex(mptr->m_pattern, lo_sender) >= 0)
					break;
				continue;
			default: /* what else can we do ? */
				continue;
			}
		}
				/* OK, do the action */
		retval = lo_doaction(mptr);

		switch( mptr->m_ar)	/* now how did it go ? */
		{
			case	M_CONDACC:
			case	M_ACCEPT:
				if( rp_isgood(retval))  /* delivered */
					delivered = TRUE;
				else if(rp_gval(retval) != RP_NO)
					return RP_AGN;  /* went bad (temp) */
				break;
			case	M_REJECT:
				continue;
		}
	}
	return(delivered ? RP_MOK : RP_MECH);
}

parse_line(mptr, line)  /* dump the line into the Mdlvry struct */
Mdlvry  *mptr;
char	*line;
{
	char	*argv[15];	/* the argv array */
	char	opts[LINESIZE]; /* temp space for gathering opts */
	int	argc;
	int	type;
	int	i;
	Lookup  *lp;

#ifdef	DEBUG
	ll_log (logptr, LLOGBTR, "parse_line(%s)", line);
#endif /* DEBUG */

	argc = sstr2arg(line, 15, argv, " \t,");	/* split it */
	if( argc < 4 )  /* not good enough! */
		return NOTOK;
	if( (type = lookup(argv[0], spec_hdrs)) == -1 ) /* not a special ?*/
	{
		mptr->m_parse = TRUE;
		mptr->m_special = FALSE;
		mptr->m_header = strdup(argv[0]);
		mptr->m_pattern = strdup(argv[1]);
#ifdef	DEBUG
		ll_log (logptr, LLOGFTR, "header='%s' pat='%s'",
				mptr->m_header, mptr->m_pattern);
#endif /* DEBUG */
	}
	else	/* Oh, it is a special */
	{
		mptr->m_special = TRUE;
		mptr->m_parse = FALSE;
		mptr->m_hdrtype = type;
		mptr->m_header = NULL;
		mptr->m_pattern = strdup(argv[1]);
#ifdef	DEBUG
		ll_log (logptr, LLOGFTR, "special pat='%s'", mptr->m_pattern);
#endif /* DEBUG */
	}
	switch( argv[3][0] )	/* only a,A & ? - anything else == R */
	{
		case	'A':
		case	'a':
			mptr->m_ar = M_ACCEPT;
			break;
		case	'?':
			mptr->m_ar = M_CONDACC;
			break;
		default:
		case	'r':
		case	'R':
			mptr->m_ar = M_REJECT;
			break;
	}
#ifdef	DEBUG
	ll_log (logptr, LLOGFTR, "action=%o", mptr->m_ar);
#endif /* DEBUG */
	if( (type = lookup(argv[2], actions)) == -1)
		ll_log(logptr, LLOGFAT, "Unknown action to perform '%s'\n",
				argv[2]); /* a `never happens case' */
	else	mptr->m_action = type;
#ifdef	DEBUG
	ll_log (logptr, LLOGFTR, "action=%o", mptr->m_action);
#endif /* DEBUG */
	switch (mptr->m_action )
	{
		case	M_FILE:
			mptr->m_options = strdup(argv[4]);
			break;
		case	M_DESTROY:
			break;
		case	M_PIPE: /* gather together the argv's */
			opts[0] = '\0';
			for( i = 4; i < argc; i++)
			{
				strcat(opts, argv[i]);
				if( i != argc -1 )
					strcat(opts, " ");
			}
			mptr->m_dollar = 0;
			mptr->m_options = strdup(opts);
			for(lp = dolfields; lp->l_key != NULL; lp++)
				if( strindex(lp->l_key, opts) >= 0)
				{
					mptr->m_parse = TRUE;
					mptr->m_dollar |= lp->l_val;
				}
			break;
		default:
			break;
	}
	mptr->m_hit = 0;
#ifdef	DEBUG
	ll_log (logptr, LLOGFTR, "dol=%o parse=%o opts='%s'",
			mptr->m_dollar, mptr->m_parse, mptr->m_options);
#endif /* DEBUG */
	return OK;
}
lo_doaction(mp)
Mdlvry	*mp;
{
  char buf[2*LINESIZE];

# ifdef	DEBUG
	ll_log (logptr, LLOGBTR, "lo_doaction");
#endif /* DEBUG */

	switch( mp->m_action )
	{
		case	M_DESTROY:	/* /dev/null it */
			return (RP_MOK);

		/* pipe has gone away */
		case	M_PIPE:
			return lo_dopipe(mp->m_options);
		case	M_FILE:
			sprintf(buf,"%s/%s/%s",mldfldir,lo_pw->pw_name,mp->m_options);
			return lo_dofile(buf);
		default:
			ll_log( logptr, LLOGFAT, "unidentified action type %o",
					mp->m_action);
			return RP_NO;
	}
	/* NOTREACHED */
}
/**************** PARSING DELIVERY ROUTINES *************** */

/*
 * the routine that actually parses the message and sets up hit/miss
 * bits in the md struct.
 */

dotheparse(mpbase, mpmax)
Mdlvry  *mpbase, *mpmax;
{
	char	line[LINESIZE];
	char	name[LINESIZE], contents[LINESIZE];
	Mdlvry  *mp;
	int	len;
	int	gotrepl = 0;

# ifdef DEBUG
	ll_log (logptr, LLOGBTR, "dotheparse()");
#endif /* DEBUG */
	/* start reading header */
	qu_rtinit(0L);

	while((len = hdr_line(line, LINESIZE)) > 0)
	{
		line[len] = '\0';
		switch(hdr_parse(line, name, contents))
		{
		case	HDR_NAM:
			continue;	/* to avoid break */

		case	HDR_EOH:
			break;	/* break out of loop below */

		case	HDR_NEW:
		case	HDR_MOR:
			for(mp = mpbase; mp < mpmax; mp++)
			{
				if( !mp->m_hit && lexequ(mp->m_header, name) &&
					strindex(mp->m_pattern, contents) >= 0)
				{
					mp->m_hit = TRUE;
#ifdef	DEBUG
			ll_log (logptr, LLOGFTR, "hit in parse '%s' & '%s'",
					contents, mp->m_pattern);
#endif /* DEBUG					 */
				}
				if(( mp->m_dollar & 1))
					if(! gotrepl && lexequ(name, "from"))
						(void) strcpy(lo_replyto, contents);
					else if(lexequ(name, "reply-to"))
					{
						(void) strcpy(lo_replyto, contents);
						gotrepl = TRUE;
					}
			}
			continue;
		} /* end switch */
	    break;
	}

	if (len <= 0)
		return RP_NO;
	return  RP_OK;
}

/* basic processing of incoming header lines */
LOCFUN
hdr_line(buf, len)
char *buf;
int len;
{
    register int count;
    static char hdr_buf[LINESIZE];	/* buffer */
    static int buf_cnt = 0;	/* chars in buf */
    static char *buf_ptr;
    register char *hp;
    register char *bp;

    count = len;
    hp = buf_ptr;
    bp = buf;
    while (count > 0)
    {
	if (buf_cnt == 0)
	{
	    buf_cnt = sizeof(hdr_buf);
	    if (qu_rtxt(hdr_buf,&buf_cnt) != RP_OK)
		buf_cnt = 0;	/* break below */
	    hp = hdr_buf;
	}
	
	/* any more chars? */
	if (buf_cnt <= 0)
	    break;

	/* bugfix from howard */
	buf_cnt--;
	count--;

	if ((*bp++ = *hp++) == '\n')
	    break;
    }

    buf_ptr = hp;

#ifdef DEBUG
    ll_log(logptr, LLOGFTR, "hdr_line (%*s)", len-count, (len-count)?buf:"");
#endif

    return(len - count);
}

LOCFUN
hdr_parse (src, name, contents)	  /* parse one header line		*/
    register char *src;		  /* a line of header text		*/
    char *name,			  /* where to put field's name		*/
	 *contents;		  /* where to put field's contents	*/
{
    extern char *compress ();
    char linetype;
    register char *dest;

#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "hdr_parse(%s)", src);
#endif

    if (isspace (*src))
    {				  /* continuation text			*/
#ifdef DEBUG
	ll_log (logptr, LLOGFTR, "cmpnt more");
#endif
	if (*src == '\n')
	    return (HDR_EOH);
	linetype = HDR_MOR;
    }
    else
    {				  /* copy the name			*/
	linetype = HDR_NEW;
	for (dest = name; *dest = *src++; dest++)
	{
	    if (*dest == ':')
		break;		  /* end of the name			*/
	    if (*dest == '\n')
	    {			  /* oops, end of the line		*/
		*dest = '\0';
		return (HDR_NAM);
	    }
	}
	*dest = '\0';
	compress (name, name);	  /* strip extra & trailing spaces	*/
#ifdef DEBUG
	ll_log (logptr, LLOGFTR, "cmpnt name '%s'", name);
#endif
    }

    for (dest = contents; isspace (*src); )
	if (*src++ == '\n')	  /* skip leading white space		*/
	{			  /* unfulfilled promise, no contents	*/
	    *dest = '\0';
	    return ((linetype == HDR_MOR) ? HDR_EOH : linetype);
	}			   /* hack to fix up illegal spaces	 */

    while ((*dest = *src) != '\n' && *src != 0)
	     src++, dest++;	  /* copy contents and then, backup	*/
    while (isspace (*--dest));	  /*   to eliminate trailing spaces	*/
    *++dest = '\0';

    return (linetype);
}
/***************  (misc) MISCELLANEOUS ROUTINES  ******************** */

/*
 *	Dig into table for a match - return associated value else -1
 *	table ends when key == 0;
 *		(JPO)
 */

LOCFUN
lookup( str, table )
char	*str;
Lookup  table[];
{
	register Lookup *p;

	for(p = table; p -> l_key ; p++)
	{
/*
 * match with prefix, this allows "addr" to match "address"
 */
		if( prefix(p -> l_key, str) )
			return (p -> l_val);
	}
	return (-1);
}

/*
 * setup the environment for the process to run in. This includes :-
 *	Environment variables
 *	file descriptors - close all but fd0
 *	process groups
 *	controlling ttys.
 *	umask
 */

LOCFUN setupenv()
{
	int	fd;
	static char	homestr[64];		/* environment $HOME param */
	static char	shellstr[64];		/* environment $SHELL param */
	static char	userstr[64];		/* environment $USER param */

#ifdef	DEBUG
	ll_log (logptr, LLOGBTR, "setupenv()");
#endif /* DEBUG */
	for (fd = 1; fd < numfds; fd++)
		(void) close (fd);
	fd = open ("/dev/null", 1);	/* stdout */
	dup (fd);			/* stderr */

	umask(077);	/* Restrictive umask */

#ifdef TIOCNOTTY
	if (setjmp (timerest) == 0)
	{
		flgtrest = TRUE;
		s_alarm(15);	/* should be enough */
		if( (fd = open("/dev/tty", 2)) != NOTOK) {
			(void) ioctl(fd, TIOCNOTTY, (char *)0);
			(void) close(fd);
		}
		s_alarm(0);
	}
	else
	{
		flgtrest = FALSE;
		ll_log (logptr, LLOGGEN, "TIOCNOTTY not available");
	}
#endif /* TIOCNOTTY */
#ifdef V4_2BSD
	setpgrp (0, getpid());
#endif
	sprintf (homestr, "HOME=%s/%s/home",mldfldir,lo_pw->pw_name);
	sprintf (shellstr, "SHELL=%s",
			isstr(lo_pw->pw_shell) ? lo_pw->pw_shell : "/bin/sh");
	sprintf (userstr, "USER=%s", lo_pw->pw_name);

	envp[0] = homestr;
	envp[1] = shellstr;
	envp[2] = userstr;
}

sigtype
onpipe()
{
	signal(SIGPIPE, onpipe);
	sigpipe = TRUE;
}









