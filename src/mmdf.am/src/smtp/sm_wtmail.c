/*	@(#)sm_wtmail.c	1.3	96/03/05 10:56:33 */
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
 *
 *     This program module was developed as part of the University
 *     of Delaware's Multi-Channel Memo Distribution Facility (MMDF).
 *
 *     Acquisition, use, and distribution of this module and its listings
 *     are subject restricted to the terms of a license agreement.
 *     Documents describing systems using this module must cite its source.
 *
 *     The above statements must be retained with all copies of this
 *     program and may not be removed without the consent of the
 *     University of Delaware.
 *     
 *
 *     version  -1    David H. Crocker    March   1979
 *     version   0    David H. Crocker    April   1980
 *     version  v7    David H. Crocker    May     1981
 *     version   1    David H. Crocker    October 1981
 *
 */
/*                  MAIL-COMMANDS FOR SMTP MAIL                      */

/*  Oct 82 Dave Crocker   derived from arpanet ftp/ncp channel
 *
 *      -------------------------------------------------
 *
 *  Feb 83 Doug Kingston  major rewrite, some fragments kept
 *
 *      -------------------------------------------------
 *  May 92 Philip Homburg Changes for Amoeba.
 *
 */

#include "util.h"
#include "mmdf.h"
#include "ch.h"
#include <signal.h>
#include "phs.h"
#include "ap.h"
#include "dm.h"
#include "smtp.h"
#if AMOEBA
#define bufptr	bufptr_t

#include <amoeba.h>
#include <module/stdcmd.h>
#include <cmdreg.h>
#include <stderr.h>
#include <server/ip/hton.h>
#include <server/ip/tcpip.h>
#include <server/ip/types.h>
#include <server/ip/gen/in.h>
#include <server/ip/gen/netdb.h>
#include <server/ip/gen/socket.h>

#undef bufptr
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

extern LLog     *logptr;
extern Chan     *chanptr;
extern char     *blt();
extern char     *strdup();
extern char     *strncpy ();

LOCFUN sm_rrec();

char    *sm_curname;

struct sm_rstruct sm_rp;            /* save last reply obtained           */
LOCVAR Chan     *sm_chptr;      /* structure for channel that we are  */
#if AMOEBA
static capability tcp_chan;
#else
FILE    *sm_rfp, *sm_wfp;
#endif
LOCVAR char     sm_rnotext[] = "No reply text given";
LOCVAR  char    netobuf[BUFSIZ];
LOCVAR  char    netibuf[BUFSIZ];

/**/

#if AMOEBA

static char cap_wbuf[1024];
static char *cap_wptr= cap_wbuf;
static errstat cap_err= STD_OK;
static int cap_eof_flag= 0;

errstat cap_putc(c)
int c;
{
	char *cap_ptr;
	bufsize bs;

	*cap_wptr++= c;
	if (c != '\n' && cap_wptr < cap_wbuf+sizeof(cap_wbuf))
		return STD_OK;
	for (cap_ptr= cap_wbuf; cap_ptr < cap_wptr; )
	{
		bs= tcpip_write(&tcp_chan, cap_ptr, cap_wptr-cap_ptr);
		if (ERR_STATUS(bs))
		{
			cap_err= ERR_CONVERT(bs);
			return cap_err;
		}
		if (bs == 0)
		{
			cap_eof_flag= 1;
			return STD_NOSPACE;	/* We have to use something */
		}
		cap_ptr += bs;
	}
	cap_wptr= cap_wbuf;
}

errstat cap_puts(str)
char *str;
{
	errstat am_err;

	while (*str)
	{
		am_err= cap_putc(*str++);
		if (am_err != STD_OK)
			return am_err;
	}
	return STD_OK;
}

errstat cap_printf(str)
char *str;
{
	errstat am_err;

	am_err= cap_puts(str);
	if (am_err != STD_OK)
		return am_err;
	am_err= cap_puts("\r\n");
	return am_err;
}


