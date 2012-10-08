/*	@(#)s_get.c	1.1	92/05/14 10:44:16 */
/*
**		S _ G E T
**
**
**	R E V I S I O N  H I S T O R Y
**
**	03/31/83  GWH	Split the SEND program into component parts
**			This module contains getuinfo, gethead, prefix,
**			and getaddr.
**
**	04/15/83  DPK	Changed address format from "user at host" to
**			"user@host". Also added call to getmailid() in get
**			getuinfo();
**
**	05/20/83  GWH	Modified getuinfo() to allow a number of user-
**			selectable options. These include the use of a
**			".sendrc" file on the user's home directory. With
**			the use of the "rc" file the user can set such things
**			default editor, local-alias-file, signature block,
**			and a directory for draft files. Also included is a
**			provision for keeping file copies of messages that
**			are successfully copied (incomplete at this time).
**
**	06/22/83  GWH	Added the required code to addrcat to enable local
**			alias file feature.
**
**	10/21/83  GWH	Fixed a bug in the alias file code (in addrcat).
**			Prevents segmentation faults when loops are found
**			in the alias file definitions.
**
**	11/16/83  DPK	Merged in DPK signature checking and Geo loop catcher
**
**	11/21/83  GWH	Added 'directedit' option.
**
**	03/18/85  DPK	Fixed usage of parsadr function so that after parsing
**		  VAK	we now use the components instead of original string.
**
*/


#include <stdio.h>
#include "./s.h"
#include "./s_externs.h"

extern char *getmailid();
extern char *malloc();
extern char *strdup();
extern struct passwd *getpwuid();
extern char *mktemp();
extern char *index();
extern char *getenv();

extern char *dflveditor;
extern char *dflchecker;

#define END	(char *)0

struct RCOPTION rcoptions[] = {
	"copyfile", COPYFILE,
	"draftdir", DRAFTDIR,
	"signature", SIGNATURE,
	"nosignature", NOSIGNATURE,
	"replyto", REPLYTO,
	"aliases", ALIASES,
	"subargs", SUBARGS,
	"editor", EDITOR,
	"veditor", VEDITOR,
	"file", CFILE,
	"fileonquery", QFILE,
	"nofile", NOFILE,
	"checker", CHECKER,
	"header", ADDHEADER,
	"paging", PAGING,
	END
};

int picko();
int ded = 0;
struct passwd *pw;
char *midp;
char *d_subargs = "vm";
char tmp_tmplt[] = "tmpdrft.XXXXXX";
char drft_tmplt[] = "drft.XXXXXX";
char linebuf[128];
char tmpline[128];

getuinfo() {
	FILE *fp;
	int realid, effecid;
	char *av[NARGS];
	char rcfilename[128];
	register char *p;
	char *rcname = "/.sendrc";

	int i;

	/* build a draft file name */

	mktemp(drft_tmplt);
	mktemp(tmp_tmplt);

	/* Who are we this time */

	getwho(&realid, &effecid);

	/* If nothing found return false. */

	if(( pw = getpwuid(effecid)) == NULL  ||
		(midp = getmailid(pw->pw_name)) == NULL )
			return(FALSE);

	/* Build a name for this user */
#ifdef PWNAME
	if (p = index( pw->pw_gecos, ',' )) *p = '\0';
	if (p = index( pw->pw_gecos, '<' )) *p = '\0';
	if (p = index( pw->pw_gecos, '&' )) {
		/* Deal with Berkeley folly */
		*p = 0;
		sprintf( from, "%s%c%s%s <%s@%s.%s>", pw->pw_gecos,
			 lowtoup( pw->pw_name[0] ),
#ifdef JNTMAIL
			 pw->pw_name+1, p+1, midp, ap_dmflip(locdomain), locname );
	} else
		sprintf( from, "%s <%s@%s.%s>", pw->pw_gecos, midp, ap_dmflip(locdomain), locname);
#else JNTMAIL
			pw->pw_name+1, p+1, midp, locname, locdomain);
	} else
		sprintf( from, "%s <%s@%s.%s>", pw->pw_gecos, midp, locname, locdomain );
