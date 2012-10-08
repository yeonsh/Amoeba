/*	@(#)msg.h	1.1	91/11/21 11:43:15 */
/*
 *			M S G . H
 *
 * This file contains the declarations for all the global variables
 * and defines for the MSG program.
 *
 *		R E V I S I O N   H I S T O R Y
 *
 *	06/09/82  MJM	Split the enormous MSG program into pieces.
 *
 *	06/10/82  MJM	Added additional flags bits
 *
 *	08/07/82  MJM	Allowed compile-time setting of NMSGS
 *
 *	09/13/82  MJM	Made msgp[] an array of pointers, to permit
 *			undelete to insert messages.
 *
 *	12/04/82  HW	Added xtra options, .msgrc, fixed two signal bugs.
 */

#define	ON		1
#define	OFF		0

#define SETREAD		1		/* regular file read parsing */
#define SETAPND		2		/* parse appended messages  */
#define SETUNDIGEST	3		/* undigestify one message */

/* Selection of who to answer to */
#define ANSFROM		01		/* just to senders */
#define ANSCC		02		/*   from & cc names */
#define ANSTO		04		/*   from & to names */
#define ANSALL		07		/* copies to all recipients */

/* Flags to gethead() to omit a field */
#define NODATE		(char *)0
#define NOFROM		(char *)0
#define NOSNDR		(char *)0
#define NORPLY		(char *)0
#define NOTO		(char *)0
#define NOCC		(char *)0
#define NOSUBJ		(char *)0

#define SIZEDATE	9
#define SIZEFROM	30	   	/* ALSO change hdrfmt  */
#define SIZESUBJ	40		/* */
#define SIZETO		30

#define	M_BSIZE		512

#define SPSIGS		0400007

struct message {
	long	start;			/* Offset into file */
	long	len;			/* Length of message */
	int	flags;			/* FLAGS word */
	long	date;
	char	datestr[SIZEDATE];
	char	from[SIZEFROM];
	char	to[SIZETO];
	char	subject[SIZESUBJ];
	short	dummy;
};

/* Flags values in msg[] struct */
#define M_DELETED	0000001		/* Msg flagged for deletion */
#define	M_PUT		0000002		/* Msg put to another file */
#define M_NEW		0000004		/* Msg not seen yet */
#define M_KEEP		0000010		/* Msg to be kept in this file */
#define M_ANSWERED	0000020		/* Msg has been answered */
#define M_FORWARDED	0000040		/* Msg has been forwarded */

#define M_RESTMAIL	0040000		/* Special for OverWrite only */
#define M_PROCESS_IT	0100000		/* Msg flagged for processing */

#ifndef NOEXTERNS
/* MMDF GLOBALS */
extern int 	sentprotect;	/* default mailbox protection */
extern char	*mldflfil;	/* default mailbox file */
extern char	*mldfldir;	/* directory containing mailbox file */
extern char	*delim1;	/* string delimiting messages (start) */
extern char	*delim2;	/* string delimiting messages (end) */
/* End of MMDF GLOBALS */

extern struct message *msgp[];
extern struct message *mptr;

extern int  errno;			/* sys-call error number */

extern int Nmsgs;		/* For Global use */

#define IDENTITY	0x6832		/* Magic Number; current value h2 */
/* Status structure, also exists at the front of a binary index file. */
struct status {
	int		ms_ident;	/* "Magic Number" */
	long		ms_time;	/* Last time file was processed */
	unsigned	ms_nmsgs;	/* Number of messages processed */
	unsigned	ms_curmsg;	/* "Current" message */
	long		ms_eofpos;	/* Msg file eof position */
	unsigned	ms_markno;	/* Marked message no */
	int		ms_xxx1;	/* Place holder - not used */
	int		ms_xxx2;	/* Place holder - not used */
	long		ms_xxx3;	/* Place holder - not used */
};

extern struct status status;