char cap_rbuf[1024];
char *cap_rbptr= cap_rbuf;
char *cap_reptr= cap_rbuf;

errstat cap_gets(bf, siz)
char *bf;
int siz;
{
	bufsize bs;
	int got_eoln;

	if (siz == 0)
		return STD_NOSPACE;
	for (;;)
	{
		if (cap_reptr == cap_rbuf)
		{
			bs= tcpip_read(&tcp_chan, cap_rbuf, sizeof(cap_rbuf));
			if (ERR_STATUS(bs))
			{
				cap_err= ERR_CONVERT(bs);
				break;
			}
			if (bs == 0)
			{
				cap_eof_flag= 1;
				break;
			}
			cap_reptr += bs;
		}
		for(; siz > 1 && cap_rbptr < cap_reptr; )
		{
			siz--;
			if ((*bf++= *cap_rbptr++) == '\n')
			{
				got_eoln= 1;
				break;
			}
		}
		if (cap_rbptr == cap_reptr)
			cap_rbptr= cap_reptr= cap_rbuf;
		if (siz == 1 || got_eoln)
			break;
	}
	*bf= '\0';
	if (cap_eof_flag)
		return STD_NOSPACE;
	if (cap_err != STD_OK)
		return cap_err;
	return STD_OK;
}

int cap_getc()
{
	char bf[2];
	errstat am_err;

	am_err= cap_gets(bf, sizeof(bf));
	if (am_err != STD_OK)
		return -1;
	else
		return *bf;
}

int cap_eof()
{
	return cap_eof_flag;
}

int cap_error()
{
	return cap_err != STD_OK;
}
#endif

sm_wfrom (sender)
char    *sender;
{
	char    linebuf[LINESIZE];

	sprintf (linebuf, "MAIL FROM:<%s>", sender);
	if (rp_isbad (sm_cmd (linebuf, SM_STIME)))
	    return (RP_DHST);

	switch( sm_rp.sm_rval ) {
	    case 250:
		break;          /* We're off and running! */

	    case 500:
	    case 501:
	    case 552:
		return( sm_rp.sm_rval = RP_PARM );

	    case 421:
	    case 450:
	    case 451:
	    case 452:
		return( sm_rp.sm_rval = RP_AGN);

	    default:
		return( sm_rp.sm_rval = RP_BHST);
	}
	return( RP_OK );
}

sm_wto (host, adr)         /* send one address spec to local     */
char    host[];                   /* "next" location part of address    */
char    adr[];                    /* rest of address                    */
{
    char linebuf[LINESIZE];

#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "sm_wto(%s, %s)", host, adr);
#endif

    sprintf (linebuf, "RCPT TO:<%s>", adr);
    if (rp_isbad (sm_cmd (linebuf, SM_TTIME)))
	return (RP_DHST);

    switch (sm_rp.sm_rval)
    {
	case 250:
	case 251:
	    sm_rp.sm_rval = RP_AOK;
	    break;

	case 421:
	case 450:
	case 451:
	case 452:
	    sm_rp.sm_rval = RP_AGN;
	    break;

	case 550:
	case 551:
	case 552:
	case 553:
	case 554:               /* BOGUS: sendmail is out of spec! */
	    sm_rp.sm_rval = RP_USER;
	    break;

	case 500:
	case 501:
	    sm_rp.sm_rval = RP_PARM;
	    break;

	default:
	    sm_rp.sm_rval = RP_RPLY;
    }
    return (sm_rp.sm_rval);
}

sm_init (curchan)                 /* session initialization             */
Chan *curchan;                    /* name of channel                    */
{
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "sm_init ()");
#endif
    sm_chptr = curchan;
    phs_note (sm_chptr, PHS_CNSTRT);
    return (RP_OK);               /* generally, a no-op                 */
}

/**/