#endif JNTMAIL
#else
	sprintf( from, "%c%s@%s.%s",	/* default from text */
#ifdef JNTMAIL
		lowtoup( midp[0] ), &(midp[1]), ap_dmflip(locdomain), locname);
#else JNTMAIL
		lowtoup( midp[0] ), &( midp[1] ), locname, locdomain );
#endif JNTMAIL
#endif
	/* Set default values for those options that need defaults */

	strcpy(editor, ((p = getenv("VISUAL")) && *p) ? p : 
 		        (((p = getenv("EDITOR")) && *p) ? p : dflveditor));
	strcpy(checker, dflchecker);
	sprintf(linebuf, "%s/.sent", pw->pw_dir);
	strcpy(copyfile, linebuf);
	sprintf(drffile, "%s/%s", pw->pw_dir, drft_tmplt);
 	sprintf(tmpdrffile, "%s/%s", pw->pw_dir, tmp_tmplt);
	strcpy( subargs, d_subargs );
	wflag = 0;
	cflag = 0;
	rflag = 0;
	qflag = 0;
	pflag = 0;
       	strcpy (signature, from);
	/* Get info from the ".sendrc" file */

	sprintf( rcfilename, "%s/%s", pw->pw_dir, rcname );
	if(( fp = fopen(rcfilename, "r")) != NULL ) {
		char *cp;

		/* Process info a line at a time */
		while( fgets(linebuf, sizeof(linebuf), fp ) != NULL ) {
			if (cp = index(linebuf, '\n'))
				*cp = 0;
			if (sstr2arg(linebuf, NARGS, av, " \t") < 0)
				continue;
			if( (i=picko(av[0])) < 0)
				continue; /* bad option */
			select_o( i, av );
		}	/* end of while on lines */
	}	/* end of if statement on file open */

	return(TRUE);
}	/* end of getuinfo routine */

/* select an option */

picko (pp)
char **pp;
{
	struct RCOPTION *rcopt;

	rcopt = rcoptions;
	while(rcopt->name != END ) {
		if( lexequ( rcopt->name, pp ) )
			return(rcopt->idnum);
		rcopt++;
	}

	return(-1); /* no match */
}

select_o (key, pp)
int key;
char **pp;
{
	register char *p;
	char *akey, *list;
	char tmpbuf[512];

	switch( key ) {
	case COPYFILE:
		/* Pick a file to store file copies of sent messages */
		if( *(++pp) == NULL )
		{
			fprintf(stderr,"You need to specify a copy file in your .sendrc.\n");
			fprintf(stderr,"Defaulting to '.sent'\n");
		}
		else
		{
			strcpy( copyfile, *pp);
			if( copyfile[0] != '/' ) 
			{
				strcpy(tmpline, copyfile);
				sprintf(copyfile, "%s/%s",pw->pw_dir,tmpline);
			}
		}
		break;

	case DRAFTDIR:
		/* pickup the draft directory */
		if( *(++pp) != NULL )
		{
			sprintf( drffile, "%s/%s", *pp, drft_tmplt );
			sprintf( tmpdrffile, "%s/%s", *pp, tmp_tmplt );
		}
		else
		{
			sprintf( drffile, "%s/%s", pw->pw_dir, drft_tmplt);
			sprintf( tmpdrffile, "%s/%s", pw->pw_dir, tmp_tmplt);
		}
		break;

	case SIGNATURE:
		/* If he wants a signature block use it */
		if( *(++pp) == 0 ){
#ifdef PWNAME
			sprintf( signature, "%c%s@%s.%s", /* use his login name alone */
#ifdef JNTMAIL
			   lowtoup(midp[0]), &(midp[1]), ap_dmflip(locdomain), locname );
#else JNTMAIL
			   lowtoup(midp[0]), &(midp[1]), locname, locdomain );
#endif JNTMAIL
#else
			strcpy( signature, from );	/* NO-OP ? */
#endif
		} else {
			linebuf[0] = '\0';
			while( *pp != 0){
				strcat(linebuf, *pp);
				strcat(linebuf, " ");
				*pp++;
			}
			if (checksignature (linebuf) == 0) {
#ifdef JNTMAIL
				sprintf(tmpline, "%s.%s", ap_dmflip(locdomain), locname);
#else JNTMAIL
				sprintf(tmpline, "%s.%s", locname, locdomain);
#endif JNTMAIL
				for( p=tmpline ; *p ; p++)
					*p = uptolow(*p);
				sprintf( signature, "%s <%s@%s>",
				   linebuf, midp, tmpline);
			} else {
				fprintf(stderr,"Illegal signature, using default\n");
			}
		}
		break;

	case NOSIGNATURE:
#ifdef PWNAME
		sprintf( signature, "%c%s@%s.%s", /* use his login name alone */
#ifdef JNTMAIL
			   lowtoup(midp[0]), &(midp[1]), ap_dmflip(locdomain), locname );
#else JNTMAIL
			   lowtoup(midp[0]), &(midp[1]), locname, locdomain);
