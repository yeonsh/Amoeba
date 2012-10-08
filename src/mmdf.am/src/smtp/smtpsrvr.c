/*	@(#)smtpsrvr.c	1.3	96/02/27 09:58:34 */
/*
 *                      S M T P S R V R . C
 *
 *              SMTP Mail Server for MMDF under 4.2bsd
 *
 * Eric Shienbrood (BBN) 3 Apr 81 - SMTP server, based on BBN FTP server
 * Modified to talk to MMDF Jun 82 by Donn Neuhengen.
 * Doug Kingston, BRL: Took a machete to large chunks of unnecessary code.
 *      Reimplemented the interface to MMDF and used 4.2BSD style networking.
 *      Usage:  smtpsrvr <them> <us> <channel>
 */


#include "util.h"
#include "mmdf.h"
#include "ch.h"
#include "ap.h"
#include "phs.h"
#include "smtp.h"
#include "stdio.h"
#include "signal.h"
#include "sys/stat.h"
#include "pwd.h"
#include "string.h"
#include "amoeba.h"
#include "semaphore.h"
#include "circbuf.h"
#include "server/ip/tcpip.h"
#include "class/stdstatus.h"
#ifndef NOFCNTL
#include "fcntl.h"
#endif

#include "ns.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "smtpsrvr.h"
#include "stderr.h"
#include "module/name.h"
#include "module/syscall.h"

#define BYTESIZE        8
#define SMTP_PORT       25
/*#define DEBUG*/

/* mmdf configuration variables */
extern LLog *logptr;
extern char *supportaddr;
extern int errno;               /* Has Unix error codes */
extern int numfds;
extern char *locname;
extern char *locdomain;


extern char *getline();
extern char *index();
extern char *rindex();
extern char *malloc();
extern char *strdup();
extern struct passwd *getpwmid();
extern char *multcat();
extern Chan *ch_h2chan();

char *progname, *us, *them, *channel;   /* Arguments to program */
char *sender = 0;                       /* address of mail sender */
Chan *chanptr;                          /* pointer to incoming channel */

#define BUFL 600                /* length of buf */
char    buf[BUFL];              /* general usage */
char    netbuf[BUFL];           /* the place that has the valid characters */
char	saveaddr[BUFL];         /* save previous address in expn_save */
int     savecode;               /* save previous result code in expn_save */
int     expn_count;             /* number of expn expansions */
int     netcount = 0;           /* number of valid characters in netbuf */
char    *netptr  = netbuf;      /* next character to come out of netbuf */
char    *arg;                   /* zero if no argument - pts to comm param */
int     dont_mung;              /* used by getline() to limit munging */
int     numrecipients = 0;      /* number of valid recipients accepted */
int     stricked;               /* force rejection of non validated hosts */
#ifdef NODOMLIT
int	themknown=TRUE;		/* do we have symbolic name for them? */
#endif /* NODOMLIT */
char    *addrfix();
int     vrfy_child;             /* pid of child that handles vrfy requests */
int     vrfy_p2c[2];            /* fd's for vrfy's parent-to-child pipe */
int     vrfy_c2p[2];            /* fd's for vrfy's child-to-parent pipe */
FILE    *vrfy_out;              /* fd for vrfy's parent to write to child */
FILE    *vrfy_in;               /* fd for vrfy's parent to read from child */

/* character defines */
#define CR      '\r'    /* carriage return */
#define LF      '\n'    /* line feed */
#define CNULL   '\0'    /* null */


errstat err;
capability chancap;
/****************************************************************
 *                                                              *
 *      C O M M A N D   D I S P A T C H   T A B L E             *
 *                                                              *
 ****************************************************************/

int helo(), mail(), quit(), help(), rcpt(), confirm();
int data(), rset(), reject(), expn(), vrfy();

struct comarr           /* format of the command table */
{
	char *cmdname;          /* ascii name */
	int (*cmdfunc)();       /* command procedure to call */
} commands[] = {
	"helo", helo,           "noop", confirm,
	"mail", mail,           "data", data,
	"rcpt", rcpt,           "help", help,
	"quit", quit,           "rset", rset,
        "expn", expn,           "vrfy", vrfy,
	NULL, NULL
};


/*
 *              M A I N
 *
 *      Takes commands from the assumed network connection (file desc 0)
 *      under the assumption that they follow the ARPA network mail
 *      transfer protocol RFC 788 and appropriate modifications.
 *      Command responses are returned via file desc 1.
 *
 *      There is a small daemon waiting for connections to be
 *      satisfied on socket 25 from any host.  As connections
 *      are completed by the ncpdaemon, the returned file descriptor
 *      is setup as the standard input (file desc 0) and standard
 *      output (file desc 1) and this program is executed to
 *      deal with each specific connection.  This allows multiple
 *      server connections, and minimizes the system overhead
 *      when connections are not in progress.  File descriptors
 *      zero and one are used to ease production debugging and
 *      to allow the forking of other relavent Unix programs
 *      with comparative ease.
 *
 *              while commands can be gotten
 *                      find command procedure
 *                              call command procedure
 *                              get next command
 *                      command not found
 *
 */

