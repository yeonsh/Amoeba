/*	@(#)rcvtrip.c	1.1	91/11/21 11:45:46 */
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
 *     This program module was developed as part of the University
 *     of Delaware's Multi-Channel Memo Distribution Facility (MMDF).
 *
 *     This module and its listings may be used freely by United States
 *     federal, state, and local government agencies and by not-for-
 *     profit institutions, after sending written notification to
 *     Professor David J. Farber at the above address.
 *
 *     For-profit institutions may use this module, by arrangement.
 *
 *     Notification must include the name of the acquiring organization,
 *     name and contact information for the person responsible for
 *     maintaining the operating system, name and contact information
 *     for the person responsible for maintaining this program, if other
 *     than the operating system maintainer, and license information if
 *     the program is to be run on a Western Electric Unix(TM) operating
 *     system.
 *
 *     Documents describing systems using this module must cite its
 *     source.
 *
 *     Development of this module was supported in part by the
 *     University of Delaware and in part by contracts from the United
 *     States Department of the Army Readiness and Materiel Command and
 *     the General Systems Division of International Business Machines.
 *
 *     Portions of the MMDF system were built on software originally
 *     developed by The Rand Corporation, under the sponsorship of the
 *     Information Processing Techniques Office of the Defense Advanced
 *     Research Projects Agency.
 *
 *     The above statements must be retained with all copies of this
 *     program and may not be removed without the consent of the
 *     University of Delaware.
 *
 *
 *     version  -1    David H. Crocker    March  1979
 *     version   0    David H. Crocker    April  1980
 *     version   1    David H. Crocker    May    1981
 *
 */

/*
 *                              RCVTRIP
 *
 *      Rcvtrip responds to incoming mail, recording who sent it, what the
 *              subject was, when it came in, and if it was replied to.
 *
 *      The reply consists of some standard text, followed by any message
 *              which the user wishes to specify.
 *
 *      Oct, 80     Bruce Whittaker         Initial coding and
 *       - May, 81   (Whittake@Udel-EE)       maintenance
 *
 *	Sep  84 Julian Onions  jpo@nott.ac.uk
 *		Total rewrite from the ground upwards, original ideas
 *		kept, but now makes use of the mmdf library routines,
 *		mmdf address parser and alias look up. This should
 *		mean future changes will be easier to deal with. (?!)
 *
 *	Feb  85 Julian Onions  jpo@cs.nott.ac.uk
 *		Rehacked to take into account weirdo addresses
 *		+ normali[zs]ed domain.s
 *
 *      Oct  87 Ian G. Batten  BattenIG@cs.bham.ac.uk
 *              A backwards step!  Code included to allow for a file
 *		called "~/.alter_egos" which contains user@site lines for 
 *		all addresses to be considered to be "you".  This is needed
 *		if you have multiple hosts forwarding their mail to you.
 *
 */

# include	"util.h"
# include	"mmdf.h"
# include	"ap.h"
# include	"ch.h"
# include	"hdr.h"
# include	<pwd.h>
# include	<stdio.h>

# define	ANIL		((AP_ptr)0)
# define	NILC		((char *)0)

# define	NONFATAL	0	/* Nonfatal reports		*/
# define	FATAL		1	/* Fatal reports (exit)		*/
# define	TRACE		2	/* General info for debug	*/

# define	SENT		0	/* Sent out a reply		*/
# define	UNSENT		1	/* No reply sent - just note	*/

# define	REPLIED		'+'	/* A reply was sent		*/
# define	NOT_REPLIED	'-'	/* A reply wasn't sent		*/

extern Llog    *logptr;
extern int     ap_outtype;

extern char    *index ();
extern char    *strcpy ();
extern char    *strncpy ();
extern char    *fgets ();
extern char    *strcat ();
extern char    *compress ();
extern char    *ap_p2s ();
extern char    *ap_s2p ();
extern char    *getmailid();
extern AP_ptr  ap_t2s ();

int	debug	=	FALSE;		/* output trace messages?	   */
int	found_1 =	FALSE;		/* true if message was sent to	   */
					/* directly to recipient	   */
