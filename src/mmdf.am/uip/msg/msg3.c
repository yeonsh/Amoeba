/*	@(#)msg3.c	1.1	91/11/21 11:43:40 */
/*
 *			M S G 3 . C
 *
 * This is the third part of the MSG program.
 *
 * Functions -
 *	xeq		run a sub-process
 *	help		supply help
 *	gitr		get message range
 *	gitrtype	get message range type details
 *	gitrnum		get message range -- numeric
 *	unset		clear all "Process it" flags
 *	cntproc		count number of messages to be processed
 *	setrange	mark group of messages to process
 *	settype		mark all messages according to type command
 *	getnum		input a decimal number
 *	doiter		apply a function to all marked messages
 *	resend		resend message to addresses
 *	sortbox		sort msg box
 *	qcomp		qsort comparison routine
 *
 *		R E V I S I O N   H I S T O R Y
 *
 *	06/09/82  MJM	Split the enormous MSG program into pieces.
 *
 *	03/16/83  MJM	Decoupled "inverse" from "all".
 */
#include "util.h"
#include "mmdf.h"
#include "pwd.h"
#include "signal.h"
#include "sys/stat.h"
#include "sgtty.h"
#ifdef V4_2BSD
#include "sys/wait.h"
#endif V4_2BSD
#include "./msg.h"

/*
 *			X E Q
 *
 *  Execute some program, such as SEND or the Shell.
 */
sigtype (*old2)();
sigtype (*old3)();
sigtype (*old13)();
sigtype (*old18)();

xeq( exflag)
char exflag;
{
	int process;
/*#ifdef V4_2BSD
	union wait pstatus;
#else V4_2BSD*/
	int	pstatus;
/*#endif V4_2BSD*/
	char *routine;

	tt_norm();
	old2 = signal( SIGINT, SIG_IGN);
	old13 = signal( SIGPIPE, SIG_IGN);
#ifdef SIGTSTP
	old18 =	signal( SIGTSTP, SIG_DFL);
#endif SIGTSTP

	process = fork();
	if( !process)  {
		int	fd;

		signal( SIGINT, orig);
		signal( SIGQUIT, old3);
		signal( SIGPIPE, old13);

#ifndef V4_2BSD
		for (fd = _NFILE-1; fd > 2; fd--)
#else
		for (fd = getdtablesize()-1; fd > 2; fd--)
#endif
			close (fd);

		switch( exflag)  {

		case 'a': 		  /* answer send fork */
			execlp( routine = sndname, sndname, sndto, "-c", sndcc,
				"-s", sndsubj, (char *)0);
			break;

		case 'A':
			/* Invoke SEND after 2-window edit */
			execlp( routine = sndname, sndname, sndto, "-c", sndcc,
				"-s", sndsubj, "-f", draft_work, "-n", (char *)0);
			break;

		case 'd': 
			execlp( routine = "date", "date", (char *)0);
			break;

		case 'e':		/* 2-window edit */
			execlp( routine = twowinfil, ueditor,
				draft_original, draft_work, (char *)0 );
			break;

		case 'f': 		  /* forward send fork */
			execlp( routine = sndname, sndname, "-f", outfile,
			  "-c", (isnull( sndsubj[0]) ? (char *)0 : "-s"),
			  sndsubj, (char *)0);
			break;

		case 'j': 		  /* One command shell fork */
			if( verbose)  {
				printf( "jump into lower shell running: ");
				fflush(stdout);
			}
			execlp( routine = ushell, ushell, "-t", (char *)0);
			break;

		case 's':		/* shell fork */
			execlp( routine = ushell, ushell, (char *)0);
			break;

		case '!':		/* shell fork */
			execlp( routine = ushell, ushell, "-c", key, (char *)0 );
			break;

		case 'S': 		  /* send fork */
			execlp( routine = sndname, sndname, (char *)0);
			break;
		}
		fprintf (stderr, "Can't execute %s\r\n", routine);
		closeup(-1);
	}  else  {
		while (wait( &pstatus ) != -1) ;

		signal( SIGINT, old2);
		signal( SIGQUIT, SIG_IGN);
		signal( SIGPIPE, old13);
#ifdef SIGTSTP
		signal( SIGTSTP, old18);
#endif SIGTSTP
		tt_init();		/* in case he changed the ttymodes */
	}
	tt_raw();
}

