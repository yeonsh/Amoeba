/*	@(#)ml_send.c	1.1	91/11/21 09:53:00 */
#include "util.h"
#include "mmdf.h"
#include "cnvtdate.h"
#include <pwd.h>

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
/* send a piece of mail, using a simple interface to submit */

/*  Basic sequence is:
 *
 *          ml_init (YES, NO, "My Name", "The Subject");
 *          ml_adr ("destination address 1");
 *          ml_adr ("destination address 2");
 *          ...
 *          ml_aend ();
 *          ml_tinit ();
 *          ml_txt ("Some opening text");
 *          ml_txt ("maybe some more text");
 *          ml_file (file-stream-descriptor-of-file-to-include);
 *          if (ml_end (OK)) != OK)
 *          {   error-handling code }
 *
 *  Arguments that are to be defaulted should be zero.
 *
 *  ml_init's arguments specify a) whether return-mail (to the sender
 *  should be allowed, b) whether a Sender field should be used to
 *  specify the correct sender (contingent on next argument), c) text
 *  for the From field, and d) text for the Subject field.  If (b) is
 *  NO, then (c)'s text will be followed by the correct sender
 *  information.
 *
 *  ml_to and ml_cc are used to switch between To and CC addresses.
 *  Normally, only To addresses are used and, for this, no ml_to call is
 *  needed.
 *
 *  An "address" is whatever is valid for your system, as if you were
 *  typing it to the mail command.
 *
 *  You may freely mix ml_txt and ml_file calls.  They just append text
 *  to the message.  The text must contain its own newlines.
 *
 *  State diagram: ml_state:
 *       ML_FRESH --> submit running --> ML_MSG  --> ready for new message -->
 *       ML_HEADER --> reading header --> ML_TEXT --> message sent --> ML_MSG
 *
 */

#include <signal.h>

extern char *mldfldir;
extern struct ll_struct   *logptr;
extern char *strdup ();
extern char *locfullname;
extern char *index ();
extern char *getenv();

int ml_state = ML_FRESH;

static char *fieldname;
static char *logdir;
static char *username;

/**/

ml_init (ret, sndr, from, sub)    /* set-up for using submit            */
int     ret,                      /* allow return mail to sender?       */
	sndr;                     /* include Sender field?              */
char    *sub,                     /* subject line                       */
        *from;			  /* from field                         */
{
    static first_ml = TRUE;
    char linebuf[LINESIZE];
    char datbuf[64];
    extern struct passwd *getpwuid ();
    extern char *getmailid ();
    struct passwd  *pwdptr;
    int     realid,
	    effecid;

    if (first_ml) {
        getwho (&realid, &effecid);   /* who am i?                          */
        
        if ((pwdptr = getpwuid (realid)) == (struct passwd *) NULL)
            return(NOTOK);
        
username = getenv("USER");

        /*  save login directory   */
        logdir = strdup(pwdptr -> pw_dir);
        
/*        if ((username = getmailid(pwdptr -> pw_name)) == NULL)
            return(NOTOK);
*/
	first_ml = FALSE;
    }

    if (ml_state != ML_FRESH && ml_state != ML_MSG) {  /* odd state; resync */
	mm_end(NOTOK);
	ml_state = ML_FRESH;
    }

    if (ml_state == ML_FRESH) {
	if (rp_isbad(mm_init()) || rp_isbad(mm_sbinit()))
	    return(NOTOK);
	ml_state = ML_MSG;
    }

    strcpy (linebuf, "rmxto,cc*");
    if (!ret)                     /* disable return to sender           */
	strcat(linebuf, "q");

    if (rp_isbad(mm_winit((char *) 0, linebuf, (char *) 0))) {
	ml_end(NOTOK);
	return(NOTOK);
    }
    ml_state = ML_HEADER; 

    cnvtdate (TIMREG, datbuf);
    ml_sndhdr("Date:", datbuf);

    if (from != 0)
    {                             /* user-specified From field          */
	if (sndr) {
	    ml_sndhdr("From:", from);
	    ml_sender("Sender:", (char *) 0);
	}
	else
	    ml_sender("From:", from);
    }
    else
        ml_sender("From:", (char *) 0);

    if (sub != 0) 		  /* user-specified Subject field       */
	ml_sndhdr("Subject:", sub);

    return (ml_to ());		  /* set-up for To: addresses           */
}
/**/


ml_to ()			  /* ready to specify To: address       */
{
    fieldname = "To:";
    return (OK);
}

ml_cc ()			  /* ready to specify CC: address       */
{
    fieldname = "Cc:";
    return (OK);
}