unsigned long t_start, t_base;

main (argc, argv)
     int argc;
     char **argv;
{
  register struct comarr *comp;
  char    replybuf[128];
  char *i;
  long atime;
  Chan *curchan;
  char    tmp_buf[LINESIZE];
  char    tmpstr[LINESIZE];
  char    *Ags[20];
  int     n, Agc;
  int ret = -1;
  int j = 0;

#ifdef DEBUG
  t_start= sys_milli();
  printf("1: just started, time= %d\n", t_base);
#endif

  if (argc != SMTPARGS){
    ll_log(logptr,LLOGFTR,"Incorrect number of args passed by smtpd -- exiting");
    exit(1);
  }

  them = (char *)malloc(512);
  progname = argv[0];
  mmdf_init( progname );
  
#ifdef DEBUG
  t_base=sys_milli();
#endif
  name_lookup(argv[1],&chancap);
#ifdef DEBUG
  printf("2: name_lookup: %d\n",sys_milli()-t_base);
#endif


#ifdef DEBUG
  t_base=sys_milli();
#endif
  err= tcpip_keepalive_cap(&chancap);
#ifdef DEBUG
  printf("3: tcpip_keepalive_cap: %d\n",sys_milli()-t_base);
#endif

  if (err != STD_OK)
  {
    std_destroy(&chancap);
  }


#ifdef DEBUG
  t_base=sys_milli();
#endif
  name_delete(argv[1]); 
#ifdef DEBUG
  printf("4: name_delete: %d\n",sys_milli()-t_base);
#endif


#ifdef DEBUG
  t_base=sys_milli();
#endif
    
  /* force rejection of unknown hosts */
  if (*progname == 'r')
    stricked++;
    
  strcpy(them, "unixland");
  us = argv[SMTPARGS -1];
	
	
  /*
   * found out who you are I might even believe you.
   */
  
  /*
   * the channel arg is now a comma seperated list of channels
   * useful for multiple sources ( As on UCL's ether )
   */
  strcpy(tmp_buf,"smtp");
  Agc = str2arg (tmp_buf, 20, Ags, (char *)0);
  channel = Ags[Agc-1];
  for(chanptr = (Chan *)0, n = 0 ; n < Agc ; n++){
    if ( (curchan = ch_nm2struct(Ags[n])) == (Chan *) NOTOK) {
      ll_log (logptr, LLOGTMP, "smtpsrvr (%s) bad channel",
	      Ags[n]);
      continue;
    }
    /*
     * Is this a valid host for this channel ?
     */
    switch(tb_k2val (curchan -> ch_table, TRUE, them, tmpstr)){
    default:        /* Either NOTOK or MAYBE */
      if ((n != (Agc-1)) || stricked)
	continue;
      /* fall through so we get some channel name to use */
    case OK:
      chanptr = curchan;
      channel = curchan -> ch_name;
      break;
    }
    break;
  }
	
  time(&atime);

  if (chanptr == (Chan *) 0){
    ll_log (logptr, LLOGTMP, "smtpsrvr (%s) no channel for host",
	    them);
  }
  else{
    ch_llinit (chanptr);
    ll_log( logptr, LLOGGEN, "OPEN: %s %.19s (%s)",
	   them, ctime(&atime), channel);
    phs_note(chanptr, PHS_RESTRT);
  }
  

  mmdfstart();
  

  if (chanptr == (Chan *) 0){
    sprintf (replybuf,
	     "421 %s: Your name, '%s', is unknown to us.\r\n",
	     us, them);
    netreply (replybuf);
    tcp_exit (-1);
  }
  
  /* say we're listening */
  sprintf (replybuf, "220 %s Server SMTP (Complaints/bugs to:  %s)\r\n",
	   us, supportaddr);
  netreply (replybuf);

#ifdef DEBUG
  printf("5: connection set-up: %d\n", sys_milli()-t_base);
#endif

  
 nextcomm:
  while (i = getline())
    {

      if (i == (char *)NOTOK)         /* handle error ??? */
	byebye( 1 );
      
      /* find and call command procedure */
      comp = commands;
	    
      while( comp->cmdname != NULL)   /* while there are names */
	{
	  if (strcmp(buf, comp->cmdname) == 0) /* a winner */
	    {
#ifdef DEBUG
	      t_base=sys_milli();
#endif
	      (*comp->cmdfunc)();     /* call comm proc */
#ifdef DEBUG
	      printf("6: command %s: %d\n",comp->cmdname,sys_milli()-t_base);
#endif
	      goto nextcomm;          /* back for more */
	    }
	  comp++;             /* bump to next candidate */
	}
      netreply("500 Unknown or unimplemented command\r\n" );
      ll_log(logptr, LLOGBTR, "unknown command '%s'", buf);
    }

  byebye(0);
}