/*--------------------------------------------------------------------*/

char *cmdlst[] = {
	"",
	"Command Summary -- Type only the first letter of a command",
	"",
	"MESSAGE HANDLING:			    NAVIGATION:",
	"Answer [message #(s)]			    Backup to prev msg & type it",
	"Forward [message #(s)]			    Next message & type it",
	"Send (compose) a new message		    Headers of [message #(s)]",
	"Type [message #(s)] onto terminal	    Go to message #",
	"Delete [message #(s)]			    Current message # is typed",
	"Undelete [message #(s)]",		
	"Keep [message #(s)]                        /-same as dcn",
	"Y-Resend [message #(s)]",
	"Z-Two window answer",
	"",
	"FILE HANDLING:				    MISCELLANEOUS:",
	"Exit and update -- normal exit		    Jump into sub-Shell",
	"List [message #(s)] into text file	    Xtra user options",
	"Overwrite old file			    : current date and time",
	"Put [message #(s)] into msg file	    ; ignore rest of line",
	"Quit -- fast exit			    @ Undigestify",
	"Read another msg file			    ! sub-Shell",
	"Move [message #(s)] into msg file & mark as deleted",
	"",
	"Examples of message #(s) are:  42  1-4  2,5,7-.  1-5,7-@  c  32-$",
	0
};

char *xtcmdlst[] = {
	"",
	"xtra options command summary -- Type only the first letter of a command",
	"",
	"Binary file write",
	"Control char filter (on/off)",
	"Exit without saves",
	"List message body",
	"Mark message",
	"Numbered message lists (on/off)",
	"Output mbox file",
	"Paging (on/off)",
	"Reorder (sort) msg file",
	"Strip keywords (on/off)",
	"Verbose (on/off)",
	"Xtra options status",
	0
};

/*
 *			H E L P
 */
help(helplist)
char *helplist[];
{
	register unsigned int i;

	tt_norm();
	for( i = 0; helplist[i] != 0; i++)
		printf( "%s\r\n", helplist[i]);
	tt_raw();
}

/*--------------------------------------------------------------------*/

/*
 *			G I T R
 *
 *  Get message range.
 */
gitr()
{
	if( status.ms_nmsgs == 0)  {
		printf( "'%s' is empty\r\n", filename);
		error( "" );
	}

	unset();

	if( !gitrtype())
		gitrnum();

	if( cntproc() == 0)
		error( " no such messages\r\n");
}

/*
 *			G I T R T Y P E
 *
 * Get range type.  Alphabetic answers are processed here,
 * numerics are left alone (FALSE return).
 */