#endif JNTMAIL
#else
		strcpy( signature, from );
#endif
		break;

	case REPLYTO:
		rflag = 1;
		break;

	case ALIASES:
		if( *(++pp) == NULL )
			fprintf(stderr,"You need to specify an alias file name!\n");
		else
		{
			strcpy( aliasfilename, *pp );
			if( aliasfilename[0] != '/' ) 
			{
				strcpy(tmpline, aliasfilename);
				sprintf(aliasfilename, "%s/%s",pw->pw_dir,tmpline);
			}
		}
		aliasinit( aliasfilename );
		break;

	case SUBARGS:
		while( *(++pp) != NULL )
			strcat(subargs, *pp);
		wflag = 1;
		break;
	case EDITOR:
		if(ded)
			break;
		else if( *(++pp) == NULL )
		{
			fprintf(stderr,"You need to specify an editor after 'editor' in your .sendrc.\n");
			fprintf(stderr,"Defaulting to %s.\n",editor);
		}
		else
			strcpy( editor, *pp );
		break;
	case VEDITOR:
		/* Default editor for use with the "e" command */
		if( *(++pp) == NULL )
		{
			fprintf(stderr,"You need to specify an editor with the veditor in your .sendrc.\n");
			fprintf(stderr,"Defaulting to %s.\n",editor);
		}
		else
			strcpy( editor, *pp );
		ded = 1;
		break;

	case CHECKER:
		/* Default spelling checker for use with the "c" command */
		if( *(++pp) == NULL )
		{
			fprintf(stderr,"You need to specify a spelling checker after 'checker' in your .sendrc.\n");
			fprintf(stderr,"Defaulting to %s.\n",checker);
		}
		else
			strcpy( checker, *pp );
		break;

   	case PAGING : 
	   	/* Turn on paging during message review */
   		pflag = 1;
   		break;

	case CFILE:
		cflag = 1;
		break;

	case QFILE:
		qflag = 1;
		/* DROP */
	case NOFILE:
		cflag = 0;
		break;

	case ADDHEADER:
		/* Add a header line to the message */
		++pp;
		if (isstr(*pp))
			addheader(pp[0], pp[1]);
		break;

	default:
		fprintf(stderr,"GETUINFO - NO SUCH OPTION key = %d\n", key);

	} /* end of switch */
}
/* Prompts for a header with print as prompt and gathers it up */