ml_adr (address)		  /* a destination for the mail         */
char    address[];
{
    ml_sndhdr (fieldname, address);
    return (OK);
}

ml_aend ()			  /* end of addrs                       */
{
    if (ml_state == ML_HEADER) 
	mm_wtxt("\n", 1);
    return (OK);
}

ml_tinit ()                     /* ready to send text                 */
{
    if (ml_state == ML_HEADER)
	ml_state = ML_TEXT;
    return (OK);
}

ml_sndhdr (name, contents)
char *name;
char *contents;
{
    char linebuf[LINESIZE];

    if (ml_state == ML_HEADER) {
	sprintf(linebuf, "%-10s%s\n", name, contents);
	mm_wtxt(linebuf, strlen (linebuf));
    }
    return(OK);
}
/**/
ml_sender (cmpnt, name)
char    cmpnt[],
	name[];
{
    int     sigfd,
	    sigsiz;
    char   *ptr,
	    linebuf[LINESIZE],
	    sigtxt[LINESIZE];     /* where is signature text?           */
    char    gotsig;

    gotsig = FALSE;

    if (name != 0)
    {
	strcpy (sigtxt, name);
	gotsig = TRUE;
    }
    else
    {                             /* user didn't give us a signature    */
	sprintf (linebuf, "%s/%s/.fullname", mldfldir,username);

	if ((sigfd = open (linebuf, 0)) >= 0)
	{                         /* there is a file w/signature?       */
	    if ((sigsiz = read (sigfd, sigtxt, sizeof sigtxt)) > 0) {
	    	char *cp;

		sigtxt[sigsiz - 1] = '\0';
	    	if (cp = index(sigtxt, '\n'))
	    	    *cp = '\0';
	    	if (sigtxt[0])
		    gotsig = TRUE;
	    }
	}
    }

    if (gotsig)                   /* real name + mailbox                */
    {
	for (ptr = locfullname; *ptr != 0; ptr++)
	    *ptr = uptolow (*ptr);      /* Screw locfullname */
	sprintf (linebuf, "%s <%s@%s>", sigtxt, username, locfullname);
    }
    else                          /* just the mailbox info              */
	sprintf (linebuf, "%s@%s", username, locfullname);

    ml_sndhdr (cmpnt, linebuf);
}

/*  */
ml_file (infp)                    /* send a file to the message         */
register FILE  *infp;             /* input stdio file stream pointer    */
{
    register short len;
    char    buffer[BUFSIZ];

    if (ml_state != ML_TEXT)
	return (OK);

    while ((len = fread (buffer, sizeof (char), sizeof(buffer), infp )) > 0) {
	if (rp_isbad(mm_wtxt(buffer, len))) {
	    ll_log (logptr, LLOGFAT, "ml_file() mm_wtxt error");
	    ml_end (NOTOK);  
	    return (NOTOK);
	}
    }

    if (!feof(infp))
    {
	ll_log (logptr, LLOGFAT, "ml_file() Error on file");
	ml_end (NOTOK);
	return (NOTOK);
    }
    return (OK);
}

ml_txt (text)                     /* some text for the body             */
char text[];                      /* the text                           */
{
    if (ml_state != ML_TEXT)
	return (OK);

    if (rp_isbad(mm_wtxt(text, strlen(text))))
    {
	ml_end (NOTOK);
	return (NOTOK);
    }
    return (OK);
}
/**/


ml_end (type)			  /* message is finished                */
int     type;                     /* normal ending or not               */
{
    struct rp_bufstruct thereply;
    int len;

    switch (type) {
	case OK:
	    if (ml_state != ML_TEXT || rp_isbad(mm_wtend()) 
				 || rp_isbad(mm_rrply(&thereply, &len)) 
				 || rp_isbad(thereply.rp_val)) {
		mm_end(NOTOK);
		ml_state = ML_FRESH;
		return(NOTOK);
	    }
	    ml_state = ML_MSG;
	    break;

	case NOTOK:
	    mm_end(NOTOK);
	    ml_state = ML_FRESH;
	    break;
    }
    return(OK);
}
/**/

ml_1adr (ret, sndr, from, sub, adr)
				  /* all set-up overhead in 1 proc      */
int     ret,                      /* allow return mail to sender?       */
	sndr;                     /* include Sender field?              */
char    *sub,                     /* subject line                       */
	*from,                    /* from field                         */
	*adr;                     /* the one address to receive msg     */
{

    if (ml_init (ret, sndr, from, sub) != OK ||
	    ml_adr (adr) != OK ||
	    ml_aend () != OK ||
	    ml_tinit () != OK)
	return (NOTOK);

    return (OK);
}