LOCFUN
sm_irdrply ()             /* get net reply & stuff into sm_rp   */
{
    static char sep[] = "; ";     /* for sticking multi-lines together  */
    short     len,
	    tmpreply,
	    retval;
    char    linebuf[LINESIZE];
    char    tmpmore;
    register char  *linestrt;     /* to bypass bad initial chars in buf */
    register short    i;
    register char   more;         /* are there continuation lines?      */

#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "sm_irdrply ()");
#endif

newrply: 
    for (more = FALSE, sm_rp.sm_rgot = FALSE, sm_rp.sm_rlen = 0;
	    rp_isgood (retval = sm_rrec (linebuf, &len));)
    {                             /* 1st col in linebuf gets reply code */
	printx("<-(%s)\r\n", linebuf);
	fflush( stdout );

	for (linestrt = linebuf;  /* skip leading baddies, probably     */
		len > 0 &&        /*  from a lousy Multics              */
		    (!isascii ((char) *linestrt) ||
			!isdigit ((char) *linestrt));
		linestrt++, len--);

	tmpmore = FALSE;          /* start fresh                        */
	tmpreply = atoi (linestrt);
	blt (linestrt, sm_rp.sm_rstr, 3);       /* Grab reply code      */
	if ((len -= 3) > 0)
	{
	    linestrt += 3;
	    if (len > 0 && *linestrt == '-')
	    {
		tmpmore = TRUE;
		linestrt++;
		if (--len > 0)
		    for (; len > 0 && isspace (*linestrt); linestrt++, len--);
	    }
	}

	if (more)                 /* save reply value from 1st line     */
	{                         /* we at end of continued reply?      */
	    if (tmpreply != sm_rp.sm_rval || tmpmore)
		continue;
	    more = FALSE;         /* end of continuation                */
	}
	else                      /* not in continuation state          */
	{
	    sm_rp.sm_rval = tmpreply;
	    more = tmpmore;   /* more lines to follow?              */

	    if (len <= 0)
	    {                     /* fake it, if no text given          */
		blt (sm_rnotext, linestrt = linebuf,
		       (sizeof sm_rnotext) - 1);
		len = (sizeof sm_rnotext) - 1;
	    }
	}

	if ((i = min (len, (LINESIZE - 1) - sm_rp.sm_rlen)) > 0)
	{                         /* if room left, save the human text  */
	    blt (linestrt, &sm_rp.sm_rstr[sm_rp.sm_rlen], i);
	    sm_rp.sm_rlen += i;
	    if (more && sm_rp.sm_rlen < (LINESIZE - 4))
	    {                     /* put a separator between lines      */
		blt (sep, &(sm_rp.sm_rstr[sm_rp.sm_rlen]), (sizeof sep) - 1);
		sm_rp.sm_rlen += (sizeof sep) - 1;
	    }
	}
#ifdef DEBUG
	else
	    ll_log (logptr, LLOGFTR, "skipping");
#endif

	if (!more)
	{
#ifdef DEBUG
	    ll_log (logptr, LLOGBTR, "(%u)%s", sm_rp.sm_rval, sm_rp.sm_rstr);
#endif
	    if (sm_rp.sm_rval < 100)
		goto newrply;     /* skip info messages                 */

	    sm_rp.sm_rgot = TRUE;
	    return (RP_OK);
	}
    }
    return (retval);              /* error return                       */
}

sm_rpcpy (rp, len)           /* return arpanet command reply       */
RP_Buf *rp;      /* where to put it                    */
short    *len;                      /* its length                         */
{
    if( sm_rp.sm_rgot == FALSE )
	return( RP_RPLY );

    rp -> rp_val = sm_rp.sm_rval;
    *len = sm_rp.sm_rlen;
    blt (sm_rp.sm_rstr, rp -> rp_line, sm_rp.sm_rlen + 1);
    sm_rp.sm_rgot = FALSE;        /* flag as empty                      */

    return (RP_OK);
}
/**/

