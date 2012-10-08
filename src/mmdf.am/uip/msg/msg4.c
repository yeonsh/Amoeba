/*	@(#)msg4.c	1.1	91/11/21 11:43:49 */
/*
 *			M S G 4 . C
 *
 * This is the fourth part of the MSG program.
 *
 * Functions -
 *	txtmtch		search for given pattern in text of messages
 *	mainbox		test to see if we are processing home mailbox
 *	gethead		fetch and scan header from message on disk
 *	gothdr		check input line for header field match
 *	prefix		compare string to RFC822 header line prefix
 *	confirm		ask a yes/no question
 *	gather		read in a line
 *	suckup		discard rest of line
 *	echochar
 *	ttychar
 *	tt_init		fetch tty modes
 *	tt_raw		switch to raw mode
 *	tt_norm		switch to cooked mode
 *	onint		interrupt catcher
 *	onnopipe	broken pipe catcher
 *	onhangup	hangup signal catcher
 *	onstop		stop signal catcher
 *	error		error handler
 *	xomsgrc		sets up xtra options when .msgrc file exists
 *	xostat		xtra options status printer
 *	strend		detects keyword at start of str and returns pointer
 *	xfgets		specials fgets that removes nulls
 *
 *		R E V I S I O N   H I S T O R Y
 *
 *	06/09/82  MJM	Split the enormous MSG program into pieces.
 *
 *	11/22/82  HW	Added Xtra options and header keyword stripping.
 *
 *	12/04/82  HW	Added SIGPIPE catcher and fixed SIGINT after pipe
 *			bug.
 *
 *	06/01/84  HW	Added ctrl char filter.
 *
 *	04/20/88  ECB	Added 2-window-answer-mode-filter option.
 */
#include "util.h"
#include "mmdf.h"
#include "pwd.h"
#include "signal.h"
#include "sys/stat.h"
#ifdef SYS5
#include "termio.h"
#include "string.h"
#else SYS5
#include "sgtty.h"
#include "strings.h"
#endif SYS5
#include "./msg.h"

static	int save_istty;

char	*keywds[MAXKEYS] = {
	"via",
	"message",
	"remailed-date",
	"remailed-to",
	"sender",
	"mail",
	"article",
	"origin",
	"received",
	0
};
		
/*
 *			T X T M T C H
 */
txtmtch()
{
	long size;
	int retval;
	char line[LINESIZE];

	mptr = msgp[msgno];

	fseek( filefp, mptr->start, 0);
	retval = FALSE;
	for( size = mptr->len; size > 0; size -= strlen(line))  {
		if( xfgets( line, sizeof( line), filefp) == NULL )
			break;
		if( strindex( key, line) != NOTOK)  {
			retval = TRUE;
			break;
		}
	}

	return( retval);
}
/*--------------------------------------------------------------------*/

/*--------------------------------------------------------------------*/

/*
 *			M A I N B O X
 *
 * last part of filename => main?
 */
mainbox()
{
	register char *cp;

	for( cp = filename; *cp++; );		/* get to end of name */
	while( --cp != filename && *cp != '/');
	/* find last part */
	if( *cp == '/')
		cp++;

	ismainbox = strcmp(cp, "mailbox") == 0;
	return( ismainbox );
}

/*
 *			G E T H E A D
 *
 * This routine seeks to the beginning of the message in the disk file,
 * reads in the header, and scans it to find the values of the various
 * fields.  Fields which the caller is not interested are passed as
 * NULLs.
 *
 * NOTE that the fseek() can cause great inefficiency if the message
 * happens to start on an ODD byte in the file...
 *
 * Typical use now is to get info for header on answer/reply.
 */