gethead (print, buf, flag)
char    print[];
register char   *buf;
int     flag;                     /* print contents, if any?            */
{
    char    c;
    int retval;                   /* type of FLD modification           */
    register int    n;
    register int    max = S_BSIZE;

    if (flag && *buf)             /* print what already is there        */
	printf (shrtfmt, print, buf);

    flag = TRUE;                  /* always & only look at first char   */
    do
    {
	printf ("%s", print);     /* prompt                             */
	if (*buf == '\\')
	{
	    *buf++ = ',';
	    *buf++ = '\n';
	}
	fflush (stdout);

	if (flag)
	{
	    flag = FALSE;         /* ignore first char of next lines    */
	    switch (c = getchar ())
	    {
		case '-':         /* delete single name                */
			retval = ITMRMV;
			break;
		case '#':         /* delete all contents     */
		    *buf = 0;
		    while (getchar () != '\n');
		    return (FLDREMV);

		case '\n':        /* leave it as it is                  */
		    return (FLDSAME);

		case '+':         /* append to end of field             */
		    retval = FLDADD;
		    if (*buf != '\0')
		    {                 /*   if not already empty             */
			buf = &buf[strlen (buf)];
				      /* point to end of buf                */
			*buf++ = ','; /* assume it's an address field       */
			*buf++ = ' ';
			max -= 2;
		    }
		    break;

		default:          /* copy over it                       */
		    retval = FLDOVER;
		    *buf++ = c;
		    max--;
		    break;
	    }
	}
	n = gather (buf, max);    /* get rest of header line            */
	max -= n;
	buf = &buf[n - 1];
    }
    while (*buf == '\\');         /* user wants to add more?            */
    return (retval);
}

prefix (buf, pre)
char   *buf,
       *pre;
{
    extern char chrcnv[];

    while (*pre)
	if (chrcnv[*buf++] != chrcnv[*pre++])
	    return (FALSE);
    return (TRUE);
}


getaddr (print, buf, flag, defhost)
    char print[];
    char *buf;
    int flag;
    char defhost[];
{
    char addrin[S_BSIZE];     /* temporary store for user input     */
    char    *dn, *gstrindex();

    if (flag && *buf)             /* print what already is there        */
	printf (shrtfmt, print, buf);

    addrin[0] = '\0';             /* mark temp buffer as empty          */
    switch (gethead (print, addrin, NO))
    {
	case FLDREMV:             /* erase any existing contents        */
	    buf[0] = '\0';
	    break;

	case ITMRMV:		 /* delete single name			*/
		dn = gstrindex(addrin, buf);
		if(dn == 0){
			fprintf(stderr," no such name in the list; name = '%s'\n",addrin);
			break;
		}
		dlt_name( buf, dn );
	    break;
	case FLDSAME:             /* leave as is                        */
	    break;

	case FLDOVER:             /* overwrite existing contents        */
	    buf[0] = '\0';        /* erase existing, then add           */
	    aliasmap(buf, addrin, defhost);
	    break;

	case FLDADD:              /* append to existing                 */
	    aliasmap(buf, addrin, defhost);
	    break;
    }
}

/*
**	S P E C I A L  S T R I N D E X  F U N C T I O N
**
** Specially modified by George Hartwig to return a pointer to the beginning
** of the requested string instead of the offset.
*/

char *gstrindex (str, target)           /* return column str starts in target */
register char   *str,
		*target;
{
    char *otarget;
    register short slen;

    for (otarget = target, slen = strlen (str); ; target++)
    {
	if (*target == '\0')
	    return ((char *) 0);

	if (equal (str, target, slen))
	    return( target );
    }
}
dlt_name(buf, pos)
char *buf, *pos;

/* buf is a pointer to a character array and pos is a pointer to a position  */
/* within that array. The purpose of this routine is delete the name field   */
/* pointed to by pos. In order to do this we will search backward to either  */
/* comma ',' or a colon ':' (if a comma, we will then delete that comma also */
/* and then delete forward until another comma or 			     */
/* a end-of-string '\0' is found. 					     */