gitrtype()
{
	char cmd;

	cmd = nxtchar;
	nxtchar = rdnxtfld();
	nxtchar = uptolow(nxtchar);

	if( cmd == 'g' )  {
		/* Only numerics valid for GOTO command */
		if( isalpha( nxtchar) && nxtchar != 'c' && nxtchar != 'm')
			error( "...not legal for this command\r\n");
	}

	switch( nxtchar)  {
	case 'a': 
		if( verbose)
			printf( "all");
		if( cmd != 'h' ) {
			if( !confirm((char *)0,DOLF) )
				error("");
		}
		else if (verbose)
			printf("\r\n");
		setrange( 1, status.ms_nmsgs);
		break;

	case 'c': 
		if( verbose)
			printf( "current\r\n");
		if( status.ms_curmsg == 0)  {
			if( status.ms_nmsgs != 0)
				status.ms_curmsg = 1;
			else
				if( status.ms_curmsg > status.ms_nmsgs)
					error( "no current message\r\n");
		}
		setrange( status.ms_curmsg, status.ms_curmsg);
		break;

	case 'd': 
		if( verbose)
			printf( "deleted messages\r\n");
		settype( 'd');
		if( cntproc() == 0)
			error( "no deleted messages\r\n");
		break;

	case 'e': 
		if( verbose)
			printf( "expression: ");
		key[0] = rdnxtfld();
		gather( key, 79);
		if (key[0])
			settype( 'e');
		break;

	case 'f': 
		if( verbose)
			printf( "from: ");
		key[0] = rdnxtfld();
		gather( key, 79);
		if (key[0])
			settype( 'f');
		break;

	case 'i': 
		if( verbose)
			printf( "inverse ");
		ascending = FALSE;	/* flag for doiter() */

		if( !gitrtype() )	/* RECURSE */
			gitrnum();
		return( TRUE );

	case 'k':
		if( verbose )
			printf( "kept messages" );

		if( cmd == 'd' ) {
			if( !confirm((char *)0,DOLF) )
				error("");
		}
		else if (verbose)
			printf("\r\n");

		settype( 'k' );
		break;

	case 'l':
		if( verbose)
			printf( "leaving" );

		if( !ismainbox )
			error( "\r\nNot main mailbox - no messages will leave\r\n");

		if( cmd == 'd' ) {
			if( !confirm((char *)0,DOLF) )
				error("");
		}
		else if (verbose)
			printf("\r\n");

		settype( 'l' );
		break;

	case 'm':
		if(verbose)
			printf( "mark\r\n");
		if( status.ms_markno > 0 && status.ms_markno <= status.ms_nmsgs )
			setrange(status.ms_markno, status.ms_markno);
		else
			error("no mark set\r\n");
		break;

	case 'n':
		if( verbose)
			printf( "new");

		if( cmd == 'd' ) {
			if( !confirm((char *)0,DOLF) )
				error("");
		}
		else if (verbose)
			printf("\r\n");

		settype( 'n' );
		break;

	case 's': 
		if( verbose)
			printf( "subject: ");
		key[0] = rdnxtfld();
		gather( key, 79);
		if (key[0])
			settype( 's');
		break;

	case 't': 
		if( verbose)
			printf( "to: ");
		key[0] = rdnxtfld();
		gather( key, 79);
		if (key[0])
			settype( 't');
		break;

	case 'u': 
		if( verbose)
			printf( "undeleted messages");

		if( cmd == 'd' ) {
			if( !confirm((char *)0,DOLF) )
				error("");
		}
		else if (verbose)
			printf("\r\n");

		settype( 'u');
		if( cntproc() == 0)
			error( "no undeleted messages\r\n");
		break;

	case '?':
		printf( "\r\na message sequence:\r\n");
		printf( " a(ll), c(urrent), d(eleted), e(xpression), f(rom), l(eaving),\r\n");
		printf( " k(ept), m(ark), n(new), i(inverse), s(ubject), t(o), u(ndeleted)\r\n");
		printf( " or a number or list of numbers of the form\r\n");
		printf( " (#  #-#  #,#,#,#  $ .-$)  .=current @=mark $=last\r\n" );
		error( "" );

	case '\003':
		error(" ^C\r\n");
		
	case '\004':
		error(" ^D\r\n");

	case '\n': 
		if( status.ms_curmsg == 0)  {
			if( status.ms_nmsgs != 0)
				status.ms_curmsg = 1;
			else
				if( status.ms_curmsg > status.ms_nmsgs)
					error( "no current message\r\n");
		}
		if( cmd == 'g' && status.ms_markno > 0 && status.ms_markno <= status.ms_nmsgs )
			status.ms_curmsg = status.ms_markno;
		setrange( status.ms_curmsg, status.ms_curmsg);
		switch( cmd)  {

		case 'a': 
		case 'f': 
		case 'h': 
		case 't': 
		case 'y':
		case 'z':
			if (verbose) printf("\r\n");
			break;	  /* message number is printed in header */

		default: 
			/* Print brief header */
			printf( "\r\n # %d  %s\r\n", status.ms_curmsg,
				msgp[status.ms_curmsg-1]->from);
		}			  /* DROP ON THROUGH */
		break;

	default: 
		return( FALSE);
	}
	if (!verbose) suckup();
	return( TRUE);
}

