/*	@(#)conf.c	1.3	96/02/27 09:56:14 */
#include "util.h"
#include "mmdf.h"
#include "ap_norm.h"
#include "amoeba.h"
#include "semaphore.h"
#include "vc.h"

/*            COMPILE-TIME CONFIGURATION FILE WITH SITE-DEPENDENT INFORMATION
 *
 *      This file contains general host-dependent variable info that
 *   the message system needs to have.
 */

struct vc *vc;
struct vc *vcsend;

char *mmtailor = "/profile/module/mmdf/mmdftailor";
				/* location of external tailoring file  */

/* ************************  PUBLIC NAMES  ****************************** */

char
	*locname = "am",  /* Generic name for local host   */
	*locdomain = "cs.vu.nl",	/* Name of domain that is local to us */
	*sitesignature = "Mail System (MMDF)",
				/* in signature field of return-mail  */
	*mmdflogin = "mmdf",	/* login name for mmdf         */
	*supportaddr = "Postmaster@am.cs.vu.nl";
				/* where to send bug reports, etc. */
/* *****************  DEFAULT BASE DIRECTORIES  *********************** */

/*  The following pathnames define the default working directories for
 *  MMDF activities.  Specific files usually will be referenced relative
 *  to these directories.  To override the default, the specific filename
 *  should be specified as an anchored pathname, beginning with a slash.
 *
 *  The following pathnames must be anchored.
 */

char	*cmddfldir = "/profile/module/mmdf";
			      /* contains MMDF commands, such as      */
			      /* submit, deliver, and queclean        */
char	*logdfldir = "/profile/module/mmdf/log";
			      /* contains highly volatile files, such */
			      /* as logs and check-point markers      */
char	*phsdfldir = "/profile/module/mmdf/log/phase";
			      /* contains timestamp files             */
char	*tbldfldir = "/profile/module/mmdf/table";
			      /* contains sticky files, such as       */
			      /* name tables & dialing scripts        */
char	*tbldbm = "/profile/module/mmdf/table/mmdfdbm";
			      /* dbm() hash of name tables      */
char	*quedfldir = "/profile/module/mmdf/lock/home";
			      /* contains queued mail files, in       */
			      /* subordinate directories              */
			      /* (also see below)                     */
char	*chndfldir = "/profile/module/mmdf/chans";
				/* contains the channel programs        */
				/* (ch_*) called by deliver             */
char	*lckdfldir = "/profile/module/mmdf/tmp";
				/* Directory for lock files SEK if needed */


/* ***************  DEFAULT LOG LOCATIONS & SETTINGS  ***************** */

char    mlogloc[] = "msg.log";
char    chlogloc[] = "chan.log";

/*  It is common for specific using processes to tailor the second
 *  field, which is the log entry label, so that you can tell which
 *  incarnation of the process made a set of entries.
 *
 *  These structures conform to ll_log.h
 */

struct ll_struct
		    msglog =      /* deliver, submit, & queclean          */
{
    mlogloc, "XX-0000", LLOGFST, 40, LLOGSOME, 0
},

		   chanlog =      /* log for channels & user agents       */
{
    chlogloc, "UA-0000", LLOGFST, 40, LLOGSOME, 0
};

/* Needed for user authorisation logging        */

struct ll_struct authlog =
{
    "auth.log", "AU-0000", LLOGFTR, 40, LLOGSOME, 0
};

int domsg;

char *zap_env[2] = {
	"TZ=MET",
	NULL
};
/* ****************  MESSAGE QUEUE SUBSTRUCTURE  ********************** */

/*  The queue has a top-level, locking directory, which only trusted
 *  processes can get through.  Under that is a base working directory.
 *  Below that are three directories which contain queued messages data
 *
 *  quedfldir[] specifies the top-level and base directories.
 */

int     queprot = 0700;           /* protection on quedfldir parent      */

/* the next five are relative to quedfldir[] */

char
	*deadletter = "DeadLetters",
				/* Actually NOT a directory, where	*/
				/* unreturned letters go to die.	*/
	*tquedir = "tmp/",	/* sub-directory for builing            */
				/* address list file for msg            */
	*aquedir = "addr/",	/* sub-dir with files of address        */
				/* lists for queued mail; files are     */
				/* linked into here from tquedir        */
				/* as last act of queuing msg           */
	*squepref = "q.",	/* preface sub-queue name with this	*/
	*mquedir = "msg/";	/* sub-dir containing text of           */
				/* queued messages; file names          */
				/* are same as in aquedir               */