{
	char tmpbuffer[S_BSIZE];
	char *sd, *ed, *cp;
	int quote_flag;


	cp = pos;
	quote_flag = 0;

	while(cp >= buf ) {
		if( *cp == '"' )
			if( quote_flag == 0 )
				quote_flag = 1;
			else
				quote_flag = 0;
		if( *cp == ',' && quote_flag == 0 )
			break;
		else
			cp--;
	}
	sd = cp;

	cp = pos;

	while( *cp != '\0' ) {
		if( *cp == '"' )
			if( quote_flag == 0 )
				quote_flag = 1;
			else
				quote_flag = 0;
		if( *cp == ',' && quote_flag == 0 )
			break;
		else
			cp++;
	}

	ed = cp;

	/* at this point *sd should point at the comma preceding the name
	** to be deleted or the beginning of the line and ed should be
	** pointing to the trailing comma or the end of the line. So it is
	** necessary at this point to determine just which of these cases
	** have occurred so that the string may be properly reassembled.
	*/

	if( *sd == ',' && *ed == ',' ){
		sd++;
		*sd = '\0';
		ed++;
		sprintf( tmpbuffer, "%s%s", buf, ed);
	}

	else
	if( sd <= buf ){
		ed++;
		strcpy(tmpbuffer, ed);
	}
	else
	if( *ed == '\0' ){
		*sd = '\0';
		strcpy(tmpbuffer, buf);
	}
	strcpy( buf, tmpbuffer );
}

/*
 *	checksignature ()
 *
 *  Verifies that the signature string does not contain any special
 *  characters or is quoted.  Also checks for blank "name" portion.
 */
checksignature (sp)
register char *sp;
{
	char tptr[128], *savptr, *ptr;
	char *rindex();
	static char quote[] = "\"";
	
	savptr = sp;
	while (*sp && isspace(*sp))  /* eat leading whitespace */
		sp++;
	if (*sp == '\0') {
		fprintf(stderr,"Signature is blank\n");
		return (-1);     /* must have some signature */
	}
	while (*sp) {
		switch (*sp) {
		case '<':
		case '>':
		case '@':
		case ',':
		case ';':
		case ':':
		case '.':	/*  Arggggg...  */
		case '[':
		case ']':
			/* surround bad signature with quotes */
			tptr[0] = '\0';
			strcat( tptr, quote );
			sp = savptr;
			strcat( tptr, sp );
			if( ptr = rindex( tptr, ' ' ) )
				*ptr = '\0';
			strcat( tptr, quote );
			strcpy( sp, tptr );
			return( 0 );

		case '\\':
			sp++;
			if (*sp == '\0')
				return (-1);
			break;

		case '"':
			for (sp++; *sp != '"'; sp++) {
				if (*sp == '\\') {
					sp++;
					if (*sp == '\0')
						return (-1);
				} else if (*sp == '\0') {
					fprintf(stderr,"Unmatched '\"' in signature\n");
					return (-1);
				}
			}
			break;

		default:
			break;
		}
		sp++;
	}

	return (0);
}

addheader(name, data)
char	*name;
char	*data;
{
	register struct header *nhp;
	register struct header *ohp;
	register char *cp;
	char	buf[LINESIZE];

	if(!isstr(name))
	    return;
	for (cp = name; *cp; cp++)
		if (!isprint(*cp) && *cp != '\t') {
			fprintf(stderr,"Illegal character in header name.\n");
			return;
		}
	data = (isstr(data) ? data : "");
	for (cp = data; *cp; cp++)
		if (!isprint(*cp) && *cp != '\t') {
			fprintf(stderr,"Illegal character in header data.\n");
			return;
		}
	if (headers) {
		for (ohp = headers; !lexequ(name, ohp->hname); ohp = ohp->hnext)
			if (ohp->hnext == (struct header *)0)
				break;
		if(lexequ(name, ohp->hname)) {
			strncpy(ohp->hdata, data, sizeof(ohp->hdata)-1);
			return;
		}
	}
	nhp = (struct header *)malloc(sizeof (*nhp));
	sprintf(buf, "%s:  ", name);
	if (nhp == NULL || (nhp->hname = strdup(buf)) == NULL) {
		fprintf(stderr,"No memory for header '%s'\n", buf);
		if (nhp)
			free(nhp);
		return;
	}
	strncpy(nhp->hdata, data, sizeof(nhp->hdata)-1);
	nhp->hnext = (struct header *)0;
	nhp->hdata[sizeof(nhp->hdata)-1] = '\0';
	if (headers == (struct header *)0) {
		headers = nhp;
		return;
	}
	ohp->hnext = nhp;
}