char	subj_field[ADDRSIZE];		/* contents of subject field	   */
char	username[20];			/* the users username		   */
char	logloc []	= "logfile";	/* where to log error conditions   */
char	msg_note []	= "tripnote";	/* where to get reply message from */
char	wholoc []	= "triplog";	/* where to note incoming messages */
char	alter_egos []	= ".alter_egos"; /* where to get alternative names  */
char	sigloc []	= ".fullname";	/* where to find user's name	   */
char	tripsig []	= "MESSAGE AGENT";
					/* signature of the returned message*/

AP_ptr  resent_adr=	ANIL;		/* address tree for resent-from addr*/
AP_ptr  rrepl_adr=	ANIL;		/* address " " resent-reply-to addr*/
AP_ptr  rsndr_adr=	ANIL;		/* address " for resent-sender addr*/
AP_ptr  from_adr=	ANIL;		/* address tree for from address   */
AP_ptr  sndr_adr=	ANIL;		/*     - " -	    sender	   */
AP_ptr  repl_adr=	ANIL;		/*     - " -	    reply-to	   */

char	*find_sig();

struct passwd	*getpwuid();

/*
 *	This message is output before the users file message
 */
char	*text_message[] = {
	"\tThis is an automatic reply.  Feel free to send additional\n",
	"mail, as only this one notice will be generated.  The following\n",
	"is a prerecorded message, sent for ", 0
};

/*
 *	This message is output if the users file is not readable or doesn't
 *	exist. (This is where you can have some fun!)
 */
char	*error_txt[] = {
	"\tIt appears that the person in question hasn't left a\n",
	"message file, so the purpose of this message is unknown.\n",
	"However it is assumed that as I, a daemon process,\n",
	"have been called into existance that the person in question\n",
	"is probably away for some time and won't be replying to your\n",
	"message straight away.\n\nCheers,\n\tThe Mail System\n", 0
};

typedef struct name_record
{
  char *name;
  struct name_record *next_name_record;
} NAME_RECORD;

NAME_RECORD *my_names;

main (argc, argv)
int     argc;
char  **argv;
{
	init (argc, argv);
	parsemsg ();
	doreplies ();
	exit( RP_MECH );
}

/*
 *	Initialisation - get user, setup flags etc.
 */

init (argc, argv)
int     argc;
char  **argv;
{
    FILE *names_file;
    NAME_RECORD *temp;
    char buffer[128];
    register struct  passwd  *pwdptr;
    int	realid, effecid;
    char	*cp;

	ap_outtype = AP_822;		/* standardise on 822 */
	argv ++;
	if (argc > 1 && strcmp(*argv, "-d") == 0)
	{
		argv++;
		argc--;
		debug++;
	}

	if( argc > 1 )  /* we have a command line sender address */
		parse_addr(*argv, &sndr_adr);

	mmdf_init ("TRIP");

chdir(getenv("HOME"));
report(TRACE,"HOME = %s USER = %s",getenv("HOME"),getenv("USER"));
/*  getwho (&realid, &effecid);
  if((pwdptr = getpwuid (realid)) == (struct passwd *)0
     || (cp = getmailid (pwdptr -> pw_name)) == NULL)
    report( FATAL, "Can't Identify user %d", realid);
  else
    strcpy (username, cp);*/

    strcpy(username,getenv("USER"));
 
  if ((names_file = fopen (alter_egos, "r")) != (FILE *) 0)
    {
      while ((fgets (buffer, sizeof buffer, names_file)) != NULL)
	{
	  temp = (NAME_RECORD *) malloc (sizeof (NAME_RECORD));
	  
	  if (my_names == (NAME_RECORD *) 0)
	    {
	      my_names = temp;
	      temp->next_name_record = (NAME_RECORD *) 0;
	    }
	  else
	    {
	      temp->next_name_record = my_names;
	      my_names = temp;
	    }
	
	  temp->name = (char *) malloc (strlen (buffer));
	  /* buffer + 1 for null, less 1 for CR */
	  buffer[strlen(buffer) -1] = '\0';	     /* Zap the CR */
	  strcpy (temp->name, buffer);
	}
      fclose (names_file);
    }
}