/*
 *			G I T R N U M
 *
 * Get range -- numeric case
 */
gitrnum()
{
	unsigned int getn;
	unsigned int mbegin, mend;

	key[0] = nxtchar;
	gather( key, 78 );
	gc = key;

	while( *gc != '\000' )  {
		switch( *gc )  {

		case ' ':
		case ',':
			gc++;
		}

		if( *gc == '\000')
			break;

		getn = getnum( &mbegin);
		mend = mbegin;

		switch( *gc )  {

		case '\000': 
		case ' ': 
		case ',': 
			if( mbegin == 0 || mbegin > status.ms_nmsgs)
				error( "bad number (type ? for help)\r\n");
			break;

		case '>': 
		case '-': 		  /* ;2 Replace range delimiters */
		case ':': 		  /* ;2 Replace range delimiters */
			if( getn == 0)
				mbegin = 1;
			if( mbegin == 0 || mbegin > status.ms_nmsgs)
				error( " bad first number (type ? for help)\r\n");

			gc++;
			getn = getnum( &mend);

			if( getn == 0)
				mend = status.ms_nmsgs;
			else
				if( mend > status.ms_nmsgs)  {
					printf( "... %d is last message\r\n", status.ms_nmsgs);
					mend = status.ms_nmsgs;
				}
			if( mend == 0)
				error( "bad last number (type ? for help)\r\n");
			break;

		default: 
			error( " bad message number (type ? for help)\r\n");
		}
		setrange( mbegin, mend);
	}
}

/*--------------------------------------------------------------------*/

/*
 *			U N S E T
 *
 * Clear all the M_PROCESS_IT flags.  Used prior to setting ranges, etc.
 */
unset()
{
	register struct message **mp;

	/* set all the flags to zero */
	for( mp = &msgp[status.ms_nmsgs]; mp-- != &msgp[0]; )
		(*mp)->flags &= ~M_PROCESS_IT;
}

/*
 *			C N T P R O C
 *
 * Count how many messages to process.
 */
cntproc()
{
	register unsigned int nproc;
	register struct message **mp;

	nproc = 0;
	for( mp = &msgp[status.ms_nmsgs]; mp-- != &msgp[0]; )
		if( (*mp)->flags & M_PROCESS_IT)
			nproc++;
	return( nproc);
}

/*
 *			S E T R A N G E 
 *
 * set group of messages to process
 */
setrange( mbegin, mend )
register unsigned int mbegin;
register unsigned int mend;
{
	if( mbegin == 0 )
		mbegin = 1;
	if( mend > Nmsgs )
		mend = Nmsgs;

	while( mend-- >= mbegin)
		msgp[mend]->flags |= M_PROCESS_IT;
}

/*
 *			S E T T Y P E
 */