#define OLDIDENTITY	0x6831		/* Magic Number; previous value h1 */
struct oldstatus {
	int		ms_ident;	/* "Magic Number" */
	long		ms_time;	/* Last time file was processed */
	unsigned	ms_nmsgs;	/* Number of messages processed */
	unsigned	ms_curmsg;	/* "Current" message */
	long		ms_eofpos;	/* Msg file eof position */
};

/* Stat buffers, used for comparing times */
extern struct stat statb1;			/* Reference buffer */
extern struct stat statb2;			/* Temporary buffer */

extern char	ch_erase;

extern int     nottty;
extern unsigned int    msgno;		/* message currently being processed */

/* user search key for from/subject/text msg sequences */
extern char    key[];
extern char	*gc;

extern char    filename[];
extern char	outfile[];
extern char	defmbox[];
extern char	oldfile[];
extern char	*homedir;		/* Full path to home directory */

extern char maininbox[60];
extern char binarybox[60];	    /* Binary map file filename */
extern char msgrcname[60];	    /* User options filename */
extern char defoutfile[60];
extern char username[20];
extern char ascending;
extern char ismainbox;		    /* current file is receiving .mail  */
extern char nxtchar;
extern char lstsep;		    /* separate messages by formfeed   */
extern char lstmore;		    /* second, or more listed message  */
extern char autoconfirm;	    /* bypass asking user confirmation */
extern char anstype;		    /* what addresses to send answers to  */
extern char binaryvalid;	    /* TRUE if incore binary box is valid */
extern sigtype (*orig) ();	    /* DEL interrupt value on entry */
extern int  verbose;

extern int  outfd;

extern int exclfd;		/* overwrit() exclusive use fd */
extern jmp_buf savej;

extern unsigned int ansnum;
extern unsigned int fwdnum;

extern FILE *filefp;			/* Input FILEp */
extern FILE *outfp;			/* Output FILEp */
extern FILE *binfp;			/* Binary map FILEp */
extern FILE *fpmsgrc;			/* User options FILEp */

extern char    ttyobuf[];
extern char    sndto[];
extern char    sndcc[];
extern char	sndsubj[M_BSIZE + 9];

extern char	lastc;
extern int	istty;
extern int	keystrip;
extern int	mdots, bdots, bprint;
extern int	paging;
extern int	pagesize;
extern int	quicknflag;
extern int	quickexit;
extern int	filoutflag;
extern int	binsavflag;
extern int	wmsgflag;
extern int	doctrlel;
extern int	prettylist;
extern int	linelength;
extern int	linecount;
extern int	outmem;
extern char	*ushell, *ueditor;
extern int	readonly;
extern char	*tempname;
extern char	*binarypre;
extern char	*savmsgfn;
extern char	*sndname;		/* Name of mail sending program */
extern char	*nullstr;		/* A null string */

extern char	twowinfil[];		/* filter for 2-window-answer mode */
extern char	draft_work[];		/* Work file for 2-window answer */
extern char	draft_original[];  /* Original message(s) for 2-window ans */
extern char	draftorig[];		/* Message saving file */

#define	MAXKEYS		41
extern char	*keywds[];
#define	SP_BODY		0	/* Reading msg body */
#define	SP_HNOSP	1	/* Line was NOT stripped */
#define SP_HSP		2	/* Line was stripped */

#define DOIT		0	/* Argument to cpyiter() */
#define NOIT		1	/* Argument to cpyiter() */
#define DOLF		0
#define NOLF		1

extern char *compress ();
extern char *strdup ();
extern char *malloc();
/*amoeba changeextern char *index(), *strend(), *rindex();*/
extern char  *strchr(), *strend(), *strrchr();

extern prmsg(), delmsg(), undelmsg(), prhdr(), movmsg(), onint(), onnopipe();
extern putmsg(), writmsg(), gomsg(), lstmsg(), keepmsg(), lstbdy(), writbdy();
extern onstop(), hdrfile(), dolstmsg();

extern ansmsg(), fwdmsg(), fwdpost(), resendmsg(), ansend();
extern undigestify();
extern xomsgrc();
extern char *getenv();
extern char *xfgets();
extern long smtpdate();
extern qcomp();

#endif NOEXTERNS