/*
 *	parse message - just a front end to the real parser
 */

parsemsg()
{
	if( rp_isbad(hdr_fil(stdin)) )
		report( FATAL, "parse of message failed");
}

/*************** Routines to despatch messages ***************/

/*
 *	Send replies to reply-to address if specified else
 *		from and sender address.
 */

doreplies()
{
	if( found_1 != TRUE )
		report( FATAL, "Not sent directly to %s", username);

	if (from_adr == ANIL && sndr_adr == ANIL && resent_adr == ANIL
	                     && repl_adr == ANIL && rrepl_adr == ANIL
			     && rsndr_adr == ANIL)
		report( FATAL, "No parsable from or sender lines");

/*
 *	If there were any reply-to lines, use those in preference.
 */
	if (rrepl_adr != ANIL)
		replyto( rrepl_adr);
	else if (resent_adr != ANIL) {
		replyto( resent_adr );
   		if (rsndr_adr != ANIL)
   		   replyto( rsndr_adr );
             }
	else if( repl_adr != ANIL )
		replyto( repl_adr );
	else	/*	send to both from && sender if they exist */
	{
		if (from_adr != ANIL)
			replyto( from_adr );
		if (sndr_adr != ANIL)
			replyto( sndr_adr );
	}
}

/*
 *	Actually do the work of sending the reply.
 */

replyto( aptr )
AP_ptr aptr;
{
	register char	 *cp;
	register char	 **cpp;
	AP_ptr  user, local, domain;
	AP_ptr	norm;
	char	*addr;
	char	*ref;
	char	*person;
	char	buffer[ADDRSIZE];
	FILE	*fp;

#ifdef	DEBUG
	report( TRACE, "replyto()");
#endif	DEBUG

	norm = ap_normalize( NILC, NILC, aptr, (Chan *)0);
	ap_t2parts(norm, (AP_ptr *)0, &user, &local, &domain, (AP_ptr *)0);
	ref = ap_p2s(ANIL, ANIL, local, domain, ANIL);
#ifdef	DEBUG
	report( TRACE, "normed addr = '%s'", ref);
#endif	DEBUG

	if( user != ANIL)
		person = user-> ap_obvalue;
	else	person = NILC;

	if( checkuser( ref ) )
	{
#ifdef	DEBUG
		report( TRACE, "Seen %s before", ref);
#endif	DEBUG
		noteuser( ref, UNSENT );
		free( ref );
		return;
	}
	if( ap_t2s(norm, &addr) == (AP_ptr)NOTOK)
	{
		report( NONFATAL, "Parse error for %s", addr);
		noteuser( ref, UNSENT);
		free( ref );
		return;
	}
	buffer[0] = '\0';
	if( ! prefix("re:", subj_field) )
		strcpy( buffer, "Re: ");

	strcat( buffer, subj_field);

#ifdef	DEBUG
	report(TRACE, "ml_1adr(NO, NO, '%s' %s, %s)",
			tripsig, buffer, addr);
#endif	DEBUG

	ml_1adr( NO, NO, tripsig, buffer, addr);

	if( person != NILC)
	{
		compress(person, person);
		sprintf( buffer, "\nDear %s,\n", person);
#ifdef	DEBUG
		report( TRACE, "1st line %s", buffer);
#endif	DEBUG
		ml_txt( buffer );
	}

#ifdef	DEBUG
	report( TRACE, "start builtin message");
#endif	DEBUG
	for( cpp = text_message; *cpp != NILC ; cpp++)
		ml_txt( *cpp );
	if(( cp = find_sig()) != NILC)
		ml_txt( cp );
	else	ml_txt( username );
	ml_txt("\n\n\n");

	if( (fp = fopen( msg_note, "r")) != (FILE *)0)
	{
#ifdef	DEBUG
		report( TRACE, "processing file %s", msg_note);
#endif	DEBUG
		ml_file(fp);
		fclose(fp);
	}
	else
	{
#ifdef	DEBUG
		report( TRACE, "Sending built in error message");
#endif	DEBUG
		for( cpp = error_txt; *cpp != NILC; cpp++)
			ml_txt(*cpp);
	}
#ifdef	DEBUG
	report( TRACE, "ml_end(OK)");
#endif	DEBUG
	if( ml_end(OK) == OK)
		noteuser(ref, SENT);
	else	noteuser(ref, UNSENT);
	free( ref );
	free( addr );
}

