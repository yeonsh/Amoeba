/*	@(#)msg2.c	1.1	91/11/21 11:43:31 */
/*
 *			M S G 2 . C
 *
 * This is the second part of the MSG program.
 *
 * Functions -
 *	rdnxtfld	read next field from input
 *	prhdr		print header on stdout
 *	hdrfile		print header on outfp
 *	hdrout		print header on specified FILE p
 *	gomsg		goto a specific message
 *	delmsg		delete specified message
 *	undelmsg	un-delete specified message
 *	keepmsg		mark specified message for keeping
 *	getfn		
 *	cpyiter
 *	cppipe
 *	cpopen
 *	dolstmsg	list the selected messages, maybe separated
 *	lstmsg		list one message, maybe separated
 *	lstbdy		list body of one message
 *	movmsg		"move" a message
 *	putmsg		"put" a message
 *	writmsg		write specified message onto given FD
 *	writbdy		write body of specified message onto given FD
 *	prmsg		print specified message onto terminal
 *	ansiter		top-level iteration for "answer"
 *	ansqry		enquire who to send answer to
 *	ansmsg		create header for answer
 *	fwditer		top-level iteration for "forward"
 *	fwdmsg		copy one forwarded message, with markers
 *	fwdpost		add trailer to forwarded message
 *	srchkey		Keyword search for stripping obnoxious header lines.
 *	filout		Filters control characters before writing on terminal.
 *	makedrft	Make a file containing one or more messages
 *
 *		R E V I S I O N   H I S T O R Y
 *
 *	06/08/82  MJM	Split the enormous MSG program into pieces.
 *
 *	09/10/82  MJM	Modified to use STDIO for output to files, too.
 *
 *	11/14/82  HW	Added keyword filter to ignore Via, Remailed, etc.
 */
#include "util.h"
#include "mmdf.h"
#include "pwd.h"
#include "signal.h"
#include "sys/stat.h"
#include "sgtty.h"
#include "./msg.h"

extern FILE *popen();

/*
 *			R D N X T F L D
 *
 * read next field from input
 */
rdnxtfld()
{
	register char   c;

	do  {
	    c = ttychar();
	}  while( (isspace( c) || c == ch_erase) && c != '\n' );

	return(c);
}

/*
 *			P R H D R
 */
prhdr()
{
	hdrout( stdout, TRUE);
}

/*
 *			H D R F I L E
 */
hdrfile()
{
	hdrout( outfp, FALSE);
}




hdrout( fp,crflag)
FILE *fp;
int crflag;
{
	char lbuf[LINESIZE];

	sprintf( lbuf, "%4d%c%c%c%c%c%c%5ld: %-9.9s %-15.15s %.30s%s\n",
		msgno,
		mptr->flags & M_NEW ? 'N' : ' ',
		mptr->flags & M_DELETED ? 'D' : ' ',
		mptr->flags & M_KEEP ? 'K' : ' ',
		mptr->flags & M_ANSWERED ? 'A' : ' ',
		mptr->flags & M_FORWARDED ? 'F' : ' ',
		mptr->flags & M_PUT ? 'P' : ' ',
		mptr->len,
		mptr->datestr,
		mptr->from,
		mptr->subject,
		crflag ? "\r" : ""
	);
	filout( lbuf, fp );
}

/*--------------------------------------------------------------------*/

gomsg()
{
	status.ms_curmsg = msgno;		  /* set the current message number     */
}

delmsg()
{
	mptr->flags |= M_DELETED;
	mptr->flags &= ~M_KEEP;
}

undelmsg()
{
	mptr->flags &= ~(M_DELETED|M_KEEP);
}

keepmsg()
{
	mptr->flags |= M_KEEP;
	mptr->flags &= ~M_DELETED;
}

/*
 *			G E T F N
 *
 * Get a file name for the user.
 */
