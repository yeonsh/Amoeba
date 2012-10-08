/*	@(#)chan.c	1.1	91/11/20 16:49:10 */
#include "util.h"
#include "mmdf.h"
#include "ch.h"
#include "dm.h"

char    *ch_dflnam = "local";

Alias	*al_list = (Alias *)0;

/*******************  MAIL IDENTIFIER  SELECTION  ***********************/

/*
 *	The global mid_enable controls the use of an alternate
 *	mail name system refered to as Mail-Ids.  When mail-ids are
 *	in use, login-ids and mail-ids are nolong bound to one another
 *	by the system account database, and are bound only by mapping
 *	tables.  The tables "mailids" and "users" will always be present
 *	though they may not be used.  "mailids" maps a mailid to a
 *	system account.  The table "users" maps a username to a mailid.
 */

int	mid_enable = 0;

Table
	tb_mailids =		/* Used in getpwmid() */
{
    "mailids", "Mail-ID to Username Mappings", "mailids", 0, 0l
},
	tb_users =		/* Used in getmailid() */
{
    "users", "Username to Mail-ID Mappings", "users", 0, 0l
};


/*
 *  The following set of structures provides a complete list of channels
 *  known to MMDF.
 *
 *  It is followed by two arrays that list the channels in useful orders.
 *
 *  ch_tbsrch[] is a full list, of all known channels, and is primarily
 *  used to guide the ch_table routines when treating the hostname space as
 *  flat (linear).  It also is used by lnk_submit, for sorting the address
 *  linked-list.
 *
 *  THE ORDER IS CRITICAL.  A hostname may appear in several tables, but
 *  the first table checked will cause the hit.
 *
 *  One nice aspect of this is that, for example, you can have a standard
 *  arpanet host table, but have some of its hosts actually get mail on a
 *  different channel.  Simply place that table's entry earlier in the
 *  ch_tbsrch[] list.
 *
 *  ch_exsrch[] is a list of channels that are processed by Deliver
 *  (usually just when it does standard, default daemon processing).
 *  Therefore, it need contain only those channels that are active and are
 *  to be processed by the daemon.  Again, order is important.  Channels
 *  are processed in the order listed.  This means that, typically, you
 *  want ch_sloc to be first, unless you have the strange view that foreign
 *  mail is more important than local...
 */

LOCVAR Chan *chsrch[NUMCHANS+1] = {	/* Order chan tables searched */
	(Chan *) 0
};
Chan **ch_tbsrch = chsrch;

LOCVAR Chan *exsrch[NUMCHANS+1] = {	/* Order of active chan execution */
	(Chan *) 0
};
Chan **ch_exsrch = exsrch;
Chan * ch_ptrlist[NUMCHANS + 1];  /* can hold pointers to chans */

LOCVAR Table *tblist[NUMTABLES+1] = {	/* All known tables */
    (Table *) 0
};
Table **tb_list = tblist;

LOCVAR Domain *dmlist[NUMDOMAINS+1];
Domain **dm_list = dmlist;         /* all known domains */

int     tb_maxtables = NUMTABLES; /* max number of slots */
int     ch_maxchans = NUMCHANS;   /* max number of slots */
int     dm_maxtables = NUMDOMAINS; /* max number of slots */

/* These next entries should reflect the ACTUAL number of slots in use  */
int     tb_numtables = 0;         /* actual number of tables */
int     ch_numchans = 0;          /* actual number of channels */
int     dm_numtables = 0;         /* actual number of domains */