/*  The following two parameters establish thresholds for queue residency
 *  limits.  If a message does not move out of the queue by warntime,
 *  a notice may be sent to the originator.  If it is still in the queue
 *  by failtime, it is removed from the queue and may be returned to the
 *  originator.
 */

int
	warntime = 36,
	failtime = 96;


/* ******************  COMMAND NAMES & LOCATIONS  *********************** */

char
	*namsubmit = "submit",      /* who to submit mail through         */
	*pathsubmit = "submit",     /* location relative to cmddfldir[]   */

	*namdeliver = "deliver",    /* name of delivery overseer          */
	*pathdeliver = "deliver",   /* location relative to cmddfldir[]   */

	*nampkup = "pickup",        /* name of pickup overseer            */
	*pathpkup = "deliver",      /* location relative to cmddfldir[]   */

	*nammail = "mmdfmail",      /* the mmdf simple mail-sender        */
	*pathmail = "v6mail";       /* needs to handle special switches   */
				    /* probably must be MMDF's uip/mail   */

/* *********************  SUBMIT TAILORING  ************************** */

int     maxhops = 40;               /* number of Via fields permitted   */
int	mgt_addid = 1;		    /* if set, add message-id if necessary */
int	lnk_listsize = 12;	    /* if more than this many addresses,
				     * then do not send warning and only
				     * send citation on return
				     */


/*   *********************  DELIVER TAILORING  ************************** */

int	maxqueue = 300;		  /* maximum size of sortable queue       */
int	mailsleep = 600;          /* seconds between queue sweeps by a    */
				  /* background (-b) Deliver              */

/*   *********************  ADDRESS TRANSFORMATION  ********************* */


/*  Format:
 *
 *  Original host name, New hostname, String appended to mailbox
 *
 *      e.g.        "DCrocker at UDel-EE"
 *      maps to     "DCrocker.EE at UDel"
 *
 *  Note the usage, for Darcom.  It is designed to bypass address
 *  mapping, so that "foo at bar" does not go out as "foo.bar at relay"
 */

LOCVAR struct ap_hstab hstab[] =
{
    0, 0, 0,
};
struct ap_hstab *ap_exhstab = hstab;

/* *****************  LOCAL DELIVERY TAILORING  ************************* */

/*  Obviously, the following is only needed if there is local delivery,
 *  rather than pure relaying (a rare, but permitted mode).  Simplest
 *  determinor is ch_okloc in conf_chan.c
 */

int     sentprotect = 0600;         /* protection on mail files             */

char
	*dlvfile = "home/.maildelivery", /* User specified delivery instructions */
	*sysdlvfile = (char *)0,  /* if non-zero, the default dlvfile */
	*mldfldir = { 0 },	/* directory to contain users' file     */
				/* for receiving local mail.  if this   */
				/* spec is empty, recipient's login     */
				/* directory will be used.              */
	*mldflfil = "home/mailbox",    /* file to receive new mail             */
				  /* relative to mldfldir[] or to login   */
				  /*   dir if mldfldir[] is empty         */
				  /* if this spec empty, user's login     */
				  /* name will be name of file            */
	*delim1 = "\001\001\001\001\n",
	*delim2 = "\001\001\001\001\n";
				  /* add to begin and end of messages     */
				  /* NOTE:  to avoid author spoofin,      */
				  /* lo_wtmail() catches mail that        */
				  /* has either of these strings in it    */
				  /* and changes the first char           */
				  /* (it increments its ascii value)      */


/* *****************  UUCP CHANNEL TAILORING  *********************** */

char	*Uchan = "uucp";		/* default channel name */
char    *Uuxstr = "uux - -z";		/* command string to start UUX */
char	*Uuname = "am.cs.vu.nl";

/******************* NIFTP CHANNEL TAILORING  *************************/

char	*pn_quedir   = "/usr/spool/jntmail";
				/* Location of JNTmail queues for NIFTP */

/******************* MULTIPLE HOST TAILORING **************************
 *
 *  This variable is initialised to be the name of the local machine
 *  It is used to transparently forward between the UCL machines
 */
char	*locmachine = "vmesc1";


/******************* AUTHORIZATION TAILORING **************************/
char	*authrequest = "Postmaster@am.cs.vu.nl";
				/* authorisation request address        */
char	*authfile = "/profile/module/mmdf/warning";
				/* warning letter - full pathname       */

















