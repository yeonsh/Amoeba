/*	@(#)s_main.c	1.1	91/11/21 11:48:53 */
/*
   interactive mail-sender

   ??? ?? ?. ?????          Probably originated in Illinois, but it's
			    like Daniel Boone's knife, with the blade
			    replaced 3 times & the handle 4.
   Oct 80 S. Cracraft       Conversion from V6 to V7 and incorprate changes
			    for all interaction done with MMDF, including
			    *_address routines
   Spr 81 D. Crocker        More cleanup; adding V6 compatability; signature;
			    draft; forward inclusion mod; some help
			    commands; better address parsing
   Jul 81 D. Crocker        add quoting to name-parts of addresses, with
			    special characters; allow comment fields to
			    have special chars.
			    minor diddling with fieldname displaying
    Aug 81 D. Crocker       gather() needed to check characters from getchar
    Sep 81 D. Crocker       have Bcc not printed, when there are no To addrs
    Oct 81 D. Crocker       Make BCC say "(Private)" and disable To & CC,
			    to prevent accidental replies.
			    Make continuation  header lines begin with space,
			    instead of name.
    Nov 81 D. Crocker       save copy in ",sent", instead of ",send"
			    input commands case-insensitive
    Jan 82 D. Kingston	    Modified to make use of the ",sent" file optional
			    based on defining SENTFILE, also changed screen
			    editor to Emacs.  (BRL 6.145)
    Feb 82 D. Kingston	    Users full name is now taken from /etc/passwd,
			    Network name is placed in <> afterwords (BRL 6.152)
    Mar 82 D. Kingston	    Have send close file desc. 3-N before execing
			    subshells or subprograms.
    Apr 82 D. Kingston	    Rearranged the invocation of submit.  Lossage:
			    channel table fd's are still left open on execs
	03/31/83  GWH	    Split SEND into its component parts.

	06/30/83  GWH	Added numerous features to send. See comments in
			"s_get.c", etal. Also added unique draft file naming
			with the draft files' deletion at the end of the
			session.

	01/29/86 TRT	Changed the input scheme so that the user is placed
			directly into a visual editor, and is free to edit
			both the headers and body.  Also removed the
			following options:
				direct edit, header edit, input more body,
				delete body, visual edit (same as edit)
			Threw in some assorted comments.
*/


#include "./s.h"

extern int errno;
extern char *compress ();
extern char *index();

/* mmdf globals */

extern int  sentprotect;
extern char *locname;
extern char *delim1;

/* end of mmdf globals */

char hdrfmt[] = "%-10s%s\n";
char    toname[] =  "To:  ";
char    BCtoname[] = "[To]: ";   /* Bcc copy's To fieldname              */
char    ccname[] =  "cc:  ";
char    BCccname[] = "[cc]: ";   /* Bcc copy's CC fieldname              */
char    subjname[] = "Subject:  ";
char    fromname[] = "From:  ";
char    datename[] = "Date:  ";
char    bccname[] = "BCC:  ";
char    shrtfmt[] = "%s%s\n";

jmp_buf savej;

int     drffd;                  /* handle on the draft file */
int 	tmpfd;
int     nsent;
int     badflg;			  /* true if at least one bad address */
sigtype	(*orig) ();               /* to save old signal values */

char   *adrptr;                   /* field currently getting addresses  */
char    bigbuf[BBSIZE],              /* buffer for text of msg */
	signature[S_BSIZE],            /* hold the signature */
	host[64],                 /* default hostname */
	from[S_BSIZE],
	to[S_BSIZE],          /* primary recipients                 */
	bcc[S_BSIZE],         /* blind carbon copy addresses        */
	cc[S_BSIZE],          /* secondary recipients               */
	subject[S_BSIZE],
	stdobuffer[BUFSIZ],
	inclfile[FILNSIZE],
	aliasfilename[128],
	drffile[S_BSIZE],
	tmpdrffile[S_BSIZE];
	
char    body,
	lastsend,
	aborted,
	inrdwr,                   /* true if in a read or write */
	toflag = TRUE,
	ccflag= TRUE,
	subjflag = TRUE;

/* Set the default user settable options */

char editor[128];
char checker[128];
char copyfile[128];
char subargs[128];
int wflag, cflag;
int rflag;
int qflag, pflag;
struct header *headers = (struct header *)0;

/**/

main (argc, argv)
int     argc;
char   *argv[];
{
    extern sigtype  onint2 ();
    int     retval;

    mmdf_init( argv[0] );

    setbuf (stdout, stdobuffer);

    if (setjmp(savej) != 0)
	exit(9);

    orig = signal (SIGINT, onint2);

    if (!getuinfo ())
	snd_abort ("Problem acquiring user information.\n");

    if (rp_isbad (retval = mm_init ()) || rp_isbad (retval = mm_sbinit ()))
	snd_abort ("Problem with mail startup [%s].\n", rp_valstr (retval));

    arginit (argc, argv);

    if ((drffd = creat (drffile, sentprotect)) >= 0)
	drclose ();
    else                          /* can't get a clean drf            */
    {
	fprintf(stderr,"Can't open draft file '%s'\n", drffile);
	fflush(stdout);
	s_exit( NOTOK );
    }

    if((tmpfd = creat(tmpdrffile, sentprotect)) >= 0)
	close( tmpfd );
    else
    {
	fprintf(stderr,"Can't open temporary draft file '%s'\n", tmpdrffile);
	fflush(stdout);
	s_exit( NOTOK );
    }

    input ();

    mm_end (OK);
    fflush (stdout);
    s_exit (0);
}