getfn( s, f, def)
char    *s;		/* Prompt string to provoke user */
char	*f;		/* place to put resulting answer */
char	*def;		/* optional default */
{
	char    tmpbuf[LINESIZE];
	char	name[LINESIZE];
	struct passwd *getpwuid(), *getpwnam();
	register char *t, *n;
	register struct passwd *pw;

	tt_norm();
	fputs( s, stdout);
	fflush( stdout);

	strcpy( oldfile, f);

	if( gets( tmpbuf) == NULL || isnull( tmpbuf[0]))  {
		if( def == (char *)0)
			error( "no filename specified\r\n");
		else  {
			strcpy( f, def);
			printf( "%s...\r\n", f);
		}
	}  else  {
		/* Remove leading whitespace */
		for( t = tmpbuf; isspace(*t); t++ )
			;
		if( *t == '~' ) {
			/* Expand */
			for( n = name; *++t && *t != '/'; *n++ = *t )
				;
			*n = '\0';
			if( name[0] == '\0' )
				pw = getpwuid(getuid());
			else
				pw = getpwnam(name);
			if( pw == NULL )
				error("~name not found\r\n");
			(void) strcpy(f, pw->pw_dir);
			(void) strcat(f, t);
		} else
			strcpy(f, t);
	}

	nxtchar = '\n';		  /* note the last character typed      */
	tt_raw();
}

/*
 *			C P Y I T E R
 */
cpyiter( fn, iterfl, post)
int    ( *fn)();		/* per-message function               */
int	iterfl;			/* To iterate or not to iterate	      */
int    ( *post)();		/* post-process function              */
{
	int save_istty = istty;

	if( outfile[0] == '|') {	/* output filter, not file */
		tt_norm();
		cppipe();		/* get the output pipe */
	} else
		cpopen();		/* get the output file */

	istty = 0;

	if( iterfl == DOIT )
		doiter( fn);
	else
		(*fn)();

	if(post != 0)
		(*post)();

	istty = save_istty;

	if(outfile[0] == '|') {	/* collect the child */
		pclose( outfp );	/* done sending to file/pipe */
		signal( SIGINT, onint );
		tt_raw();
	} else
		lk_fclose (outfp, outfile, NULL, NULL);

	outfd = -1;
}

/*
 *			C P P I P E
 */
cppipe()
{
	char	buf[LINESIZE];

	fflush(stdout);
	if ((outfp = popen(&outfile[1], "w")) == NULL)
		error( "problem starting pipe command\r\n");

	outfd = -1;
	return;
}

/*
 *			C P O P E N
 *
 * open a file to copy into
 */
cpopen()
{

	/* EXCLUSIVE open the file, as we are writing */
	if( (outfd = lk_open( outfile, 1, (char *)0, (char *)0, 5)) < 0)  {
		switch( errno)  {
		case ENOENT:
			if( !autoconfirm)  {
				printf( "Create '%s'", outfile);
				if( !confirm((char *)0,DOLF))
					error( "");
			}
			if( (outfd = creat( outfile, sentprotect)) < 0)  {
				printf( "can't create '%s'", outfile);
				error( "\r\n");
			}

			close( outfd);
			/* EXCLUSIVE open, since writing */
			if( (outfd = lk_open( outfile, 1, (char *)0, (char *)0, 5)) < 0)  {
				printf( "can't open '%s'", outfile);
				error( "\r\n");
			}
			break;

		case EBUSY:
			printf( "'%s' is busy; try later", outfile);
			error( "\r\n");

		default:
			perror(outfile);
			error( "\r");
		}
	}

	/*
	 * Set up for STDIO output using the exclusive
	 * write-only FD.  The fclose( outfd ) will flush
	 * the buffers.  The "a" append is necessary in
	 * the event that the file already exists.
	 */
	if( (outfp = fdopen( outfd, "a" )) == NULL ) {
		error( "can't fdopen outfile\n" );
		outfd = -1;
	}
}

/*
 *			D O L S T M S G
 *
 * list the selected messages, maybe separated
 */
dolstmsg()
{
	doiter ( lstmsg);
}

/*
 *			L S T M S G
 *
 * list one message, maybe separated
 */
lstmsg()
{
	if( lstsep && lstmore)
		fputs( "\f\n", outfp );
	lstmore = TRUE;

	if( prettylist) {
		fprintf(outfp, "(Message # %d: %ld bytes", msgno, mptr->len );
		if( mptr->flags & M_DELETED )	fprintf(outfp, ", Deleted");
		if( mptr->flags & M_PUT )	fprintf(outfp, ", Put");
		if( mptr->flags & M_NEW )	fprintf(outfp, ", New");
		if( mptr->flags & M_KEEP )	fprintf(outfp, ", KEEP");
		if( mptr->flags & M_ANSWERED )	fprintf(outfp, ", Answered");
		if( mptr->flags & M_FORWARDED )	fprintf(outfp, ", Forwarded");
		fprintf(outfp, ")\n");
	}

	writmsg();
	mptr->flags &= ~M_NEW;		/* Message seen */
}