/*name:
	getline

function:
	get commands from the standard input terminated by <cr><lf>.
	afix a pointer( arg ) to any arguments passed.
	ignore carriage returns
	map UPPER case to lower case.
	manage the netptr and netcount variables

algorithm:
	while we havent received a line feed and buffer not full

		if netcount is zero or less
			get more data from net
				error: return 0
		check for delimiter character
			null terminate first string
			set arg pointer to next character
		check for carriage return
			ignore it
		if looking at command name
			convert upper case to lower case

	if command line (not mail)
		null terminate last token
	manage netptr

returns:
	0 for EOF
	-1 when an error occurs on network connection
	ptr to last character (null) in command line

globals:
	dont_mung
	netcount=
	netptr =
	buf=
*/

char *
getline()
{
	register char *inp;     /* input pointer in netbuf */
	register char *outp;    /* output pointer in buf */
	register int c;         /* temporary char */
	extern char *progname;

	inp = netptr;
	outp = buf;
	arg = 0;

	do
	{
		if( --netcount <= 0 )
		{
			if (setjmp(timerest)) {
				ll_log( logptr, LLOGTMP,
					"%s net input read error (%d)",
					them, errno);
				return((char *)NOTOK);
			}
			s_alarm (NTIMEOUT);
			netcount = tcpip_read (&chancap, netbuf, BUFL);

			s_alarm( 0 );
			if (netcount == 0)      /* EOF */
				return( 0 );
			if (netcount < 0) {     /* error */
				ll_log( logptr, LLOGTMP,
					"%s net input read error (%d)",
					them, errno);
				return((char *)NOTOK);
			}
			inp = netbuf;
		}
		c = *inp++ & 0377;

		if (c == '\r' ||        /* ignore CR */
		    c >= 0200)          /* or any telnet codes that */
			continue;       /*  try to sneak through */
		if (dont_mung == 0 && arg == NULL)
		{
			/* if char is a delim afix token */
			if (c == ' ' || c == ',')
			{
				c = CNULL;       /* make null term'ed */
				arg = outp + 1; /* set arg ptr */
			}
			else if (c >= 'A' && c <= 'Z')
			/* do case mapping (UPPER -> lower) */
				c += 'a' - 'A';
		}
		*outp++ = c;
	} while( c != '\n' && outp < &buf[BUFL] );

	if( dont_mung == 0 )
		*--outp = 0;            /* null term the last token */

	/* scan off blanks in argument */
	if (arg) {
		while (*arg == ' ')
			arg++;
		if (*arg == '\0')
			arg = 0;        /* if all blanks, no argument */
	}
	if (dont_mung == 0)
		ll_log( logptr, LLOGFTR, "'%s', '%s'", buf,
			arg == 0 ? "<noarg>" : arg );

	/* reset netptr for next trip in */
	netptr = inp;
	/* return success */
	return (outp);
}

/*
 *  Process the HELO command
 */
helo()
{
	char replybuf[128];

        (void) sprintf(replybuf, "%s.%s.%s", them, locname, locdomain);
strcpy(them,arg);
        if(arg == 0 || !lexequ(arg, them) && !lexequ(arg, replybuf))
                sprintf(replybuf, "250 %s - you are a charlatan\r\n", us);
        else  {                
 		if (lexequ(arg, replybuf))
                       them = strdup(replybuf);
                sprintf (replybuf, "250 %s\r\n", us);
        }                      
        netreply (replybuf);   
}

/*
 *      mail
 *
 *      handle the MAIL command  ("MAIL from:<user@host>")
 */