/*************** Routines to pick apart the message ***************/

/*
 *	The actual picking out of headers
 */

hdr_fil (msg)
FILE * msg;			/* The message file			 */
{
	char    line[LINESIZE];	/* temp buffer			 */
	char    name[LINESIZE];	/* Name of header field		 */
	char    contents[LINESIZE];/* Contents of header field	 */
	int     len;

#ifdef	DEBUG
	report (TRACE, "hdr_fil()");
#endif	DEBUG
	if (msg == (FILE *) NULL)
	{
		report( FATAL, "NULL file pointer");
		return (RP_NO); /* not much point doing anything else */
	}
/* process the file    */

	FOREVER
	{
		if ((len = gcread (msg, line, LINESIZE, "\n\377")) < 1)
		{
			report( NONFATAL, "read error on message");
			return(RP_NO);  /* skip and go home */
		}
		line[len] = '\0';

		switch (hdr_parse (line, name, contents))
		{
		case HDR_NAM: /* No name so contine through header */
			continue;

		case HDR_EOH: /* End of header - lets go home */
			return(RP_OK);

		case HDR_NEW:
		case HDR_MOR: /* We have a valid field	    */
			if (lexequ (name, "to") ||
				lexequ(name, "cc") ||
				lexequ(name, "resent-to") ||
				lexequ(name, "resent-cc") )
			{
				find_user(contents);
				break;
			}
			else    if (lexequ (name, "resent-reply-to"))
			{
				parse_addr(contents, &rrepl_adr);
				break;
			}
			else    if (lexequ (name, "resent-sender"))
			{
				parse_addr(contents, &rsndr_adr);
				break;
			}
			else    if (lexequ (name, "resent-from"))
			{
				parse_addr(contents, &resent_adr);
				break;
			}
			else	if (lexequ (name, "from"))
			{
				parse_addr(contents, &from_adr);
				break;
			}
			else	if (lexequ (name, "reply-to"))
			{
				parse_addr(contents, &repl_adr);
				break;
			}
			else	if (lexequ (name, "sender"))
			{
				parse_addr(contents, &sndr_adr);
				break;
			}
			else	if (lexequ (name, "subject"))
			{
				strncpy(subj_field, contents, ADDRSIZE-1);
				break;
			}
			else
				continue;
		}
	}
}

/*
 *	The real parser
 */

LOCFUN
hdr_parse (src, name, contents)	/* parse one header line		 */
register char  *src;		/* a line of header text		 */
char   *name,			/* where to put field's name		 */
       *contents;		/* where to put field's contents	 */
{
	extern char    *compress ();
	char		linetype;
	register char  *dest;

#ifdef	DEBUG
	report (TRACE, "hdr_parse('%s')", src);
#endif	DEBUG

	if (isspace (*src))
	{			/* continuation text			 */
#ifdef	DEBUG
		report( TRACE, "hrd_parse -> cmpnt more");
#endif	DEBUG
		if (*src == '\n')
			return (HDR_EOH);
		linetype = HDR_MOR;
	}
	else
	{			/* copy the name			*/
		linetype = HDR_NEW;
		for (dest = name; *dest = *src++; dest++)
		{
			if (*dest == ':')
				break;		/* end of the name	*/
			if (*dest == '\n')
			{	/* oops, end of the line		*/
				*dest = '\0';
				return (HDR_NAM);
			}
		}
		*dest = '\0';
		compress (name, name);/* strip extra & trailing spaces	 */
#ifdef	DEBUG
		report (TRACE, "hdr_parse -> cmpnt name '%s'", name);
#endif	DEBUG
	}

	for (dest = contents; isspace (*src);)
		if (*src++ == '\n')/* skip leading white space		 */
		{		/* unfulfilled promise, no contents	 */
			*dest = '\0';
			return ((linetype == HDR_MOR) ? HDR_EOH : linetype);
		}		/* hack to fix up illegal spaces	 */

	while ((*dest = *src) != '\n' && *src != 0)
		src++, dest++;	/* copy contents and then, backup	 */
	while (isspace (*--dest));/*   to eliminate trailing spaces	 */
	*++dest = '\0';

	return (linetype);
}

