/*	@(#)msg1.c	1.2	96/02/27 09:58:51 */
/*
 *			M S G 1 . C
 *
 * Functions -
 *	main		main command loop
 *
 *
 *  This has wended its way from Illinois through many other sites.
 *  It approximates the Tenex MSG program, written by John Vittal.
 *
 *  Oct 80  Stu Cracraft	Moby edit for conversion from V6 to V7
 *
 *  Nov 80  Dave Crocker	Retrofit for V6 compatibility; add stdio
 *  -May 81			cleanup & improve error detect/correct
 *
 *  Jun 81  Dave Crocker	gitr() fixed '?' case
 *				overwrit() confirm file deletion and
 *				have temp file in same directory as original
 *				xeq() call to date needed 0 at end of args
 *			 	change 'index' to 'equal' for reply tests
 *
 *  Jul 81  Dave Crocker	stop mapping DEL->newline in ttychar()
 *				fix settype(t) to make txtmtch work
 *			 	add 'new' messages added after folder open
 *
 *  Jul 81  Doug Kingston	getnum() check for < 0 -> 0.
 *  Sep 81  Dave Crocker	prmsg() catch form-feeds
 *  Jan 82  Doug Kingston	Modified for use on BRL machines
 *				Threw out some deadwood.
 *  Mar 82  Doug Kingston	Fixed blank line bug in setup().
 *  Jun 82  Doug Kingston	Fixed the "type current" bug.
 *
 *	06/07/82  MJM		First pass on cleanup prior to changover
 *				to dual file version.
 *
 *	06/08/82  MJM	Split the enormous MSG program into pieces.
 *			Added new help messages.
 *
 *	06/10/82  MJM	Optimized file reading, header parsing, eliminated
 *			re-read after overwrite, added additional flags
 *			processing, 'k', '$', ',' commands, plus general
 *			cleanup and optimization.
 *
 *	07/12/81  MJM	Fixed up bugs in Quit signal processing, and in the
 *			',' command.
 *
 *	09/10/82  MJM	Converted output to use STDIO also.
 *
 *	09/13/82  MJM	Added Undigestify code.  Switched msg[] to msgp[].
 *
 *	11/22/82  HW	Added Keyword stripping and Xtra options.
 *
 *	12/04/82  HW	Added P flag, dot options.
 *
 *	03/16/83  MJM	VAXorcised the code.
 *
 *	03/29/83  MJM	Made eXtend a command, not a mode.  Added "d" debug.
 *
 *	08/11/83  DPK	Fixed interrupt handling, added "." in number ranges,
 *			fixed continuation line handling, added close of files
 *			prior to execs, added wait loop.
 *
 *      11/01/85       Craig Partridge: fix bugs in using spool directory
 *
 *	04/20/88  ECB	Added "twowinfil" option.
 */

extern char *verdate;

#include "util.h"
#include "mmdf.h"
#include "pwd.h"
#include "signal.h"
#include "sys/stat.h"
#include "sgtty.h"
#ifdef V4_2BSD
#include "sys/ioctl.h"
#endif V4_2BSD
#include "./msg.h"

extern	int Nmsgs;

char    lastc = '\n';
char	ascending = TRUE;

extern	sigtype	(*old3)();
extern	sigtype	(*old13)();
extern	sigtype	(*old18)();

extern	int	pagesize;
extern	int	linelength;

extern	int	paging;
extern	int	keystrip;
extern	int	bdots;
extern	int	mdots;
extern	int	bprint;
extern	int	quicknflag;
extern	int	prettylist;
extern	int	filoutflag;
extern	int	binsavflag;
extern	char	*resendprog;
extern	char    *savmsgfn;
extern	char	*dflshell;
extern	char	*dfleditor;
extern	char	*verdate;

int	outmem = FALSE;
int	readonly = FALSE;
char	*msgrcfn = ".msgrc";
char	*binarypre = "._";	/* Prefix for binary map filename */