LOCFUN
sm_rrec (linebuf, len)   /* read a reply record from net       */
char   *linebuf;                  /* where to stuff text                */
short    *len;                      /* where to stuff length              */
{
    extern int errno;
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "sm_rrec ()");
#endif
#if AMOEBA
    errstat am_err;
#endif

    *len = 0;                     /* for clean logging if nothing read  */
    linebuf[0] = '\0';

#if AMOEBA
    am_err= cap_gets (linebuf, LINESIZE);
    if (am_err != STD_OK) {
	printx("sm_rrec: cap_gets returns %s\n",  tcpip_why(am_err));
    }
#else
    if (fgets (linebuf, LINESIZE, sm_rfp) == NULL) {
	printx("sm_rrec: fgets returns NULL, errno = %d\n",  errno);
    }
#endif
    *len = strlen (linebuf);

#if AMOEBA
    if (cap_error (&tcp_chan) || cap_eof (&tcp_chan))
#else
    if (ferror (sm_rfp) || feof (sm_rfp))
#endif
    {                             /* error or unexpected eof            */
	printx ("sm_rrec: problem reading from net, ");
#if AMOEBA
	printx("netread:  ret=%d, ", *len);
#else
	printx("netread:  ret=%d, fd=%d, ", *len, fileno (sm_rfp));
#endif
	fflush (stdout);
#if AMOEBA
	ll_err(logptr, LLOGTMP, "netread:  ret=%d", *len);
#else
	ll_err(logptr, LLOGTMP, "netread:  ret=%d, fd=%d",
		*len, fileno (sm_rfp));
#endif
	sm_nclose (NOTOK);         /* since it won't work anymore        */
	return (RP_BHST);
    }
    if (linebuf[*len - 1] != '\n')
    {
	ll_log (logptr, LLOGTMP, "net input overflow");
#if AMOEBA
	while (cap_getc () != '\n'
		&& !cap_error (&tcp_chan) && !cap_eof (&tcp_chan));
#else
	while (getc (sm_rfp) != '\n'
		&& !ferror (sm_rfp) && !feof (sm_rfp));
#endif
    }
    else
	if (linebuf[*len - 2] == '\r')
	    *len -= 1;            /* get rid of crlf or just lf         */

    linebuf[*len - 1] = '\0';
#ifdef DEBUG
    ll_log (logptr, LLOGFTR, "(%u)'%s'", *len, linebuf);
#endif
    return (RP_OK);
}
/**/

sm_cmd (cmd, time)              /* Send a command */
char    *cmd;
int     time;                   /* Max time for sending and getting reply */
{
    short     retval;
    extern char *sys_errlist[];
    extern int errno;
#if AMOEBA
    errstat am_err;
#endif

    ll_log (logptr, LLOGPTR, "sm_cmd (%s)", cmd);

    printx("->(%s)\r\n", cmd);
    fflush( stdout );

    if (setjmp(timerest)) {
	printx("cmd = '%s'; errno = %d\n", cmd, sys_errlist[errno]);
	ll_log (logptr, LLOGGEN,
	    "sm_cmd(): timed out after %d sec, cmd %.10s",time,cmd);
	sm_nclose (NOTOK);
	return (sm_rp.sm_rval = RP_DHST);
    }
    s_alarm( (unsigned) time );
#if AMOEBA
    am_err= cap_printf(cmd);
    if (am_err != STD_OK) 
    {
	printx("cap_printf returned %s\n", tcpip_why(am_err));
    }
#else
    if (fprintf(sm_wfp, "%s\r\n", cmd) == EOF) 
    {
    /* if (fwrite (cmd, sizeof (char), strlen(cmd), sm_wfp) == 0) { */
	printx("fprintf returned EOF, errno=%d\n", errno);
    }
    if (fflush (sm_wfp) == EOF) {
	printx("first fflush returned EOF, errno = %d\n", errno);
    }
    /* fputs ("\r\n", sm_wfp);
     * if (fflush (sm_wfp) == EOF) {
 * 	printx("second fflush returned EOF, errno = %d\n", errno);
   *   }
  */
#endif

#if AMOEBA
    if (cap_error (&tcp_chan))
#else
    if (ferror (sm_wfp))
#endif
    {
	s_alarm ( 0 );
	ll_log (logptr, LLOGGEN, "sm_cmd(): host died?");
	sm_nclose (NOTOK);
	return (sm_rp.sm_rval = RP_DHST);
    }

    if (rp_isbad (retval = sm_irdrply ())) {
	s_alarm( 0 );
	return( sm_rp.sm_rval = retval );
    }
    s_alarm( 0 );
    return (RP_OK);
}
/**/