gethead( datestr, fromstr, sndstr, rplystr, tostr, ccstr, subjstr)
char *datestr;		/* where to save Date     field */
char *fromstr;		/* where to save From     field */
char *sndstr;		/* where to save Sender   field */
char *rplystr;		/* where to save Reply-To field */
char *tostr;		/* where to save To       field */
char *ccstr;		/* where to save cc       field */
char *subjstr;		/* where to save Subj     field */
{
	char line[LINESIZE];
	char curhdr[M_BSIZE];

	if( fromstr != 0)
		fromstr[0] = '\0';
	fseek( filefp, mptr->start, 0);			/* deadly! */
	curhdr[0] = '0';
	while(
		xfgets( line, sizeof( line), filefp) != NULL &&
		line[0] != '\n' &&
		strcmp( delim1, line) != 0 &&
		strcmp( delim2, line) != 0
	 )  {
		if( datestr != 0)
			if( gothdr( curhdr, "date:", line, datestr))
				continue;

		if( fromstr != 0 && isnull( fromstr[0]))
			if( gothdr( curhdr, "from:", line, fromstr))
				continue;

		if( rplystr != 0)
			if( gothdr( curhdr, "reply-to:", line, rplystr))
				continue;

		if( sndstr != 0)
			if( gothdr( curhdr, "sender:", line, sndstr))
				continue;

		if( tostr != 0)
			if( gothdr( curhdr, "To:", line, tostr))
				continue;

		if( ccstr != 0)
			if( gothdr( curhdr, "cc:", line, ccstr))
				continue;

		if( subjstr != 0)
			if( gothdr( curhdr, "subject:", line, subjstr))
				continue;

		/*
		 * If we got here, the line was of a type that we don't
		 * care about.  Zap curhdr, as it will not have been updated
		 * unless the line was processed by gothdr(), and we want
		 * to avoid processing extraneous continuations.
		 */
		 strcpy( curhdr, "boring-header-line:" );
	}
}

/*
 *			G O T H D R
 */
gothdr( curname, name, src, dest)
char *curname;		/* current field name */
char *name;		/* test field name */
char *src;		/* test input line */
char *dest;		/* where to put data part at end */
{
	unsigned int destlen;

	if( isspace( *src ))
	{
		/* This is a continuation line.  Check "remembered" field */
		if( strcmp( curname, name ) != 0 )
			return( FALSE );

		/* Continuatin of correct header line.  Retain "remembered"
		/* field.  Get rid of name & extra space from input line. */
		compress( src, src);

		/*** FALL OUT ***/
	}
	else if (prefix( src, name ))
	{
		/* New line and field name matches, lets go! */

		/* Remember the "current field" */
		strcpy( curname, name);

		if( strcmp( name, "date:" ) != 0 )
			/* get rid of name & extra space */
			compress( &src[strlen( name )], src);
		else
			/* Don't compress dates! */
			strcpy( src, &src[strlen( name )] );

		/*** FALL OUT ***/
	}
	else
	{
		/* New line but field does not match, skip it */
		return(FALSE);
	}

	/* Common code for TRUE replies */
	if( isnull( dest[0]))  {
		/* first entry */
		strcpy( dest, src);
	}  else  {
		/* add to end of field, if room */
		destlen = strlen( dest);
		if( (strlen( src) + destlen) < M_BSIZE - 1)
			sprintf( &dest[destlen], "\n%s%c", src, '\0');
	}
	return( TRUE);
}

/*
 *			P R E F I X
 */
prefix( target, prefstr)
register char  *target,
*prefstr;
{
	for( ; *prefstr; target++, prefstr++)
		if( uptolow( *target) != uptolow( *prefstr))
			return( FALSE);

	return( isnull(*prefstr));
}

/*--------------------------------------------------------------------*/

/*
 *			C O N F I R M
 *
 * Request a yes/no answer
 */