mail()
{
	char    replybuf[256];
	char    info[128];
	char    *lastdmn;
	struct rp_bufstruct thereply;
	int	len;
	AP_ptr  domain, route, mbox, themap, ap_sender;

	if (arg == 0 || *arg == 0) {
		netreply("501 No argument supplied\r\n");
		return;
	} else if( sender ) {
		netreply("503 MAIL command already accepted\r\n");
		return;
	} else if (!equal(arg, "from:", 5)) {
		netreply("501 No sender named\r\n");
		return;
	}

	ll_log( logptr, LLOGFTR, "mail from: '%s'", arg );

	/* Scan FROM: parts of arg */
	sender = index (arg, ':') + 1;
	sender = addrfix( sender );
	/*
	 * If the From part is not the same as where it came from
	 * then add on the extra part of the route.
	 */

#ifdef NODOMLIT
	if(themknown && ((ap_sender = ap_s2tree(sender)) != (AP_ptr)NOTOK)){
#else
	if ((ap_sender = ap_s2tree(sender)) != (AP_ptr)NOTOK){
#endif /* NODOMLIT */
		/*
		 * this must be a bit of a sledge hammer approach ??
		 */
		ap_t2parts(ap_sender, (AP_ptr *)0, (AP_ptr *)0,
						&mbox, &domain, &route);
		themap = ap_new(APV_DOMN, them);
		if(ap_dmnormalize(themap, (Chan *)0) == MAYBE)
			goto tout;
		if(route != (AP_ptr)0){
			/*
			 * only normalize the bits that we need
			 */
			if(ap_dmnormalize(route, (Chan *)0) == MAYBE)
				goto tout;
			lastdmn = route->ap_obvalue;
			}
		else if(domain != (AP_ptr)0) {
			if(ap_dmnormalize(domain, (Chan *)0) == MAYBE)
				goto tout;
			lastdmn = domain->ap_obvalue;
		}
		else /* is this a protocol violation? - add themap as domain? */
			lastdmn = (char *)0;

		/*
		 * Check the from part here. Make exceptions for local machines
		 */
		if (	lexequ(themap->ap_obvalue, lastdmn)
		     || islochost(themap->ap_obvalue, lastdmn))
				ap_free(themap);
		else {
			if (route != (AP_ptr)0)
				themap->ap_chain = route;
				route = themap;
		}
		sender = ap_p2s((AP_ptr)0, (AP_ptr)0, mbox, domain, route);
		if(sender == (char *)MAYBE){    /* help !! */
	tout:;
			sender = (char *) 0;
			netreply("451 Nameserver timeout during parsing\r\n");
			return;
		}
		strcpy(arg, sender);
		free(sender);
		sender = arg;
	}

	/* Supply necessary flags, "tiCHANNEL" will be supplied by winit */
	if (*sender == '\0') {
		/* No return mail */
/*  until mailing list fix is done -- er.. *WHAT* mailing list fix -- [DSH]
		sprintf( info, "mvqdh%s*k%d*", them, NS_NETTIME ); */
		sprintf( info, "mvqh%s*k%d*", them, NS_NETTIME );
		sender = "Orphanage";           /* Placeholder */
	} else
/*  until mailing list fix is done 
		sprintf( info, "mvdh%s*k%d*", them, NS_NETTIME ); */
		sprintf( info, "mvh%s*k%d*", them, NS_NETTIME );


	if( rp_isbad( mm_winit(channel,info,sender) )) {
		netreply("451 Temporary problem initializing\r\n");
		sender = (char *) 0;
		mm_end( NOTOK );
		mmdfstart();
	}
	else if( rp_isbad( mm_rrply(&thereply,&len) )) {
		netreply( "451 Temporary problem initializing\r\n" );
		sender = (char *) 0;
		mm_end( NOTOK );
		mmdfstart();
	} 
	else if( rp_gbval(rp_gbval(thereply.rp_val) ) == RP_BNO) {
		sprintf (replybuf, "501 %s\r\n", thereply.rp_line);
		netreply (replybuf);
		sender = (char *) 0;
		mm_end( NOTOK );
		mmdfstart();
	} 
	else if( rp_gbval(rp_gbval(thereply.rp_val) ) == RP_BTNO) {
		sprintf (replybuf, "451 %s\r\n", thereply.rp_line);
		netreply (replybuf);
		sender = (char *) 0;
		mm_end( NOTOK );
		mmdfstart();
	}
	else

		netreply("250 OK\r\n");

	numrecipients = 0;
}

islochost(them, from)
char *them, *from;
{
	int lo, lt;
	extern char *locfullmachine, *locfullname;

	if (locfullmachine == (char *)0)
		return(0);
	lo = strlen(locfullname);
	lt = strlen(them);
	return(
		/* return true if they claim to be us */
		lexequ(from, locfullname)
		/* and are one of our local machines */
	     && lt > lo
	     && them[lt - lo - 1] == '.'
	     && lexequ(&them[lt - lo], locfullname)
	);
}

/*
 *  Process the RCPT command  ("RCPT TO:<forward-path>")
 */
rcpt()
{
	register char *p;
	struct rp_bufstruct thereply;
	char    replybuf[256];
	int     len;

	/* parse destination arg */
	if( sender == (char *)0 ) {
		netreply("503 You must give a MAIL command first\r\n");
		return;
	} else if (arg == (char *)0 || !equal(arg, "to:", 3)) {
		netreply("501 No recipient named.\r\n");
		return;
	}
	p = index( arg, ':' ) + 1;
	p = addrfix( p );

	if (setjmp(timerest)) {
		netreply( "451 Mail system problem\r\n" );
		return;
	}

	s_alarm (DTIMEOUT);
	if( rp_isbad( mm_wadr( (char *)0, p ))) {
		if( rp_isbad( mm_rrply( &thereply, &len )))
			netreply( "451 Mail system problem\r\n" );
		else {
			sprintf (replybuf, "451 %s\r\n", thereply.rp_line);
			netreply (replybuf);
		}
	} else {
		if( rp_isbad( mm_rrply( &thereply, &len )))
			netreply("451 Mail system problem\r\n");
		else if( rp_gbval( thereply.rp_val ) == RP_BNO) {
			sprintf (replybuf, "550 %s\r\n", thereply.rp_line);
			netreply (replybuf);
		}
		else if( rp_gbval( thereply.rp_val ) == RP_BTNO) {
			sprintf (replybuf, "451 %s\r\n", thereply.rp_line);
			netreply (replybuf);
		}
		else {

			netreply("250 Recipient OK.\r\n");
			numrecipients++;


		}
	}
	s_alarm (0);

}

/*
 *      ADDRFIX()  --  This function takes the SMTP "path" and removes
 *      the leading and trailing "<>"'s which would make the address
 *      illegal to RFC822 mailers.  Note that although the spec states
 *      that the angle brackets are required, we will accept addresses
 *      without them.   (DPK@BRL, 4 Jan 83)
 */
char *
addrfix( addrp )
char *addrp;
{
	register char   *cp;

	if( cp = index( addrp, '<' )) {
		addrp = ++cp;
		if( cp = rindex( addrp, '>' ))
			*cp = 0;
	}
	compress (addrp, addrp);
#ifdef DEBUG
	ll_log( logptr, LLOGFTR, "addrfix(): '%s'", addrp );
#endif
	return( addrp );
}

/*
 *  Process the DATA command.  Send text to MMDF.
 */
data()
{
	register char *p;
	register char *bufPtr;
	time_t  tyme;
	int     errflg, werrflg;
	struct rp_bufstruct thereply;
	int     len, msglen;

	errflg = werrflg = msglen = 0;

	if (numrecipients == 0) {
		netreply("503 No recipients have been specified.\r\n");
		return;
	}

	if (setjmp(timerest)) {
		netreply( "451 Mail system problem\r\n" );
		return;
	}
	s_alarm (DTIMEOUT);
	if( rp_isbad(mm_waend())) {
		netreply("451 Unknown mail system trouble.\r\n");
		return;
	}
	s_alarm (0);

	netreply ("354 Enter Mail, end by a line with only '.'\r\n");

	dont_mung = 1;      /* tell getline only to drop cr */
#ifdef DEBUG
	
	ll_log( logptr, LLOGFTR, "... body of message ..." );
#endif
	while (1) {             /* forever */
		if ((p = getline()) == 0) {
			p = "\n***Sender closed connection***\n";
			mm_wtxt( p , strlen(p) );
			errflg++;
			break;
		}
		if (p == (char *)NOTOK) {
			p = "\n***Error on net connection***\n";
			mm_wtxt( p , strlen(p) );
			if (!errflg++)
				ll_log(logptr, LLOGTMP,
					"netread error from host %s", them);
			break;
		}

		/* are we done? */
		if (buf[0] == '.')
			if (buf[1] == '\n')
				break;          /* yep */
			else
				bufPtr = &buf[1];       /* skip leading . */
		else
			bufPtr = &buf[0];
		/* If write error occurs, stop writing but keep reading. */
		if (!werrflg) {
			if (setjmp(timerest)) {
				netreply( "451 Mail system problem\r\n" );
				return;
			}
			s_alarm (DTIMEOUT);
			msglen += (len = p-bufPtr);
			if( rp_isbad(mm_wtxt( bufPtr, len ))) {
				werrflg++;
				ll_log( logptr, LLOGTMP, "error from submit");
			}
			s_alarm (0);
		}
	}
	dont_mung = 0;  /* set getline to normal operation */
#ifdef DEBUG
	ll_log( logptr, LLOGBTR, "Finished receiving text." );
#endif

	if (werrflg) {
		netreply("451-Mail trouble (write error to mailsystem)\r\n");
		netreply("451 Please try again later.\r\n");
		byebye( 1 );
	}
	if (errflg) {
	    time (&tyme);
	    byebye(1);
	}

	if (setjmp(timerest)) {
		netreply( "451 Mail system problem\r\n" );
		return;
	}

	s_alarm (DTIMEOUT);
	if( rp_isbad(mm_wtend()) || rp_isbad( mm_rrply( &thereply, &len)))
		netreply("451 Unknown mail trouble, try later\r\n");
	else if( rp_isgood(thereply.rp_val)) {
		sprintf (buf, "250 %s\r\n", thereply.rp_line);
		netreply (buf);
		phs_msg(chanptr, numrecipients, (long) msglen);
	}
	else if( rp_gbval(thereply.rp_val) == RP_BNO) {
		sprintf (buf, "554 %s\r\n", thereply.rp_line);
		netreply (buf);
	}
	else {
		sprintf (buf, "451 %s\r\n", thereply.rp_line);
		netreply (buf);

	}
	s_alarm (0);

	sender = (char *) 0;
	numrecipients = 0;
}

/*
 *  Process the RSET command
 */
rset()
{
	mm_end( NOTOK );
	sender = (char *)0;
	mmdfstart();
	confirm();
}

mmdfstart()
{
	if( rp_isbad( mm_init() ) || rp_isbad( mm_sbinit() )) {
		ll_log( logptr, LLOGFAT, "can't reinitialize mail system" );
		netreply("421 Server can't initialize mail system (mmdf)\r\n");
		byebye( 2 );
	}
	numrecipients = 0;
}

/*
 *  handle the QUIT command
 */
quit()
{
	time_t  timenow;

	time (&timenow);
	sprintf (buf, "221 %s says goodbye to %s at %.19s.\r\n",
		us, them, ctime(&timenow));
	netreply(buf);
	byebye( 0 );
}

byebye( retval )
int retval;
{
  int wreturn = 0;

#ifdef DEBUG
  t_base=sys_milli();
#endif

  wreturn = tcpip_write(&chancap,"",0);
  ll_log(logptr,LLOGFTR,"wreturn = %d",wreturn);
  if (wreturn == 0) std_destroy(&chancap);
  if (retval == OK) {
    mm_sbend();
    phs_note(chanptr, PHS_REEND);
  }
  mm_end( retval == 0 ? OK : NOTOK );
#ifdef DEBUG
  printf("7: byebye: %d\n",sys_milli()-t_base);
  printf("8: ****smtpsrvr time: %d\n",sys_milli()-t_start);
#endif
  
  exit( retval );
}

/*
 *  Reply that the current command has been logged and noted
 */
confirm()
{
	netreply("250 OK\r\n");
}

/*
 *  Process the HELP command by giving a list of valid commands
 */
help()
{
	register i;
	register struct comarr *p;
	char    replybuf[256];

	netreply("214-The following commands are accepted:\r\n214-" );
	for(p = commands, i = 1; p->cmdname; p++, i++) {
		sprintf (replybuf, "%s%s", p->cmdname, ((i%10)?" ":"\r\n214-") );
		netreply (replybuf);
	}
	sprintf (replybuf, "\r\n214 Send complaints/bugs to:  %s\r\n", supportaddr);
	netreply (replybuf);
}

/*
 *  Send appropriate ascii responses over the network connection.
 */

netreply(instring)
char *instring;

{
char bufout[5000];
char *outptr;
int len;

	if (setjmp(timerest)) {
		byebye( 1 );
	}
	s_alarm (NTIMEOUT);

sprintf(bufout,"%s",instring);
	outptr= bufout;
	len= strlen(bufout);
	while(len)
	{
		err= tcpip_write(&chancap, outptr, len);
		if (err <= 0)
			break;
		outptr += err;
		len -= err;
	}
	if(err <= 0){
	        ll_log(logptr,LLOGFTR,"tcpip_write: %s\n",err_why(err));
		std_destroy(&chancap);
		s_alarm( 0 );
		ll_log( logptr, LLOGFST,
			"(netreply) error (%d) in writing [%s] ...",
			err, instring);
		byebye( 1 );
	}

	s_alarm( 0 );
#ifdef DEBUG
	ll_log( logptr, LLOGFTR, "%s", instring);
#endif /* DEBUG */
}

/*
 *      expn
 *
 *      handle the EXPN command  ("EXPN user@host")
 */
expn()
{

	if (arg == 0 || *arg == 0) {
		netreply("501 No argument supplied\r\n");
		return;
	}

	expn_count = 0;
	expn_str( addrfix(arg) );  /* feed fixed address into the expander */
	expn_dump();               /* dump the saved (last) address */
	return;
} 

expn_str( arg )
register char *arg;
{
	char    buf[LINESIZE];
	char	alstr[LINESIZE];
	char    comment[LINESIZE];
	register char    *p, *q;
	FILE    *fp;
	int	ret, gotalias, flags;
	Chan    *thechan;
	struct passwd	 *pw;

	gotalias=1;
	if ((ret = aliasfetch(TRUE, arg, buf, &flags)) != OK) {
		if (ret == MAYBE) {
			expn_save(MAYBE,"%s (Nameserver Timeout)",arg);
			return;
		}
		if ((pw = getpwmid (arg)) != (struct passwd *) NULL) {
			expn_save(OK,"%s <%s@%s>", pw->pw_gecos,
				pw->pw_name,us);
			return;
		}
		strcpy(buf, arg);
		gotalias--;
	}

	strcpy(alstr, arg);

	/* just say OK if it is a private alias */
	if (gotalias && (flags & AL_PUBLIC) != AL_PUBLIC) {
		expn_save(OK,"<%s@%s>",alstr,us);
		return;
	}

	/* check for a simple comma-separated list of addresses */
	if ((p = index(buf, ',')) != 0 && strindex (":include:", buf) < 0 &&
	    index (buf, '<') == 0 && index (buf, '|') == 0 && gotalias)
	{
		/* q is start of substring; it lags behind p */
		for (q=buf; p != 0; q=p, p=index(p, ',')) {
			*p++ = '\0';  /* null the comma */
			expn_str(q);
		}
		return;
	}

	/* check for a simple RHS with no @'s and no /'s (e.g. foo:bar) */
	if (index (buf, '/') == 0)
	{
		/* check for a simple RHS (e.g. foo:bar) */
		if ((p = index (buf, '@')) == 0)
		{
			if( buf[0] == '~' ) {
				if ((pw=getpwmid (buf[1])) != (struct passwd *) NULL) {
					expn_save(OK,"%s <%s@%s>", pw->pw_gecos,
						pw->pw_name,us);
					return;
				}
				expn_save(NOTOK,"%s (Unknown address)",arg);
				return;
			}
			if (gotalias)
				return(expn_str(buf));  /* recurse */
			else {
				expn_save(NOTOK,"%s (Unknown address)",arg);
				return;
			}

		}

		/* check for aliases of the form: user@domain */
		*p++ = '\0';
		thechan=ch_h2chan(p,1);
		switch ((int) thechan) {
			case OK:
				return(expn_str(buf));  /* user@US -- recurse */
			case NOTOK:
			case MAYBE:
				break;
			default:
				/* check if list/bboard type channel */
				if (isstr(thechan->ch_host) &&
				    (thechan = ch_h2chan(thechan->ch_host,1))
								== (Chan *) OK)
					return(expn_str(buf));  /* recurse */
		}
		if (thechan == (Chan *) MAYBE) {
			expn_save(MAYBE,"%s@%s (Nameserver Timeout)",buf,p);
			return;
		}
		expn_save(OK,"<%s@%s>",buf,p);
		return;
	}

	if (!gotalias) {
		expn_save(NOTOK,"%s (Unknown Address)", arg);
		return;
	}

	/* Assume if multiple entries, that only the first one is used. */
	if ((q = index (buf, ',')) != 0)
		*q = '\0';

	/* check for aliases of the form: [user]|program */
	if ((q = index (buf, '|')) != 0) {
		*q++ = '\0';
		expn_save (OK,"%s@%s (Mail piped into process: %s)", 
			  alstr, us, q);
		return;
	}

	if ((q = index (buf, '/')) == 0) {
	        expn_save(NOTOK,"%s (Bad format for alias is %s)",alstr,buf);
		return;
	}

	/* check for < and :include: */
	if (index (buf, '<') != 0 || strindex (":include:", buf) >= 0) {
		if ((p = index (buf, '@')) != 0) {
			*p++ = '\0';
			if (ch_h2chan (p, 1)  != (Chan *) OK) {  /* not local */
				expn_save (OK,"<%s@%s>",buf,p);
				return;
			}

		}
		if ((fp = fopen (q, "r")) == NULL) {
			expn_save (NOTOK,"%s@%s (Unable to open file %s)",
				  alstr,us,q);
			return;
		}
		while (fgets (buf, LINESIZE, fp) != NULL) {
			*(buf+strlen(buf)-1) = '\0';
			expn_str(buf);
		}
		fclose (fp);
		return;
	}

	/* assume alias of the form:  [user]/file */
	*q++ = '\0';
	expn_save (OK,"%s@%s (Mail filed into %s)",
			alstr, us, q);
	return;


}

expn_save(code,pattern,a1,a2,a3,a4,a5,a6,a7)
int code;
char *pattern,
     *a1,*a2,*a3,*a4,*a5,*a6,*a7;
{
	if (expn_count > 0) {
		sprintf(buf,"250-%s\r\n",saveaddr);
		netreply(buf);
	}

	sprintf(saveaddr,pattern,a1,a2,a3,a4,a5,a6,a7);
	if (expn_count == 0)
		savecode=code;
	else
		if (code != savecode)
			savecode=OK;
	expn_count++;
}

expn_dump()
{

	if (expn_count == 1 && savecode == NOTOK)
		sprintf(buf,"550 %s\r\n", saveaddr);
	else
		sprintf(buf,"250 %s\r\n", saveaddr);

	netreply(buf);
}

/*
 *      vrfy
 *
 *      handle the VRFY command  ("VRFY user@host")
 */
vrfy()
{
	register int fd;
	register char *cp;
	char linebuf[LINESIZE];
	char replybuf[LINESIZE];
	struct rp_bufstruct thereply;
	int len;

	if (arg == 0 || *arg == 0) {
		netreply("501 No argument supplied\r\n");
		return;
	}

	if (!vrfy_child) {

		/* Start up the verifying child */

		if (pipe (vrfy_p2c) < OK || pipe (vrfy_c2p) < OK) {
			netreply("550 Mail system problem1.\r\n");
			return;
		}

		switch (vrfy_child = fork()) {

		    case NOTOK:
			ll_log(logptr, LLOGFST, "Fork of vrfy child failed.");
			netreply("550 Mail system problem2.\r\n");
			return;

		    default:
			/* parent */
			vrfy_out = fdopen(vrfy_p2c[1],"w");
			vrfy_in  = fdopen(vrfy_c2p[0],"r");
			break;

		    case 0:
			/* child */
#ifndef NODUP2
			dup2(vrfy_p2c[0],0);
			dup2(vrfy_c2p[1],1);
#else
			(void) close(0);
			(void) close(1);
#ifndef NOFCNTL
			fcntl(vrfy_p2c[0], F_DUPFD, 0);
			fcntl(vrfy_c2p[1], F_DUPFD, 1);
#else
			/* something else */
#endif
#endif
			for (fd = 2; fd < numfds; fd++)
				(void)close(fd);

			if (rp_isbad(mm_init()) || rp_isbad(mm_sbinit()) ||
			    rp_isbad(mm_winit ((char *)0, "vmr", (char *)0)) ||
			    rp_isbad(mm_rrply( &thereply, &len)) ||
			    rp_isbad(thereply.rp_val)) {
				ll_log(logptr, LLOGFST, 
					"Vrfy child couldn't start up submit.");
				exit(1);
			}

			while (fgets (linebuf, LINESIZE, stdin)) {
				if (cp = rindex(linebuf, '\n'))
					*cp-- = 0;
				verify(linebuf);
			}

			mm_end(NOTOK);
			exit(0);

		}

	}

	if (setjmp(timerest)) {
		ll_log(logptr,LLOGFST,"Timeout waiting for vrfy child.");
		netreply("550 Mail system problem.\r\n");
		vrfy_kill();
		return;
	}

	s_alarm(DTIMEOUT);
	fprintf(vrfy_out,"%s\n",arg);
	fflush(vrfy_out);
	if (ferror(vrfy_out)) {
		ll_log(logptr,LLOGFST,"Unable to write to vrfy child.");
		vrfy_kill();
		netreply("550 Mail system problem.\r\n");
		return;
	}
	if (fgets (linebuf, LINESIZE, vrfy_in) == NULL) {
		ll_log(logptr,LLOGFST,"Unable to read from vrfy child.");
		vrfy_kill();
		netreply("550 Mail system problem.\r\n");
		return;
	}
	s_alarm(0);

	if (cp = rindex(linebuf, '\n'))
		*cp-- = 0;
	sprintf(replybuf,"%s\r\n",linebuf);
	netreply(replybuf);

	return;
}

verify(p)
char *p;
{
	register char *l, *r;
	struct rp_bufstruct thereply;
	char replybuf[256];
	int len;

	if (setjmp(timerest)) {
		ll_log(logptr,LLOGFST,
			         "Timeout in vrfy child waiting for submit.");
	        tcp_exit(1);
	}
	s_alarm (DTIMEOUT);
	if( rp_isbad( mm_wadr( (char *)0, p ))) {
		if( rp_isbad( mm_rrply( &thereply, &len ))) {
			ll_log(logptr,LLOGFST,
			      "Error in vrfy child reading reply from submit.");
		       tcp_exit(1);
		} else {
			sprintf (replybuf, "550 %s\r\n", thereply.rp_line);
			vrfyreply (replybuf);
		}
	} else {
		if( rp_isbad( mm_rrply( &thereply, &len ))) {
			ll_log(logptr,LLOGFST,
			     "Error in vrfy child reading reply from submit.");
			tcp_exit(1);
		} else if (rp_isbad(thereply.rp_val)) {
			sprintf (replybuf, "550 %s\r\n", thereply.rp_line);
			vrfyreply (replybuf);
		} else {
			if ((l=index(thereply.rp_line, '"')) &&
			    (r=rindex(thereply.rp_line, '"')) &&
			    (l != r) ) {
				*l = '<';
			        *r = '>';
			}
			if (l && r && !index(l,'@')) {
			    *l++ = '\0';
			    *r++ = '\0';
			    sprintf (replybuf, "250 %s<%s@%s>%s\r\n",
 				   thereply.rp_line, l, us, r);
			} else
			    sprintf (replybuf, "250 %s\r\n", thereply.rp_line);
			vrfyreply (replybuf);
		}
	}
	s_alarm (0);

	return;
}

vrfyreply(string)
char *string;
{
	char *charptr;
	int len, err;

	if (setjmp(timerest)) {
		tcp_exit(1);
	}
	s_alarm (NTIMEOUT);
	charptr= string;
	len= strlen(string);
	while (len)
	{
		err= tcpip_write(&chancap, charptr, len);
		if (err <= 0)
			break;
		charptr += err;
		len -= err;
	}
	if(err <= 0) {
		s_alarm(0);
		ll_log( logptr, LLOGFST,
			"(vrfyreply) error (%d) in writing [%s] ...",
			errno, string);
		tcp_exit(1);
	}
	s_alarm(0);
#ifdef DEBUG
	ll_log( logptr, LLOGFTR, "child writing: %s", string);
#endif /* DEBUG */
}

vrfy_kill()
{
	ll_log( logptr, LLOGFAT, "Killing vrfy child.");

	if (vrfy_child) {
		kill (vrfy_child, 9);
		vrfy_child = 0;
	}

	if (vrfy_in != (FILE *) EOF && vrfy_in != NULL) {
ll_log(logptr,LLOGFAT,"Killing vrfy_in");
		(void) close (fileno (vrfy_in));
		fclose(vrfy_in);
	}

	if (vrfy_out != (FILE *) EOF && vrfy_out != NULL) {
ll_log(logptr,LLOGFAT,"Killing vrfy_out");
		(void) close (fileno (vrfy_out));
		fclose(vrfy_out);
	}

	vrfy_in = vrfy_out = NULL;

	return;
}


tcp_exit(retval)
int retval;
{
  int wreturn = 0;
  
  wreturn = tcpip_write(&chancap,"",0);
  ll_log(logptr,LLOGFTR,"wreturn = %d",wreturn);
  if (wreturn == 0) std_destroy(&chancap);
  exit( retval );
}