sm_wstm (buf, len)            /* write some message text out        */
char    *buf;                 /* what to write                      */
register int    len;              /* how long it is                     */
{
    static char lastchar = 0;
    short     retval;
    register char  *bufptr;
    register char   newline;

#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "sm_wstm () (%u)'%s'", len, buf ? buf : "");
#endif

    if (buf == 0 && len == 0)
    {                             /* end of text                        */
	if (lastchar != '\n')     /* make sure it ends cleanly          */
#if AMOEBA
	    cap_puts ("\r\n");
#else
	    fputs ("\r\n", sm_wfp);
#endif
#if AMOEBA
	if (cap_error (&tcp_chan))
#else
	if (ferror (sm_wfp))
#endif
	    return (RP_DHST);
	lastchar = 0;             /* reset for next message             */
	retval = RP_OK;
    }
    else
    {
	newline = (lastchar == '\n') ? TRUE : FALSE;
	for (bufptr = buf; len--; bufptr++)
	{
	    switch (*bufptr)      /* cycle through the buffer           */
	    {
		case '\n':        /* Telnet requires crlf               */
		    newline = TRUE;
#if AMOEBA
		    cap_putc ('\r');
#else
		    putc ('\r', sm_wfp);
#endif
		    break;

		case '.':         /* Insert extra period at beginning   */
		    if (newline)
#if AMOEBA
			cap_putc ('.');
#else
			putc ('.', sm_wfp);
#endif
				  /* DROP ON THROUGH                    */
		default: 
		    newline = FALSE;
	    }
#if AMOEBA
	    cap_putc ((lastchar = *bufptr));
#else
	    putc ((lastchar = *bufptr), sm_wfp);
#endif
#if AMOEBA
	    if (cap_error (&tcp_chan))
#else
	    if (ferror (sm_wfp))
#endif
		return (RP_DHST);
				  /* finally send the data character    */
	}
#if AMOEBA
	retval = cap_error(&tcp_chan) ? RP_DHST : RP_OK;
#else
	retval = ferror(sm_wfp) ? RP_DHST : RP_OK;
#endif
    }

    return (retval);
}

/**/

union Haddru {
	long hnum;
	char hbyte[4];
};