/*
 *			L S T B D Y
 *
 * list one message body, maybe separated
 */
lstbdy()
{
	if( lstsep && lstmore)
		fputs( "\f\n", outfp );
	lstmore = TRUE;

	writbdy();
	mptr->flags &= ~M_NEW;		/* Message seen */
}

/*
 *			M O V M S G
 */
movmsg()
{
	putmsg();
	delmsg();
}

/*
 *			P U T M S G
 */
putmsg()
{
	register int len;

	len = strlen( delim1);
	fwrite( delim1, sizeof(char), len, outfp );
	writmsg();
	fwrite( delim2, sizeof(char), len, outfp );
	mptr->flags |= M_PUT;
}

/*
 *			W R I T M S G
 */
writmsg()
{
	long size;
	int count;
	char tmpbuf[512];


	fseek( filefp,( long)( mptr->start), 0);
	for( size = mptr->len; size > 0; size -= count)  {
		if( size <( sizeof tmpbuf))
			count = size;
		else
			count =( sizeof tmpbuf);

		if( fread( tmpbuf, sizeof( char), count, filefp) < count)
			error( "error reading\r\n");
		if( fwrite( tmpbuf, sizeof(char), count, outfp ) < count )
			error( "error writing to file\r\n");
	}
}

/*
 *			W R I T B D Y
 */
writbdy()
{
	long size;
	int srcstat;
	char line[LINESIZE];

	fseek( filefp,( long)( mptr->start), 0);
	size = mptr->len;

	srcstat = SP_HNOSP;
	while( size > 0)  {
		if( xfgets( line, sizeof( line), filefp) == NULL )
			break;
		size -= strlen( line );

		if( srcstat == SP_HNOSP && *line == '\n' ) {
			/* Don't output the separating blank line */
			srcstat = SP_BODY;
			continue;
		}

		if( srcstat == SP_BODY )
			fputs( line, outfp );

	}
}

/*
 *			P R M S G
 */
prmsg()
{
	char line[LINESIZE];
	register long size;
	int	srcstat;

	tt_norm();

	size = mptr->len;
	status.ms_curmsg = msgno;

	printf( "(Message # %d: %ld bytes", msgno, size );
	if( mptr->flags & M_DELETED )	printf(", Deleted");
	if( mptr->flags & M_PUT )	printf(", Put");
	if( mptr->flags & M_NEW )	printf(", New");
	if( mptr->flags & M_KEEP )	printf(", KEEP");
	if( mptr->flags & M_ANSWERED )	printf(", Answered");
	if( mptr->flags & M_FORWARDED )	printf(", Forwarded");
	printf(")\n");
	fflush( stdout );

	if(quicknflag == ON )
		mptr->flags &= ~M_NEW;		/* Message seen */

	fseek( filefp,( long)( mptr->start), 0);

	srcstat = SP_HNOSP;
	while( size > 0)  {
		if( xfgets( line, sizeof( line), filefp) == NULL )
			break;

		if( *line == '\n' )
			srcstat = SP_BODY;
		/* filter obnoxious lines */
		if( keystrip == ON && srcstat != SP_BODY ) {
			if( !(srcstat == SP_HSP &&
			    (*line == '\t' || *line == ' ')))
				if( srchkey( line, keywds) == 0 ) {
					filout( line, stdout );
					srcstat = SP_HNOSP;
				}
				else
					srcstat = SP_HSP;
		}
		else
			filout( line, stdout);

		size -= strlen( line );
	}
	tt_raw();
	mptr->flags &= ~M_NEW;		/* Message seen */
}

/*--------------------------------------------------------------------*/

/*
 *			A N S I T E R
 *
 * Top level for "answer" command.
 */
ansiter()
{
	sndto[0] =
	    sndcc[0] =
	    sndsubj[0] = '\0';
	ansnum = 0;

	doiter( prhdr);
	anstype = ansqry();

	doiter( ansmsg);

	if( ansnum == 0)
		printf( "no messages to answer\r\n");
	else
		if( isnull( sndto[0]))
			printf( "no From field in header\r\n");
		else {
			xeq( 'a');		/* execute an answer command */
			doiter( ansend );	/* Set the A flag */
		}
}

/*
 *			E D A N S I T E R
 *
 * Top level for "answer" command, to drop into EMACS 2-window mode.
 */