FILE * filefp = NULL;		/* Input FILEp */
FILE * outfp = NULL;		/* Output FILEp */
FILE * binfp = NULL;		/* Binary FILEp */

/*
 * Place to put temporary message file, while doing
 * the overwrite in current directory
 */
char	*tempname = "msgtmp.XXXXXX";

/* The HELP command lists are defined in msg3.c */
extern char	*cmdlst[];
extern char	*xtcmdlst[];
extern char	*ctime();

/*
 *			M A I N
 */
main( argc, argv)
int     argc;
char   *argv[];
{
	static int     restrt;
	int	j;
	char    tb[1024];
	register struct message **mp;
	extern struct passwd *getpwuid();
	extern int onhangup();
	struct passwd  *pwdptr;
	char *uterm;
	int realid;
	int effecid;
	char *tmpname;
	extern char *getlogin();
	extern char *getenv();
#ifdef TIOCGWINSZ
	struct winsize ws;
#endif

	setbuf( stdout, ttyobuf);
	signal( SIGHUP, onhangup );
	orig = signal( SIGINT, SIG_DFL);
	old3 = signal( SIGQUIT, SIG_IGN);
	old13 = signal( SIGPIPE, onnopipe);
#ifdef SIGTSTP
	if( (old18 = signal(SIGTSTP,SIG_IGN)) == SIG_DFL )
		signal(SIGTSTP,onstop);
#endif SIGTSTP

	printf( "MSG (%s)  Type ? for help.\r\n", verdate);
	fflush( stdout);

	mmdf_init("msg");		/* Should be mmdf_init() ??? */

	homedir = getenv("HOME");
	tmpname = getlogin();

	if ((tmpname == 0) || (homedir == 0))
	{
	    getwho( &realid, &effecid);

	    /* get passwd entry */
	    if ((pwdptr = getpwuid(realid)) == 0)
	    {
		if (tmpname == 0)
		    tmpname = "UnknownUsername";

		if (homedir == 0)
		    homedir = "";
	    }
	    else
	    {
		if (tmpname == 0)
		    tmpname = pwdptr->pw_name;

		if (homedir == 0)
		    homedir = pwdptr->pw_dir;
	    }
	}

	strcpy(username,tmpname);

	sprintf( maininbox, "/%s",mldflfil);

	/* Build default mailbox name */
	strcpy( filename, maininbox);

	sprintf( defmbox, "%s/%s", homedir, savmsgfn);
	strcpy( defoutfile, defmbox);

	strcpy( draftorig, "draft_original" );
	/* Build strings for 2-window mode */
	sprintf( draft_work, "%s/draft_work", homedir );
	sprintf( draft_original, "%s/%s", homedir, draftorig );

	/* Get environment variables */
	if( (ushell = getenv("SHELL")) == NULL )
		ushell = dflshell;
	if( (ueditor = getenv("EDITOR")) == NULL )
		ueditor = dfleditor;

	/* The default 2-window filter is just the editor */
	strcpy( twowinfil, ueditor);

#ifdef TIOCGWINSZ
	if (ioctl(fileno(stdout), TIOCGWINSZ, &ws) != -1) {
	    if (ws.ws_row > 0) pagesize = (int)ws.ws_row;
	    if (ws.ws_col > 0) linelength = (int)ws.ws_col;
	} else
#endif TIOCGWINSZ	
	if( (uterm = getenv("TERM")) != NULL && tgetent(tb,uterm) == 1) {
		if ((j = tgetnum("li")) > 0)
			pagesize = j;
		if ((j = tgetnum("co")) > 0)
			linelength = j;
	}

	/* Check for user option file */
	sprintf( msgrcname, "%s/%s", homedir, msgrcfn);
	if( (fpmsgrc = fopen(msgrcname,"r")) != NULL ) {
		xomsgrc( fpmsgrc );
		fclose( fpmsgrc );
	}
	restrt = 0;

	if( argc > 1)
		strcpy( filename, argv[1] );
	if( argc > 2)
		setmbox( argv[2] );
		
	tt_init();		/* get tty modes */
	setjmp( savej );
	signal (SIGINT, onint);

	/*
	 * Initial loading of messages
	 */
	if( !(restrt++))  {
		status.ms_nmsgs = 0;
		status.ms_curmsg = 0;
		status.ms_markno = 0;
		setup( SETREAD );
	}

	if( nottty || argc == 0 || argv[0][0] == 'r' )
		verbose = OFF;
	tt_raw();		/* SET tty modes */

	setjmp( savej);
	autoconfirm = FALSE;
	for(;;)  {
		if( outmem == FALSE )
			newmessage();	/* anything arrived recently? */
		printf( "<- ");
		fflush( stdout);
		unset();		/* turn off M_PROCESS_IT bit */
		ascending = TRUE;
		linecount = 0;

ignore:		nxtchar = ttychar();
		nxtchar = uptolow( nxtchar);

		/* *** KEEP cmdlst[] UP TO DATE WITH LIST OF COMMANDS *** */
		/* It can be found down by the HELP function */
		switch( nxtchar)  {

		case 000:	/* null, see also 'X' 'M' */
			if( verbose )
				printf( "mark set\r\n" );
			status.ms_markno = status.ms_curmsg;
			break;

		case 023:	/* ctl/s */
		case 021:	/* ctl/q */
			goto ignore;

		case 'a': 
			if( verbose)
				printf( "answer message: ");
			gitr();
			if( wmsgflag == ON )
				makedrft();
			ansiter();
			break;

		case 002:		/* ^B */
		case 'b': 
			if( verbose)
				printf( "backup -- previous was:\r\n");
			else
				suckup();
			if( status.ms_curmsg <= 1)
				error( "no prior message\r\n");
			msgno = --status.ms_curmsg;
			mptr = msgp[status.ms_curmsg - 1];
			if( mptr->flags & M_DELETED)
				printf( "# %d deleted\r\n", msgno);
			else
				prmsg();
			break;

		case 'c': 
			if( verbose)
				printf( "current ");
			printf( "message is %d of %d in %s.  Mark at message %d\r\n",
				status.ms_curmsg, status.ms_nmsgs, filename, status.ms_markno);
			break;

		case 'd': 
			if( verbose)
				printf( "delete ");
			if( readonly == TRUE ) {
				printf("in READONLY mode - ignored\r\n");
				break;
			}
			gitr();
			doiter( delmsg);
			if ((j = cntproc()) > 1)
				printf("%d messages deleted\r\n", j);
			break;

		case 'e': 
			if( readonly == TRUE ) {
				if( verbose )
					printf( " quit - READONLY mode" );
			}
			else
				if( verbose)  {
					printf( "exit and update %s", filename);
					if( ismainbox )
						printf( " into %s", defmbox);
					printf( "\r\n"  );
				}
			if( !confirm((char *)0,DOLF))
				break;
			if( readonly == TRUE ) {
				tt_norm();
				closeup(0);
			}

			newmessage();		/* Check for new ones */

			/* If the message hasn't fully arrived, inform the
			 * user, and return to command level.  We depend
			 * on a subsequent newmessage() to pick up the
			 * new message.
			 */
			if(
				stat( filename, &statb2) >= 0 &&
				statb2.st_mtime != status.ms_time
			)  {
				printf( "'%s' updated since last read ...\r\n",
					filename);
				break;
			}
			fflush( stdout);

			/*
			 * Special processing for main mailbox
			 */
			if( ismainbox )  {
				autoconfirm = FALSE;
				strcpy( outfile, defmbox);

				/* tag all undeleted messages for action */
				unset();
				settype( 'u');

				/*
				 * un-tag all KEEP messages
				 * Also, don't flush out messages which
				 * have just arrived and haven't been
				 * seen by the user yet.
				 */
				for( mp = &msgp[0]; mp < &msgp[status.ms_nmsgs]; mp++ )  {
					if( (*mp)->flags & (M_KEEP) )
						(*mp)->flags &= ~M_PROCESS_IT;
					if( (*mp)->flags & (M_NEW) ) {

						(*mp)->flags &= ~M_PROCESS_IT;
					}
				}

				/* MOVE all un-deleted, un-kept messages
				 * into the Mbox file.
				 */

				if( (j = cntproc()) > 0) {
					printf("Moving %d undeleted messages to %s\r\n", j, outfile);
					cpyiter( movmsg, DOIT,( int( *)()) 0);
					/* Delete all msgs moved into Mbox */
					doiter( delmsg);
				}
			}

			/*
			 * Overwrite mailbox.  Note that if some of the
			 * messages were marked as "keep", they will be
			 * retained in the mailbox, all by themselves.
			 */
			overwrit();
			binbuild();

			tt_norm();
			closeup(0);

		case 'f': 
			if( verbose)
				printf( "forward ");
			gitr();
			fwditer();
			break;

		case 'g': 
			if( verbose)
				printf( "go to message number: ");
			gitr();
			doiter( gomsg);
			break;

		case 'h': 
			if( verbose)
				printf( "headers ");
			gitr();
			doiter( prhdr);
			break;

		case 'j': 
			xeq( 'j' );
			break;

		case 'k':
			if( verbose )
				printf("keep ");
			if( readonly == TRUE ) {
				printf("in READONLY mode - ignored\r\n");
				break;
			}
			gitr();
			doiter( keepmsg );
			break;

		case 'l': 
			if( verbose)
				printf( "list ");
			gitr();
			getfn( "to file/pipe: ", outfile, "/dev/tty");
			if( cntproc() > 1)  {
				printf( "Separate messages");
				lstsep = confirm((char *)0,DOLF);
			}  else
				lstsep = FALSE;

			if ( prettylist) {
				lstmore = TRUE;
				cpyiter( hdrfile, DOIT, dolstmsg);
			}
			else {
				lstmore = FALSE;
				cpyiter( lstmsg, DOIT,( int( *)()) 0);
			}
			break;

		case 'm': 
			if( verbose)
				printf( "move ");
			gitr();
			getfn( "to file/pipe: ", outfile, defoutfile);
			cpyiter( movmsg, DOIT,( int( *)()) 0);
			break;

		case '/': 	/* Delete current, Next */
			if( verbose)
				printf( "delete current\r\n");
			if( readonly == TRUE ) {
				printf("in READONLY mode - ignored\r\n");
				break;
			}
			if( status.ms_nmsgs == 0)  {
				printf( "'%s' is empty\r\n", filename);
				error( "" );
			}
			unset();
			if( status.ms_curmsg == 0)  {
				if( status.ms_nmsgs != 0)
					status.ms_curmsg = 1;
				else
					if( status.ms_curmsg > status.ms_nmsgs)
						error( "no current message\r\n");
			}
			setrange( status.ms_curmsg, status.ms_curmsg);
			doiter( delmsg);
			/* Fall Through to case 'n' */

		case 016:	/* ^N */
		case 'n': 
			if( verbose)
				printf( "next message is:\r\n");
			else
				suckup();
			if( status.ms_curmsg >= status.ms_nmsgs)
				error( "no next message\r\n");
			mptr = msgp[status.ms_curmsg];
			msgno = ++status.ms_curmsg;
			if( mptr->flags & M_DELETED)
				printf( "message %d deleted\r\n", msgno);
			else
				prmsg();
			break;

		case 'o': 
			if( verbose)
				printf( "overwrite old file %s", filename);
			if( readonly == TRUE ) {
				printf(" in READONLY mode - ignored\r\n");
				break;
			}
			if( confirm((char *)0,DOLF))  {
				overwrit();
				outmem = FALSE;
			}
			break;

		case 'p': 
			if( verbose)
				printf( "put ");
			gitr();
			getfn( "to file/pipe: ", outfile, defoutfile);
			cpyiter( putmsg, DOIT,( int( *)()) 0);
			break;

		case 004:		/* ^D -- EOF */
		case 'q': 
			if( verbose)
				printf( "quit");
			if( confirm((char *)0,DOLF))  {
				tt_norm();
				binbuild();
				closeup(0);
			}
			break;

		case 'r': 
			/* Update the old binary box */
			binbuild();

			if( verbose)
				printf( "read ");
			else
				suckup();
			getfn( "new file: ", filename, maininbox);
			setup( SETREAD);
			break;

		case 's': 
			if( verbose)
				printf( "send ");
			if( confirm((char *)0,DOLF)) {
				if( wmsgflag == ON && status.ms_curmsg != 0 ) {
					setrange( status.ms_curmsg,status.ms_curmsg );
					makedrft();
				}
				xeq( 'S');
			}
			break;

		case 't': 
			if( verbose)
				printf( "type ");
			gitr();
			doiter( prmsg);
			break;

		case 'u': 
			if( verbose)
				printf( "undelete ");
			if( readonly == TRUE ) {
				printf("in READONLY mode - ignored\r\n");
				break;
			}
			gitr();
			doiter( undelmsg);
			if ((j = cntproc()) > 1)
				printf("%d messages undeleted\r\n", j);
			break;

		case 'x':
			if( verbose)
				printf( "Xtra command: ");
			fflush( stdout);

			while( isspace(nxtchar = ttychar()));
			nxtchar = uptolow( nxtchar);

			switch(nxtchar) {

			case 'b':
				if( verbose)
					printf( "binary file write");
	 			if( readonly == TRUE ) {
					printf(" in READONLY mode - ignored\r\n");
					break;
				}
				if( confirm((char *)0,DOLF))
					binbuild();
				break;

			case 'c':
				if( filoutflag == OFF ) {
					if( verbose )
						printf( "Ctrl char filter on\r\n" );
					filoutflag = ON;
				}
				else {
					if( verbose )
						printf( "Ctrl char filter off\r\n" );
					filoutflag = OFF;
				}
				break;

			case 'd':
				printf("debug\r\n");
				if (status.ms_nmsgs == 0)
					printf ("No message read in...");
				else {
					if( status.ms_curmsg < 1 )
						status.ms_curmsg = 1;
					mptr=msgp[status.ms_curmsg-1];
					printf("\r\nstart=%ld, len=%ld\r\n",mptr->start, mptr->len);
					printf("Date: %s\r",ctime(&mptr->date));
					printf("Datestr: %.*s\r\n", SIZEDATE, mptr->datestr);
					printf("From: %.*s\r\n", SIZEFROM, mptr->from);
					printf("To: %.*s\r\n", SIZETO, mptr->to);
					printf("Subj: %.*s\r\n", SIZESUBJ, mptr->subject);
				}
				break;

			case '\004':
			case 'e': 
				if( verbose )
					printf( "exit without saves " );

				if( confirm((char *)0,DOLF)) {
					tt_norm();
					closeup(0);
				}
				break;

			case 'l': 
				if( verbose)
					printf( "list body ");
				gitr();
				getfn( "to file/pipe: ", outfile, "/dev/tty");
				if( cntproc() > 1)  {
					printf( "Separate messages");
					lstsep = confirm((char *)0,DOLF);
				}  else
					lstsep = FALSE;
				lstmore = FALSE;
				cpyiter( lstbdy, DOIT, ( int( *)()) 0);
				break;

			case 000:	/* see also null in main switch */
			case 'm':
				if( verbose )
					printf( "mark set\r\n" );
				status.ms_markno = status.ms_curmsg;
				break;

			case 'n':
				if( prettylist == OFF ) {
					if( verbose )
						printf( "Numbered list on\r\n" );
					prettylist = ON;
				}
				else {
					if( verbose )
						printf( "Numbered list off\r\n" );
					prettylist = OFF;
				}
				break;

			case 'o':
				if (!verbose)
					suckup();
				getfn( "output mbox file: ", defmbox, (char *)0);
				setmbox(defmbox);
				break;

			case 'p':
				if( paging == OFF ) {
					if( verbose )
						printf( "Paging on\r\n" );
					paging = ON;
				}
				else {
					if( verbose )
						printf( "Paging off\r\n" );
					paging = OFF;
				}
				break;

			case 'r':
				if( verbose )
					printf("re-order key: ");
				sortbox();
				break;

			case 's':
				if( keystrip == OFF ) {
					if( verbose )
						printf( "Strip on\r\n" );
					keystrip = ON;
				}
				else {
					if( verbose )
						printf( "Strip off\r\n" );
					keystrip = OFF;
				}
				break;

			case 'v':
				if( verbose)  {       
					printf("Verbose off -- Short form typeout\r\n");
					tt_norm();
					verbose = 0;
					goto dont_suck;
				}  else  {       
					suckup();
					if( nottty) error("input not tty\r\n");
					printf("Verbose on -- Long form typeout\r\n");
					verbose++;
					tt_raw();
				}
				break;

			case 'x':
				xostat();
				break;

			case '?': 
				help(xtcmdlst);
				break;

			default: 
				printf( " ...not a command( type ? for help )\r\n");

			}
			break;

		case 'y':
			if( verbose )
				printf("Resend ");
			gitr();
			doiter(prhdr);
			printf("To: ");
			strcpy(outfile,resendprog);
			if( (outfile[strlen(resendprog)]=rdnxtfld()) == '\n' )
				error("Cancelled\r\n");
			else {
				gather( &outfile[strlen(resendprog)], 200 );
				if( verbose )
					printf("Sending ");
				doiter( resendmsg );
				if( verbose )
					printf(" Done\r\n");
			}
			break;

		case 'z':
			if( verbose)
				printf( "z = two window mode.  Answer message: ");
			gitr();
			edansiter();
			break;

		case '\n': 
			if( verbose )
				printf( "\r\n" );
			break;

		case '!': 
			if( verbose )
				printf("!");
			if( (key[0] = rdnxtfld()) == '\n' ) {
				if (verbose) printf("\r\n");
				xeq( 's' );
			}
			else {
				gather( key, 79 );
				xeq( '!' );
			}
			printf("\r\n");
			break;

		case '?': 
			help(cmdlst);
			break;

		case ':': 
			if (verbose) printf( ": ");
			xeq( 'd');
			break;

		case ';': 
			if (verbose) printf( "; ");
			do
			    nxtchar = echochar();
			while( nxtchar != '\n' && nxtchar != '\004' );
			break;

		case ',':
			if (verbose) printf(",\r\n");
			setrange(
				(status.ms_curmsg <= 5) ? 1 : status.ms_curmsg - 5,
				(status.ms_curmsg + 5 > status.ms_nmsgs) ? status.ms_nmsgs : status.ms_curmsg+5
			);
			doiter( prhdr );
			break;

		case '@':
			if (verbose)
				printf( "@ = Undigestify " );
			gitr();
			printf( "Undigestifying " );
			doiter( undigestify );
			printf( "\r\n" );
			break;

		case '$':
			/* Drop down to the last message */
			if (verbose)
				printf("$ = Last message:\r\n");
			status.ms_curmsg = status.ms_nmsgs;
			setrange( status.ms_nmsgs, status.ms_nmsgs);
			doiter( prhdr );
			break;

		case '\177': 	  /* DEL end */
			printf( "use control-D or 'q' to exit\r\n");
			break;

		default: 
			printf( " ...not a command( type ? for help)\r\n");
		}
		if( !(verbose || nxtchar == '\n'))
			suckup();
dont_suck:;
	}
}
