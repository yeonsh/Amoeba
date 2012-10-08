/*	@(#)qu2lo_send.c	1.2	92/05/14 10:40:42 */
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
/*                  SEND FROM DELIVER TO LOCAL MACHINE
 *
 *  Jul 81 Dave Crocker     change to have a child handle actual delivery,
 *          + BRL crew      setuid'ing to recipient.
 *  Aug 81 Steve Bellovin   mb_unlock() added to x2y_each
 *  Apr 84 Doug Kingston    Major rewrite.
 *  Aug 84 Julian Onions    Extended the delivery file syntax quite a bit
 *  Feb 85 Julian Onions    Reworked to parse only if really really necessary
 */

#include "util.h"
#include "mmdf.h"
#include "phs.h"
#include "ap.h"
#include "ch.h"
#include <pwd.h>
#include <sys/stat.h>
#include <signal.h>
#include <sgtty.h>
#include "adr_queue.h"

char    lo_info[2 * LINESIZE],
	lo_sender[2 * LINESIZE],
	lo_adr[2 * LINESIZE],
	lo_replyto[2 * LINESIZE],
	lo_size[16],
	*lo_parm;

struct passwd *lo_pw;

extern Chan     *chanptr;
extern LLog     *logptr;

extern int      errno;

extern char     *qu_msgfile;          /* name of file containing msg text   */
extern long     qu_msglen;

extern char *index();
extern struct passwd *getpwmid();

LOCFUN qu2lo_each(), lo_verify(), lo_master();

LOCVAR struct rp_construct
rp_aend  =
{
	RP_OK, 'l', 'o', 'c', ' ', 'e', 'n', 'd', ' ', 'o', 'f', ' ', 'a',
	'd', 'd', 'r', ' ', 'l', 'i', 's', 't', '\0'
},
rp_hend  =
{
	RP_NOOP, 'e', 'n', 'd', ' ', 'o', 'f', ' ', 'h', 'o', 's', 't', ' ',
	'i', 'g', 'n', 'o', 'r', 'e', 'd', '\0'
};

/**/

qu2lo_send ()                       /* overall mngmt for batch of msgs    */
{
	short     result;

#ifdef DEBUG
	ll_log (logptr, LLOGBTR, "qu2lo_send ()");
#endif

	if (rp_isbad (result = qu_pkinit ()))
		return (result);

	/* While there are messages to process... */
	for(;;){
		result = qu_rinit (lo_info, lo_sender, chanptr -> ch_apout);
		if(rp_gval(result) == RP_NS){
			qu_rend();
			continue;
		}
		else if(rp_gval(result) != RP_OK)
			break;
		phs_note (chanptr, PHS_WRSTRT);

		sprintf (lo_size, "%ld", qu_msglen);
		result = qu2lo_each ();
		qu_rend();
		if (rp_isbad (result))
			return (result);
	}

	if (rp_gval (result) != RP_DONE)
	{
		ll_log (logptr, LLOGTMP, "not DONE (%s)", rp_valstr (result));
		return (RP_RPLY);         /* catch protocol errors      */
	}

	qu_pkend ();                  /* done getting messages          */
	phs_note (chanptr, PHS_WREND);

	return (result);
}
/**/

LOCFUN
qu2lo_each ()               /* send copy of text per address      */
{
	RP_Buf  replyval;
	short   result;
	char    host[LINESIZE];

#ifdef DEBUG
	ll_log (logptr, LLOGBTR, "qu2lo_each()");
#endif
	/* while there are addessses to process... */
	FOREVER
	{
		result = qu_radr (host, lo_adr);
		if (rp_isbad (result))
			return (result);      /* get address from Deliver */

		if (rp_gval (result) == RP_HOK)
		{                       /* no-op the sub-list indication */
			qu_wrply ((RP_Buf *) &rp_hend, rp_conlen (rp_hend));
			continue;
		}

		if (rp_gval (result) == RP_DONE)
		{
			qu_wrply ((RP_Buf *) &rp_aend, rp_conlen (rp_aend));
			return (RP_OK);       /* end of address list */
		}

		replyval.rp_val = lo_verify ();
		if (rp_isbad (replyval.rp_val)) {
			qu_wrply (&replyval, (sizeof replyval.rp_val));
			continue;                       /* go for more! */
		}

		replyval.rp_val = lo_master ();
		switch (rp_gval (replyval.rp_val)) {
		case RP_LIO:
		case RP_TIME:
			/* RP_AGN prevents deliver from calling us dead */
			replyval.rp_val = RP_AGN;
		case RP_LOCK:
		case RP_USER:         /* typicial valid responses */
		case RP_BHST:
		case RP_NOOP:
		case RP_NO:
		case RP_MOK:
		case RP_AOK:
		case RP_OK:
			break;
		default:              /* not expected so may abort          */
			if (rp_isgood (replyval.rp_val))
			{
				ll_log (logptr, LLOGFAT,
				"lo_wadr = %s", rp_valstr (replyval.rp_val));
				replyval.rp_val = RP_RPLY;  /* Conservative */
			}
		}
		qu_wrply (&replyval, (sizeof replyval.rp_val));
	}
}

LOCFUN
lo_verify ()                    /* send one address spec to local     */
{
	char    mailid[MAILIDSIZ];      /* mailid of recipient */
	register char   *p;
	extern char     *strncpy();
#ifdef DEBUG
	ll_log (logptr, LLOGBTR, "lo_verify()");
#endif
	/* Strip any trailing host strings from address */
	if (p = index (lo_adr, '@'))
		*p = '\0';

	for (p = lo_adr; ; p++) {
		switch (*p) {
		case '|':
		case '/':
		case '=':
		case '\0':
			lo_parm = p;
			strncpy (mailid, lo_adr, p - lo_adr);
			mailid[p - lo_adr] = '\0';
			goto checkit;
		}
	}

checkit:
	if ((lo_pw = getpwmid (mailid)) == (struct passwd *) NULL) {
		ll_err (logptr, LLOGTMP, "user '%s' unknown", mailid);
		return (RP_USER);     /* can't deliver to non-persons       */
	}
	return (RP_OK);          /* Invalid addresss or real problems */
}

LOCFUN
lo_master()
{
	int   childid;                /* child does the real work           */

	switch (childid = fork ()) {
	case NOTOK:
		return (RP_LIO);        /* problem with system */

	case OK:
		ll_close (logptr);      /* since the process is splitting */
#ifndef AMOEBA /* Amoeba doesn't have initgroups. */
#ifdef V4_2BSD
		if (initgroups (lo_pw->pw_name, lo_pw->pw_gid) == NOTOK
		  || setgid (lo_pw->pw_gid) == NOTOK
#else
		if (setgid (lo_pw->pw_gid) == NOTOK
#endif /* V4_2BSD */
		  || setuid (lo_pw->pw_uid) == NOTOK) {
			ll_err (logptr, LLOGTMP, "can't set id's (%d,%d)",
				lo_pw->pw_uid, lo_pw->pw_gid);
			exit (RP_BHST);
		}

		if (chdir (lo_pw->pw_dir) == NOTOK) {
			/* move out of MMDF queue space       */
			ll_err (logptr, LLOGTMP, "can't chdir to '%s'",
				lo_pw->pw_dir);
			exit (RP_LIO);
		}
#endif /* AMOEBA */
		exit (lo_slave());

	default:                  /* parent just waits  */
		return (pgmwait (childid));
	}
	/* NOTREACHED */
}