/*************** Parsing of addresses got from message ***************/

/*
 *	See if user is in this field. parse address first and
 *		compare mbox's.
 */

find_user(str)
char *str;
{
	AP_ptr tree, local, domain;
	char *p;
	NAME_RECORD *temp;

#ifdef	DEBUG
	report(TRACE, "find_user('%s')", str);
#endif	DEBUG
	if( found_1 == TRUE)
	{
#ifdef	DEBUG
		report(TRACE, "find_user() name already found\n");
#endif	DEBUG
		return;
	}

	while( (str = ap_s2p(str, &tree, (AP_ptr *)0, (AP_ptr *)0, &local,
		&domain, (AP_ptr *)0) ) != (char *)DONE
		&& str != (char *) NOTOK )
	{
	  if (my_names == (NAME_RECORD *) 0) /* No alter-ego file found */
	    {
	      p = ap_p2s( ANIL, ANIL, local, ANIL, ANIL);
#ifdef	DEBUG
	      report( TRACE, "find_user() -> found mbox %s", p);
#endif	DEBUG
	      if( lexequ(p, username) )
		found_1 = TRUE;
	      else	found_1 = alias(p, username) ? TRUE: FALSE;
	    }
	  else
	    {
	      p = ap_p2s (ANIL, ANIL, local, domain, ANIL);
	      for (temp = my_names; temp != (NAME_RECORD *) 0; 
		   temp = temp->next_name_record)
		{
#ifdef DEBUG
		  report (TRACE, "find_user (): Testing %s vs. %s\n", p, temp->name);
#endif DEBUG

		  if (lexequ (p, temp->name))
		    found_1 = TRUE;
		}
	    }
		  
		free(p);
		ap_sqdelete(tree, ANIL);
		ap_free(tree);
		if( found_1 == TRUE )
			return;
	}
}
/*
 *	Attempt to parse a field into an address tree for later use.
 */

parse_addr( str, aptr)
char	*str;
AP_ptr  *aptr;
{

#ifdef	DEBUG
	report( TRACE, "parse_addr('%s')", str);
#endif	DEBUG

	if( *aptr != ANIL)
	{
#ifdef	DEBUG
		report( TRACE, "field '%s' rejected, already seen one");
#endif	DEBUG
		return;
	}

	if( (*aptr = ap_s2tree(str) ) == (AP_ptr)NOTOK)
		report( NONFATAL, "Can't parse address '%s'", str);
}

/*************** User Data Base Routines ***************/

/*
 *	Note the fact we did/didnot reply to a given person
 *	and include the subject line for good measure.
 */

noteuser(addr, mode)
char	*addr;
int	mode;
{
	FILE	*fp;
	time_t  now;
	char	*ctime();

#ifdef	DEBUG
	report( TRACE, "noteuser(%s,%d)", addr, mode);
#endif	DEBUG

	time(&now);

	if( (fp = fopen(wholoc, "a")) != NULL)
	{
		chmod(wholoc, 0600);
		fprintf( fp, "%c %-30s %19.19s >> %.20s\n", mode == SENT? REPLIED: NOT_REPLIED,
			addr, ctime(&now), subj_field);
		fclose(fp);
	}
}
/*
 *	Have we replied to this person before?
 */