edansiter()
{
	register int fd;

	sndto[0] =
	    sndcc[0] =
	    sndsubj[0] = '\0';
	ansnum = 0;

	doiter( prhdr);
	anstype = ansqry();

	doiter( ansmsg);

	if( ansnum == 0)  {
		printf( "no messages to answer\r\n");
		return;
	}
	if( isnull( sndto[0]) )  {
		printf( "no From field in header\r\n");
		return;
	}

	/* Prepare the work files */
	if( (fd = creat(draft_work, sentprotect)) < 0 )  {
		perror( draft_work );
		return;
	}
	close(fd);

	makedrft();

	/* Invoke EDITOR in 2-window mode */
	xeq( 'e' );

	/* Feed results into SEND */
	xeq( 'A' );

	/* Set the A flag */
	doiter( ansend );

	/* Clean up */
	unlink( draft_original );
	/* draft_work left behind in case he wants another look at it */
}

/*
 *			A N S Q R Y
 * who to send answer to
 */
ansqry()
{

again:
	printf( "copies to which original addresses: ");
	nxtchar = echochar();
	nxtchar = uptolow( nxtchar);

	switch( nxtchar)  {

	case '\n':
	case '\r':
		if( verbose)
			printf( "from\r\n");
		return( ANSFROM);

	case 'a':
		if( verbose)
			printf( "ll\r\n");
		return( ANSALL);

	case 'c':
		if( verbose)
			printf( "c'd\r\n");
		return( ANSCC);

	case 'f':
		if( verbose)
			printf( "rom\r\n");
		return( ANSFROM);

	case 't':
		if( verbose)
			printf( "o\r\n");
		return( ANSTO);

	case '?':
		if( verbose)
			printf( "\r\n");
		printf( "all\r\n");
		printf( "cc'd\r\n");
		printf( "from [default]\r\n");
		printf( "to\r\n");
		goto again;

	case '\004':
	default:
		error( " ?\r\n");
	}
	/* NOTREACHED */
}


/*
 *			A N S M S G
 *
 * get create To, & Subject fields
 */
ansmsg()
{
	char tmpfrom[M_BSIZE],
	tmprply[M_BSIZE],
	tmpsender[M_BSIZE],
	tmpto[M_BSIZE],
	tmpcc[M_BSIZE],
	tmpsubj[M_BSIZE];
	register unsigned int ind;
	int llenleft;

	ansnum++;
	status.ms_curmsg = msgno;

	tmpfrom[0] =
	    tmprply[0] =
	    tmpsender[0] =
	    tmpto[0] =
	    tmpcc[0] =
	    tmpsubj[0] = '\0';

	gethead( NODATE, tmpfrom, tmpsender, tmprply,
	(anstype & ANSTO) ? tmpto : NOTO,
	(anstype & ANSCC) ? tmpcc : NOCC, tmpsubj);

	if( !isnull( tmprply[0]))
		/* send to Reply-To */
		sprintf( &sndto[strlen( sndto)], ",%s%c", tmprply, '\0');
	else if( !isnull( tmpfrom[0]))
		/* send to From, if no Reply-To */
		sprintf( &sndto[strlen( sndto)], ",%s%c", tmpfrom, '\0');

	if( !isnull( tmpto[0]))
		sprintf( &sndcc[strlen( sndcc)], ",%s%c", tmpto, '\0');

	if( !isnull( tmpcc[0]))
		sprintf( &sndcc[strlen( sndcc)], ",%s%c", tmpcc, '\0');

	if( !isnull( tmpsubj[0]))  {
		/* save the destination */
		if( equal( "re:", tmpsubj, 3))
			for( ind = 3; isspace( tmpsubj[ind]); ind++);
		else
			if( equal( "reply to:", tmpsubj, 9))
				for( ind = 9; isspace( tmpsubj[ind]); ind++);
			else
				ind = 0;

		if( ansnum  == 1)
			/* not the first message */
			sprintf( sndsubj, "Re:  %s%c", &tmpsubj[ind], '\0');
		else {
			/* append more addresses */
			if( (llenleft = sizeof(sndsubj) - strlen(sndsubj))
			    -strlen(&tmpsubj[ind])-9 > 0 )
				sprintf( &sndsubj[strlen( sndsubj)],
					"\n     %s%c", &tmpsubj[ind], '\0');
			else if( llenleft > 7 )
				sprintf( &sndsubj[strlen( sndsubj)],
				"\n...%c", '\0');
		}
	}
}
/*--------------------------------------------------------------------*/
ansend() {		/* Set the A flag */

	mptr->flags |= M_ANSWERED;
}
/*--------------------------------------------------------------------*/
/*
 *			F W D I T E R
 *
 * Top level for forward command.
 */