settype( type )
char type;
{
	register struct message **mp;
	register unsigned int i;

	switch( type )  {

	case 'd':
		for( mp = &msgp[status.ms_nmsgs]; mp-- != &msgp[0]; )
			if( (*mp)->flags & M_DELETED)
				(*mp)->flags |= M_PROCESS_IT;
		break;

	case 'n':
		for( mp = &msgp[status.ms_nmsgs]; mp-- != &msgp[0]; )
			if( (*mp)->flags & M_NEW)
				(*mp)->flags |= M_PROCESS_IT;
		break;

	case 'k':
		for( mp = &msgp[status.ms_nmsgs]; mp-- != &msgp[0]; )
			if( (*mp)->flags & M_KEEP)
				(*mp)->flags |= M_PROCESS_IT;
		break;

	case 'u':
		for( i = status.ms_nmsgs; i-- != 0;)
			if( !(msgp[i]->flags & M_DELETED))
				msgp[i]->flags |= M_PROCESS_IT;
		break;

	case 's':
		for( i = status.ms_nmsgs; i-- != 0; )
			if( strindex( key, msgp[i]->subject) >= 0)
				msgp[i]->flags |= M_PROCESS_IT;
		break;

	case 'f':
		for( i = status.ms_nmsgs; i-- != 0; )
			if( prefix( msgp[i]->from, "To:"))  {
				if( prefix( key, username))
					msgp[i]->flags |= M_PROCESS_IT;
			}  else  {
				if( strindex( key, msgp[i]->from) >= 0)
					msgp[i]->flags |= M_PROCESS_IT;
			}
		break;

	case 't':
		for( i = status.ms_nmsgs; i-- != 0; )
			if( strindex( key, msgp[i]->to ) >= 0 )
				msgp[i]->flags |= M_PROCESS_IT;
		break;

	case 'e':
		tt_norm();
		for( msgno = 0; msgno < status.ms_nmsgs; msgno++)
			if(txtmtch())
				msgp[msgno]->flags |= M_PROCESS_IT;
		tt_raw();
		break;

	case 'l':
		tt_norm();
		for( mp = &msgp[status.ms_nmsgs]; mp-- != &msgp[0]; )
			if(    !((*mp)->flags & M_KEEP)
			    && !((*mp)->flags & M_DELETED)
			    && !((*mp)->flags & M_NEW))
				(*mp)->flags |= M_PROCESS_IT;
		tt_raw();
		break;

	default:
		error("bad settype() call");
	}
}

/*
 *			G E T N U M
 */
getnum( result)
unsigned int *result;
{
	register unsigned int i;
	register unsigned int n;

	i = n = 0;
	while( *gc == ',' || *gc == '-' || *gc == '>' || *gc == ':' )
		gc++;
	if( *gc == '$')  {
		*result = status.ms_nmsgs;
		gc++;
		return( TRUE);
	} else if ( *gc == '.') {
		*result = status.ms_curmsg;
		gc++;
		return( TRUE );
	} else if ( *gc == '@' ) {
		if( status.ms_markno > 0 && status.ms_markno <= status.ms_nmsgs )
			*result = status.ms_markno;
		else
			error("no mark set\r\n");
		gc++;
		return( TRUE );
	}
	while( isdigit( *gc ) )  {
		n = n * 10 + *gc - '0';
		i++;
		gc++;
	}

	*result = n;
	return( i);
}

/*--------------------------------------------------------------------*/

/*
 *			D O I T E R
 *
 * do iteration on message numbers
 */
doiter( fn)
int ( *fn)();
{
	unsigned int lastdone;

	if( status.ms_nmsgs == 0)
		return;
	tt_norm();
	lastdone = msgno;
	if( ascending == TRUE )  {
		/* process in ascending order */
		for( msgno = 1; msgno <= status.ms_nmsgs; msgno++)
			if( (mptr = msgp[msgno-1])->flags & M_PROCESS_IT)  {
				(*fn)();
				lastdone = msgno;
			}
	}  else  {
		/* Process in inverse (descending) order */
		for( msgno = status.ms_nmsgs; msgno > 0; msgno-- )
			if( (mptr = msgp[msgno-1])->flags & M_PROCESS_IT)  {
				(*fn)();
				lastdone = msgno;
			}
	}
	tt_raw();
	msgno = lastdone;
}

/*-----------------------------------------------------------------------
 *  			R E S E N D M S G
 *  
 *  Resend message to addressees
 */