checkuser( addr )
char *addr;
{
	FILE	*fp;
	char	buffer[LINESIZE];
	char	compaddr[LINESIZE];

	if((fp = fopen(wholoc, "r")) == NULL)
		return FALSE;
	while( fgets( buffer, sizeof buffer, fp) != NULL)
	{
		if(buffer[0] == NOT_REPLIED )
			continue;
		getaddress(buffer, compaddr);
#ifdef	DEBUG
		report( TRACE, "checkuser, = '%s' & '%s'?", compaddr, addr);
#endif	DEBUG
		if( lexequ(compaddr, addr) )
		{
			fclose(fp);
			return TRUE;
		}
	}
	return FALSE;
}

/*************** Alias Lookup ***************/

/*
 *	Saunter thru the alias table looking for aliases
 *	This only looks for single pass aliases, for two reasons
 *	1)	Its more work than I can be bothered to do now.
 *	2)	If its more than a simple alias substitution, then
 *		its probably part of an address list or something
 *		similarly complicated, in which case its probably
 *		better to keep quite about it.
 */

alias(str, uname)
char	*str;
char	*uname;
{
	char	aliasval[ADDRSIZE];
	register char	*p;

#ifdef	DEBUG
	report( TRACE, "alias(%s, %s)", str, uname);
#endif	DEBUG
	if (aliasfetch(TRUE, str, aliasval, 0) != OK)
		return FALSE;
	p = aliasval;
	if( *p == '~')  p++;		/* strip off leading ~ */

	return lexequ(p, uname);
}

/*************** Some Utility Routines ***************/


/*
 *	Dig out a signature for the user
 */

char *find_sig()
{
	FILE *fp;
	static char buf[ADDRSIZE];
	static int  been_here = FALSE;
	char *p;

#ifdef	DEBUG
	report( TRACE, "find_sig()");
#endif	DEBUG

	if( been_here == TRUE )  /* cuts off at least 1/4 micro-second */
		return buf;

	if((fp = fopen(sigloc, "r")) == NULL)
		return NILC;
	if( fgets(buf, sizeof(buf), fp) == NILC)
	{
		fclose(fp);
		return NILC;
	}
	if( (p = index(buf, '\n')) != NILC)
		*p = '\0';
	fclose(fp);
	return buf;
}

getaddress(source, dest)
char *source, *dest;
{
	register char *p;
	int	depth = 0;

	p = source + 2;		/* skip over reply indicator */
	while(*p)
	{
		switch( *p )
		{
			case '"':	/* in quoted part ? */
				depth = !depth;
				break;
			case ' ':
			case '\t':
				if( depth == 0)
				{
					*dest = '\0';
					return;
				}
				break;
			case '\\':	/* gobble up next char */
				*dest++ = *p++;
				break;
		}
		*dest++ = *p++;
	}
	*dest = '\0';
}

/*VARARGS2*/
report( mode, fmt, a1, a2, a3, a4)
int mode;
char *fmt, *a1, *a2, *a3, *a4;
{
	static FILE *log;

	if( debug == FALSE && mode == TRACE)
		return;
	if( log == NULL && access(logloc, 0) != -1)
	{
		log = fopen(logloc, "a");
		chmod(logloc, 0600);		/* keep things private */
	}
	if( debug == TRUE )
	{
		fprintf( stderr,"%s\t", mode == TRACE?"TRACE":
			(mode == FATAL?"FATAL":"!FATAL"));
		fprintf( stderr, fmt, a1, a2, a3, a4);
		putc('\n', stderr);
		fflush( stderr);
	}
	if( log != NULL)
	{
		fprintf( log, "%s\t", mode == TRACE?"TRACE":
			(mode == FATAL?"FATAL":"!FATAL"));
		fprintf( log, fmt, a1, a2, a3, a4);
		putc('\n', log);
		fflush( log );
	}
	else if(mode != TRACE)
		ll_log( logptr, LLOGTMP, fmt, a1, a2, a3, a4);
	if( mode == FATAL)
		exit(RP_MECH);		/* die a horrible death */
}