fwditer()
{
	char *mktemp();

	status.ms_curmsg = msgno;
	fwdnum = 0;
	sndto[0] =
	    sndcc[0] =
	    sndsubj[0] = '\0';

	doiter( prhdr);

	strcpy( outfile, "/tmp/send.XXXXXX");
	mktemp( outfile);

	autoconfirm = TRUE;
	cpyiter( fwdmsg, DOIT, fwdpost);
	autoconfirm = FALSE;

	xeq( 'f');
	unlink( outfile);
}

/*
 *			F W D M S G
 *
 * copy one forwarded message
 */
fwdmsg()
{
	char line[M_BSIZE];
	int	llenleft;

	fwdnum++;

	line[0] = '\0';		/* build a subject line */
	gethead( NODATE, NOFROM, NOSNDR, NORPLY, NOTO, NOCC, line);

	if( !isnull( line[0]))  {
		/* If we had a subject line, bracket the subject info */
		if( isnull( sndsubj[0]) )
			/* the first subject line */
			sprintf( sndsubj, "[%s:  %s]%c",
				mptr->from, line, '\0');
		else {
			/* not the first line */
			if( (llenleft = sizeof(sndsubj) - strlen(sndsubj))
			    -strlen(line)-SIZEFROM-10 > 0 )
				sprintf( &sndsubj[strlen( sndsubj)],
				"\n[%s:  %s]%c", mptr->from, line, '\0');
			else if( llenleft > 7 )
				sprintf( &sndsubj[strlen( sndsubj)],
				"\n...%c", '\0');

		}
	}

	fprintf( outfp, "\n----- Forwarded message # %d:\n\n", fwdnum);

	writmsg();

	mptr->flags |= M_FORWARDED;
}

/*
 *			F W D P O S T
 */
fwdpost()
{
	fprintf( outfp, "\n----- End of forwarded messages\n" );
}
/*
 *			S R C H K E Y
 */
srchkey( line, keypt)
	char *line, *keypt[];
{
	register int	n;
	register char	*pkey, *pline;

	for( n = 0; keypt[n] != 0; n++ ) {
		pkey = keypt[n];
		pline = line;

		while( *pkey != '\0' ) {

			if( *pline != *pkey && *pline - 'A' + 'a' != *pkey )
				goto trynext;
			pline++;
			pkey++;
		}
		return(1);
trynext: ;
	}
	return(0);
}
/* ------------------------------------------------------------------ 
 *			F I L O U T
 *
 *	Filter most control chars from text - Prevents letter bombs
 *	Also does paging
 *	****** linecount must be initialized before calling filout ******
 */
filout(line, ofp)
	char *line;
	FILE *ofp;
{
	register int cmd;

	if( paging ) {
		linecount += (strlen( line ) / linelength ) + 1;

		if( doctrlel && strindex( "\014", line) >= 0) {
			/* Got formfeed */
			tt_raw();
			if( !confirm("Formfeed. Continue?",NOLF))
				error("");
			tt_norm();
			linecount = 0;
		}

		if ( linecount >= pagesize - 2) {
			tt_raw();
			if( (cmd = confirm("Continue?",NOLF)) == FALSE )
				error("");
			tt_norm();
			switch( cmd ) {

			case '\n':		/* One more line */
				linecount = pagesize;
				break;

			default:
				linecount = 0;
				break;
			}
		}
	}

	if( filoutflag == OFF ) {
		fputs( line, ofp );
		return;
	}

	for( ; *line != '\000'; line++ ) {
		if( *line >= ' ' && *line <= '~' )
			putc( *line, ofp );
		else {
			switch ( *line ) {

			case '\007':	/* Bel */
			case '\t':
			case '\n':
			case '\r':
			case '\b':
				putc( *line, ofp );
				break;

			default:
				putc( '^', ofp );
				putc( *line + '@', ofp );
				break;
			}
		}
	}
}
/*
 *  			M A K E D R F T
 *  Make a file containing one or more messages
 */
makedrft() {

	register int fd;

	if( (fd = creat(draft_original, sentprotect)) < 0 )  {
		perror( draft_original );
		return;
	}
	close( fd );

	/* LIST messages into the draft_original file */
	lstsep = TRUE;
	strcpy( outfile, draft_original );
	cpyiter( lstmsg, DOIT, (int(*)()) 0 );

}