confirm(msg,lfflag)
char	*msg;
int	lfflag;
{
	register char c;

	if(msg == (char *)0)
		printf(" [Confirm] (y) ");
	else
		printf("%s (y) ", msg);
	if( !verbose)
		suckup();			/* BRL */
	c = ttychar();
	if( !verbose)
		suckup();		  /* suck up rest of line */

	switch( c)  {

	case 'Y': 
	case 'y': 
	case '\004':
	case ' ':
	case '\n': 
		if( verbose) {
			if( lfflag == DOLF )
				printf( "yes\r\n");
			else
				printf("\r                               \r");
		}
		return( (int) c );

	case 'N':
	case 'n': 
	case 003:				/* Ctl/C */
	case 007:				/* Ctl/G, for EMACS types */
	case '/':
		if (verbose) printf( "no\r\n");
		return( FALSE);

	default:
	case '?':
		if (verbose)
			printf("\r\nY, SPACE, or RETURN to confirm, N to abort\r\n");
		else
			printf("\r\nY<CR> or just <CR> to confirm, N<CR> to abort\r\n");
		fflush( stdout );
		return( confirm(msg, lfflag) );		/* recurse */
	}
}

/*
 *			G A T H E R
 *
 * gobbles a line of max length from user, stores into string 'sp'.
 * ASSUMES first char is already in sp[0].
 */
gather( sp, max)
char *sp;
{
	register char c;
	register char *p;

	for( p = sp, c = *sp;; ) {
		if( c == '\003' )		/* Error - cancel */
			error(" ^C\r\n");
		else if( c == '\004' )
			error(" ^D\r\n");
		else if( c == ch_erase ) {	/* Delete previous char */
			if( p > sp ) {
				p--;
				printf("\b \b");
			}
		}
		else if( c == '\n' || c == '\r' ) {	/* Done */
			*p = '\0';
			if (verbose) printf("\r\n");
			return;
		}
		else {
			*p++ = c;
			if( (p - sp) >= max )
				error(" Line too long\r\n");
			if (verbose) putc(c, stdout);
		}
		c = ttychar();
	}
}

/*
 *			S U C K U P
 */
suckup()
{
	if( lastc == '\n')
		return;
	while( (nxtchar = echochar()) != '\n' && nxtchar != '\004' );
}

/*
 *			E C H O C H A R
 */
echochar()
{
	register char c;

	c = ttychar();
	if( verbose)  {
		putchar( c);
		if( c == '\n')
			putchar( '\r');
	}
	return( c);
}

/*
 *			T T Y C H A R
 */
ttychar()
{
	register int c;

	fflush( stdout);
	if( (c = getchar()) == EOF) {
		if( ferror( stdin)) {
			tt_norm();
			closeup( -1);
		}
		if( feof( stdin) )
			c = '\004';
	}

	c = toascii( c);		/* get rid of high bit */

	if( c == '\r')
		c = '\n';

	return( lastc = c);
}
/*--------------------------------------------------------------------*/

#ifdef SYS5
static struct termio orig_ioctl;
static struct termio raw_ioctl;
#define	TIOCSETN TCSETAW		/* Crafty... (be careful!) */
#else SYS5
static struct sgttyb   orig_ioctl;
static struct sgttyb   raw_ioctl;
#endif SYS5

/*
 *			T T _ I N I T
 */