sm_hostid (hostnam, addr, first)  /* addresses to try if sending to hostnam */
char    *hostnam;                 /* name of host */
union Haddru *addr;
int	first;			  /* first try on this name ?*/
{
    int     argc;
    register long    n;
    char   *argv[20];
    char    numstr[50];
    int     rval;
    int     tlookup=1;		/* used table lookup? */
    struct hostent *hp;

#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "sm_gethostid (%s)", hostnam);
#endif

    for (;;) 
    {
	/* look for [x.x.x.x] format -- don't need table lookup */

	if (hostnam[0] == '[')
	{
	    tlookup = 0;
	    (void) strncpy(numstr,hostnam,sizeof(numstr)-1);
	}
	else if ((rval=tb_k2val(sm_chptr->ch_table,first,hostnam,numstr))!=OK)
	{
	    /* No such host */
	    if (first)
		ll_log (logptr, LLOGTMP, "channel '%s' unknown host '%s'",
			sm_chptr -> ch_name, hostnam);

	    if (rval == MAYBE)
		return(RP_NS);

	    return(RP_BHST);
	}

	ll_log ( logptr, LLOGFTR, "Trying to break down %s", numstr) ;

	/* watch out for quoted and bracketed strings */
	if (numstr[0] == '"' || numstr[0] == '[') {
	    char buf[50];
	    (void) strcpy(buf, numstr+1);
	    (void) strcpy(numstr, buf);
	    if (numstr[strlen(numstr) - 2] == '"' || numstr[strlen(numstr) - 2] == ']')
	        numstr[strlen(numstr) - 2] = 0;
	}

	/* what form is hostnum in? */
	argc = 1;
	argv[0] = numstr;
	if ('0' <= numstr[0] && numstr[0] <= '9') {
	    /* dot-separated */
	    argc = cstr2arg (numstr, 20, argv, '.');
	}
	ll_log ( logptr, LLOGFTR, "%d fields, '%s'", argc, argv[0] );



	switch (argc)               /* what form is hostnum in?             */
	{
	    case 4:                 /* dot-separated                        */
		n = atoi (argv[0]);
		n = (n<<8) | atoi (argv[1]);
		n = (n<<8) | atoi (argv[2]);
		addr->hnum = (n<<8) | atoi (argv[3]);
		return(RP_OK);
		break;

            case 1:                 /* nickname                             */
                hp = gethostbyname(argv[0]);
                if (hp == 0) {
                    ll_log(logptr, LLOGTMP, "channel '%s' unknown nickname '%s'",
                                        sm_chptr->ch_name, argv[0]);
                    return(RP_BHST);
                }
                if (hp->h_addrtype == AF_INET && hp->h_length == 4) {
                    n = hp->h_addr[0];
                    n = (n<<8) | hp->h_addr[1] & 0xFF;
                    n = (n<<8) | hp->h_addr[2] & 0xFF;
                    addr->hnum = (n<<8) | hp->h_addr[3] & 0xFF;
                    return(RP_OK);
                } else {
                    ll_log(logptr, LLOGTMP, "channel '%s' bad address for '%s'",
                                        sm_chptr->ch_name, argv[0]);
                    return(RP_BHST);
                }


	    default:
		ll_log (logptr, LLOGTMP,
				"channel '%s' host %s' has bad address format",
				sm_chptr -> ch_name, hostnam);
		if (!tlookup)
		{
		    /* bad hostname in brackets */
		    return(RP_NO);
		}
		break;
	}
    } /* end for */
    
    /* NOTREACHED */
}