resendmsg() {

	register int savepretty;

	if( verbose )
		putchar('.');
	lstsep = FALSE;
	lstmore = FALSE;
	savepretty = prettylist;
	prettylist = OFF;
	cpyiter( lstmsg, NOIT,( int( *)()) 0);
	prettylist = savepretty;
	mptr->flags |= M_FORWARDED;
}
/*			S O R T B O X
 *  
 */
sortbox() {

	register int i,j,k;
	struct message *mpt;
	char *pi, *pj;

	if( status.ms_nmsgs == 0)  {
		printf( "'%s' is empty\r\n", filename);
		error( "" );
	}

	nxtchar = rdnxtfld();
	nxtchar = uptolow(nxtchar);
	switch( nxtchar)  {

	case 'd':
		if( verbose )
			printf("Date\r\n");
		break;

	case 'f': 
		if( verbose)
			printf("From\r\n");
		break;

	case 'r':
		if( verbose )
			printf("date and subject\r\n");
		fflush(stdout);
		/* sort by date first */
		nxtchar = 'd';
		qsort( &msgp[0], status.ms_nmsgs, sizeof(msgp[0]), qcomp );

		/* Match subjects */
		for( i = 0; i < status.ms_nmsgs-1; i++ ) {
			if( msgp[i]->subject[0] == '\0' )
				continue;
			for( pi = msgp[i]->subject; prefix( pi, "Re:" ) == TRUE; )
				pi = strend("Re:",pi);

			for( j = i+1; j < status.ms_nmsgs; j++ ) {
				if( msgp[j]->subject[0] == '\0' )
					continue;
				for( pj = msgp[j]->subject; prefix( pj, "Re:" ) == TRUE; )
					pj = strend("Re:",pj);

				if( lexnequ( pi, pj, SIZESUBJ ) != TRUE )
					continue;

				/* Match - move rest down a slot */
				if( j == i+1 ) {	/* No need to move */
					i++;
					continue;
				}
				mpt = msgp[j];
				for( k = j-1; k > i; k-- )
					msgp[k+1] = msgp[k];
				msgp[i+1] = mpt;
				i++;
			}
		}
		return;

	case 's': 
		if( verbose)
			printf( "Subject\r\n");
		break;

	case 't':
		if( verbose )
			printf("To\r\n");
		break;

	case '?':
		printf( "\r\nd(ate), f(rom), r( date & subject), s(ubject), t(o)\r\n");
		error( "" );

	case '\004':
		error( " ^D\r\n" );

	default:
		error( "" );
	}

	fflush(stdout);
	qsort( &msgp[0], status.ms_nmsgs, sizeof(msgp[0]), qcomp);
}

/*			Q C O M P
 *  
 *  Comparison routine called by qsort
 */
qcomp(pa,pb)
struct message **pa, **pb;
{
	int sval;
	char *cpa, *cpb;
	
	switch( nxtchar ) {

	case 'd':
		return(datecmp((*pa)->date, (*pb)->date));

	case 'f': 
		return(lexrel( (*pa)->from, (*pb)->from ));

	case 's': 
		/* Remove all Re: */
		for( cpa = (*pa)->subject; prefix( cpa, "Re:" ) == TRUE; )
			cpa = strend("Re:",cpa);
		for( cpb = (*pb)->subject; prefix( cpb, "Re:" ) == TRUE; )
			cpb = strend("Re:",cpb);
		if( (sval = lexrel( cpa, cpb )) == 0 )
			return(datecmp((*pa)->date, (*pb)->date));
		else
			return(sval);

	case 't':
		return( lexrel( (*pa)->to, (*pb)->to ) );

	default:
		error( "Bad type in qcomp" );
	}
	/*NOTREACHED*/
}

datecmp(a, b)
long a, b;
{
	if ((a - b) < 0)
		return (-1);
	if ((a - b) > 0)
		return (1);
	return(0);
}