tt_init()
{
#ifdef SYS5
	if( ioctl( fileno( stdin), TCGETA, &orig_ioctl) == -1) {
#else
	if( ioctl( fileno( stdin), TIOCGETP, &orig_ioctl) == -1) {
#endif
		verbose = 0;
		istty = FALSE;
	}
	else {
		raw_ioctl = orig_ioctl;
#ifdef SYS5
		ch_erase = orig_ioctl.c_cc[VERASE];
		raw_ioctl.c_iflag &= ~(ISTRIP | INLCR | IGNCR | ICRNL);
		raw_ioctl.c_oflag &= ~OPOST;
		raw_ioctl.c_lflag &= ~(ICANON | ECHO);
		raw_ioctl.c_cc[4] = 1;
		raw_ioctl.c_cc[5] = 0;
#else
		ch_erase = orig_ioctl.sg_erase;
		raw_ioctl.sg_flags |= CBREAK;
		raw_ioctl.sg_flags &= ~ECHO;
#endif
		istty = TRUE;
	}
	save_istty = istty;
}

/*
 *			T T _ R A W
 */
tt_raw()
{
	if( verbose)  {
		fflush( stdout);
		if( istty == TRUE ) {
			if( ioctl( fileno( stdin), TIOCSETN, &raw_ioctl) == -1) {
				perror( "problem setting raw terminal modes");
				verbose = 0;
			}
		}
	}
}

/*
 *			T T _ N O R M
 */
tt_norm()
{
	fflush( stdout);
	if( verbose)  {
		if( istty == TRUE ) {
			if( ioctl( fileno( stdin), TIOCSETN, &orig_ioctl) == -1)
				perror( "problem resetting normal terminal modes" );
		}
	}
}
/*--------------------------------------------------------------------*/

/*
 *			O N I N T
 */
onint()
{
	signal( SIGINT, onint);
	error( "\r\n" );
}

/*
 *			O N H A N G U P
 *
 * This routine is called when we get a hangup signal.
 */
onhangup()
{
	signal (SIGHUP, SIG_IGN);
	signal (SIGINT, SIG_IGN);
#ifdef SIGTSTP
	signal( SIGTSTP, SIG_IGN);
#endif SIGTSTP

	/* Prevent chatter on a closed line */
	close( 1 );
	close( 2 );
	open( "/dev/null", 0 );		/* Assuming fd will be 1 */
	open( "/dev/null", 0 );		/* Assuming fd will be 2 */
	bprint = bdots = OFF;
	fclose( stdout );

	/* Overwrite the binary box */
	if( binaryvalid )
		binbuild();
	if( binfp != (FILE *) NULL)
		lk_fclose(binfp, binarybox, (char *)0, (char *)0);
	_exit(0);
}

#ifdef SIGTSTP
/*			O N S T O P
 *  
 *  SIGSTOP signal catcher
 */
onstop() {
	int pid;
	int saveverbose = verbose;
	
	pid = getpid();
	if( binsavflag == ON )
		binbuild();
	tt_norm();
	kill(pid,SIGSTOP);
	verbose = 1;
	tt_norm();
	verbose = saveverbose;
	tt_raw();
	signal(SIGTSTP,onstop);
	error("\r\n");
}
#endif SIGTSTP

/*			O N N O P I P E
 *
 *	Catches SIGPIPE
 */
onnopipe() {
	signal( SIGPIPE, onnopipe );
	outfd = -1;
	error( " msg: Broken pipe\r\n" );
}

/*
 *			E R R O R
 *
 *  An error condition has occured.  Report it to the user, and long-jump
 * back to the top of the mainline.
 *
 *  Note:  We should probably close ALL extraneous file descriptors,
 * to prevent getting hung up with an exclusive-use file being left
 * open.
 */
error( s)
char *s;
{
	istty = save_istty;
	if( outfp != NULL && fileno(outfp) > 0 )  {
		fclose( outfp );
		outfp = NULL;
	}

	if( exclfd > 0 )  {
		close( exclfd );
		exclfd = -1;
	}

	fputs( s, stdout);
	fflush( stdout);

	tt_raw();

	if( !(verbose || nxtchar == '\n'))
		suckup();

	longjmp( savej, 1);
}

/*		X O M S G R C
 *
 *	Reads user option file .msgrc and sets options
 */
xomsgrc( filept )
	FILE	*filept;
{
	char	tbuf[LINESIZE];
	int	nkeysrd;
	char	*np, *nc;
	static	char sendprog[LINESIZE];
	
	nkeysrd = 0;
	while( xfgets( tbuf, sizeof(tbuf), filept ) != NULL ) {

		if( (np = index( tbuf, '\n' )) == NULL )
			continue;
		*np = '\0';

		if( tbuf[0] == '\0' || tbuf[0] == '#' )
			continue;

		if( (np = strend( "strip", tbuf )) != NULL ) {
			if( nkeysrd+1 >= sizeof(keywds)/sizeof(char *) )
				printf("Too many strip lines - %s IGNORED\r\n", tbuf );
			else if( *np != '\0' ) {
				keywds[nkeysrd] = malloc( strlen( np )+1 );
				for( nc = np; *nc != '\0'; nc++ )
					*nc = uptolow( *nc );
				strcpy( keywds[nkeysrd++], np );
			}
		}

		else if( (np = strend( "numberedlist", tbuf )) != NULL )
			prettylist = ON;

		else if( (np = strend( "nonumberedlist", tbuf )) != NULL )
			prettylist = OFF;

		else if( (np = strend( "nostrip", tbuf )) != NULL )
			keystrip = OFF;

		else if( (np = strend( "quicknflag", tbuf )) != NULL )
			quicknflag = ON;

		else if( (np = strend( "noquicknflag", tbuf )) != NULL )
			quicknflag = OFF;

		else if( (np = strend( "quickexit", tbuf )) != NULL )
			quickexit = ON;

		else if( (np = strend( "noquickexit", tbuf )) != NULL )
			quickexit = OFF;

		else if( (np = strend( "control-l", tbuf )) != NULL )
			doctrlel = ON;

		else if( (np = strend( "nocontrol-l", tbuf )) != NULL )
			doctrlel = OFF;

		else if( (np = strend( "zbinsave", tbuf )) != NULL )
			binsavflag = ON;

		else if( (np = strend( "nozbinsave", tbuf )) != NULL )
			binsavflag = OFF;

		else if( (np = strend( "twowinfil", tbuf )) != NULL ) {
			if( *np != '\0' ) {
				strcpy( twowinfil, np );
			}
		}

		else if( (np = strend( "savebox", tbuf )) != NULL ) {
			if( *np != '\0' ) {
				strcpy( defoutfile, np );
				strcpy( defmbox, np );
			}
		}

		else if( (np = strend( "draftdir", tbuf )) != NULL ) {
			if( *np != '\0' ) {
				if( *np == '/' ) {
					sprintf(draft_work,"%s/draft_work",np);
					sprintf(draft_original,"%s/%s",np,draftorig);
				}
				else {
					sprintf(draft_work,"%s/%s/draft_work",homedir,np);
					sprintf(draft_original,"%s/%s/%s",homedir,np,draftorig);
				}						
			}
		}

		else if( (np = strend( "draftorig", tbuf )) != NULL ) {
			if( *np != '\0' ) {
				strcpy(draftorig,np);
				np = rindex(draft_original,'/');
				*++np = '\0';
				strcat(draft_original,draftorig);
			}
		}
		
		else if( (np = strend( "sendprog", tbuf )) != NULL ) {
			if( *np != '\0' ) {
				if( *np == '/' )
					strcpy(sendprog,np);
				else
					sprintf(sendprog,"%s/%s",homedir,np);

				sndname = sendprog;
			}
		}

		else if( (np = strend( "pagesize", tbuf )) != NULL )
			pagesize = atoi(np);

		else if( (np = strend( "linelength", tbuf )) != NULL )
			linelength = atoi(np);

		else if( (np = strend( "writemsg", tbuf )) != NULL )
			wmsgflag = ON;

		else if( (np = strend( "nowritemsg", tbuf )) != NULL )
			wmsgflag = OFF;

		else if( (np = strend( "ctrlfil", tbuf )) != NULL )
			filoutflag = ON;

		else if( (np = strend( "noctrlfil", tbuf )) != NULL )
			filoutflag = OFF;

		else if( (np = strend( "mdots", tbuf )) != NULL )
			mdots = ON;

		else if( (np = strend( "nomdots", tbuf )) != NULL )
			mdots = OFF;

		else if( (np = strend( "bdots", tbuf )) != NULL )
			bdots = ON;

		else if( (np = strend( "nobdots", tbuf )) != NULL )
			bdots = OFF;

		else if( (np = strend( "bprint", tbuf )) != NULL )
			bprint = ON;

		else if( (np = strend( "nobprint", tbuf )) != NULL )
			bprint = OFF;

		else if( (np = strend( "paging", tbuf )) != NULL )
			paging = ON;

		else if( (np = strend( "nopaging", tbuf )) != NULL )
			paging = OFF;

		else if( (np = strend( "verbose", tbuf )) != NULL )
			verbose = ON;

		else if( (np = strend( "noverbose", tbuf )) != NULL )
			verbose = OFF;

		else
			printf("Unknown .msgrc option: %s\r\n",tbuf);
	}	
	if( nkeysrd > 0 ) 
		keywds[nkeysrd] = 0;
}

/*		S T R E N D
 *
 *	searches for str at beginning of target. If not found,
 * returns NULL. If found, skips white space and returns pointer
 * to first character following white space.
 */
char *
strend (str, target)
	char   *str, *target;
{
	register int slen;

	if( *target == '\0' )
	    return (NULL);

	slen = strlen( str );
	if( lexnequ( str, target, slen) != TRUE )
		return( (char *) NULL );

	for( target += slen; *target == '\t' || *target == ' '; target++ )
		;

	return (target);
}

/*		X O S T A T
 *
 *	Prints Xtra options status
 */
xostat() {
	register int	i;

	printf("\r\nMail file: %s   Binary file: %s\r\n",filename,binarybox);
	printf("Save file: %s", defmbox );
	printf("   Send program: %s\r\n",sndname );
	printf("drafts: %s  %s\r\n", draft_work,draft_original);

	printf("\nbdots: %s\t\t\t\tmdots: %s",
	  bdots?"on.":"off.", mdots?"on.":"off." );
	printf("\r\nbprint: %s\r\n",
	  bprint?"on.":"off." );

	printf("\nNumbered message list: %s\r\n",prettylist?"on.":"off.");
	printf("Writemsg flag: %s\r\n",wmsgflag?"on.":"off.");
	printf("Ctrl-char filter: %s\t\t\t",filoutflag?"on.":"off.");
	printf("Ctrl-L: %s\r\n",doctrlel?"on.":"off.");
	printf("Paging: %s\t\t\t\tPagesize: %d\r\n",
	  paging?"on.":"off.", pagesize);
	printf("Quicknflag: %s\t\t\tLinelength: %d\r\n",
	  quicknflag?"on. ":"off.",linelength);
	printf("Strip: %s\t\t\t\tVerbose: %s\r\n",
	  keystrip?"on.":"off.", verbose?"on.":"off." );

	printf("Strip keywords:\r\n");
	for( i = 0; keywds[i] != 0; i++ )
		printf("\t%s\r\n", keywds[i] );
}
/*			X F G E T S
 *  
 */
char *
xfgets(s, n, iop)
char *s;
register FILE *iop;
{
	register c;
	register char *cs;

	cs = s;
	while (--n>0 && (c = getc(iop))>=0) {
		if( c == 0 )
			*cs++ = '@';
		else
			*cs++ = c;
		if (c=='\n')
			break;
	}
	if (c<0 && cs==s)
		return(NULL);
	*cs = '\0';
	return(s);
}

/*                                                                      */
/*      Determine if the two given strings are equivalent.              */
/*                                                                      */

lexnequ (str1, str2, len)
register char   *str1,
		*str2;
register int	len;
{
	extern char chrcnv[];

	while ( (len-- > 0) && (chrcnv[*str1] == chrcnv[*str2++]) ) {
		if (len == 0 || *str1++ == 0)
			return (TRUE);
	}
	return (FALSE);
}