sm_nopen( hostnam )
char    *hostnam;
{
#if AMOEBA
#else
    Pip     fds;
#endif
    short   retval;
    char    linebuf[LINESIZE];
    union   Haddru haddr;
    unsigned atime;
    int     first;
    int	    rval;
    extern char *inet_ntoa();
#if AMOEBA
    ipaddr_t haddr1;
#else
    struct  in_addr haddr1;
#endif

    ll_log (logptr, LLOGPTR, "[ %s ]", hostnam);

    printx ("trying...\n");
    fflush (stdout);
    first = 1;
    atime = SM_ATIME;

    /* keep coming here until connected or no more addresses */
retry:

    if ((rval = sm_hostid(hostnam, &haddr, first)) != RP_OK)
    {
	switch (rval)
	{
	    case RP_NS:
		printx("\tno answer from nameserver\n");
		break;

	    case RP_NO:
		printx("\tbad hostname format\n");
		break;

	    default:
		/* some non-fatal error */
		rval = RP_BHST;
		break;
	}
	fflush(stdout);
	return(rval);
    }

    if (first)
	first = 0;

#if AMOEBA
    haddr1 = htonl(haddr.hnum);
#else
    haddr1.s_addr = htonl(haddr.hnum);
#endif
#ifdef DEBUG
    ll_log (logptr, LLOGFTR, "trying %s",inet_ntoa(haddr1));
#endif

    /* tell them who we are trying */
    printx("\tconnecting to [%s]...",inet_ntoa(haddr1));
    fflush(stdout);

    /* SMTP is on socket 25 */
    retval = tc_uicp (haddr.hnum, 25L, SM_OTIME, &tcp_chan);

    if (retval != RP_OK)
    {
	/* common event, so LLOGGEN (not TMP) */
        if (retval == RP_TIME) {
            ll_err (logptr, LLOGGEN, "%s (%8lx) open timeout", hostnam, haddr.hnum);
            printx (" timeout...\n");
        } else {
            ll_err (logptr, LLOGGEN, "%s (%8lx) no open", hostnam, haddr.hnum);
            printx (" can't...\n");
        }
	fflush (stdout);
	goto retry;	/* can't reach -- try someone else */
    }
    else
    {
	ll_log(logptr, LLOGGEN,"sending to %s via %s",
		    hostnam,inet_ntoa(haddr1));
#ifdef DEBUG
	ll_log (logptr, LLOGFTR,
		    "fdr = %d,fdw = %d", fds.pip.prd, fds.pip.pwrt);
#endif
    }

    sm_curname = strdup(hostnam);
    phs_note (sm_chptr, PHS_CNGOT);
#if AMOEBA
    /* We use tcp_chan directly. */
#else
    if ((sm_rfp = fdopen (fds.pip.prd, "r")) == NULL ||
	(sm_wfp = fdopen (fds.pip.pwrt, "w")) == NULL) {
	printx (" can't fdopen!\n");
	fflush(stdout);
	return (RP_LIO);	/* new address won't fix this problem */
    }
#endif
    printx (" open.\n");
    fflush (stdout);

#if AMOEBA
    /* No buffering. */
#else
    setbuf (sm_wfp, netobuf);
    setbuf (sm_rfp, netibuf);
#endif

    if (setjmp(timerest)) {
	sm_nclose (NOTOK);
	goto retry;		/* too slow, try someone else */
    }
    s_alarm (atime);

    atime -= SM_ATINC;
    if (atime < SM_ATMIN)
	atime = SM_ATMIN;

    if (rp_isbad (retval = sm_irdrply ())) {
	s_alarm (0);
	sm_nclose (NOTOK);
	goto retry;		/* problem reading -- try someone else */
    }
    s_alarm (0);

    if( sm_rp.sm_rval != 220 )
    {
	sm_nclose (NOTOK);
	goto retry;
    }

    if (sm_chptr -> ch_confstr)
	sprintf (linebuf, "HELO %s", sm_chptr -> ch_confstr);
    else
	sprintf (linebuf, "HELO %s.%s", sm_chptr -> ch_lname,
				    sm_chptr -> ch_ldomain);
    if (rp_isbad (sm_cmd( linebuf, SM_HTIME )) || sm_rp.sm_rval != 250 ) {
	sm_nclose (NOTOK);
	goto retry;		/* try more intelligent host? */
    }
    return (RP_OK);
}
/**/

sm_nclose (type)                /* end current connection             */
short     type;                 /* clean or dirty ending              */
{
	if (type == OK) {
		sm_cmd ("QUIT", SM_QTIME);
	} else {
		printx ("\r\nDropping connection\r\n");
		fflush (stdout);
	}
	if (sm_curname)
		free(sm_curname);
	sm_curname = 0;
	if (setjmp(timerest)) {
		return;
	}
	s_alarm (15);
#if AMOEBA
	std_destroy(&tcp_chan);	/* Assume tcp_chan is initialized with a nul
				 * port. */
#else
	if (sm_rfp != NULL)
		fclose (sm_rfp);
	if (sm_wfp != NULL)
		fclose (sm_wfp);
#endif
	s_alarm (0);
#if AMOEBA
	memset(&tcp_chan.cap_port, '\0', sizeof(tcp_chan.cap_port));
#else
	sm_rfp = sm_wfp = NULL;
#endif
	phs_note (sm_chptr, PHS_CNEND);
}
