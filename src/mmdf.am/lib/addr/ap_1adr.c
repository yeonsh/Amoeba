/*	@(#)ap_1adr.c	1.1	92/05/14 10:38:17 */
#include "util.h"
#include "conf.h"
#include "ap.h"
#include "ap_lex.h"

/*  ADDRESS PARSER, as per:

    "Standard for the Format of ARPA Network Text Messages", D.  Crocker,
    J. Vittal, K. Pogran & D. Henderson, in ARPANET PROTOCOL HANDBOOK, E.
    Feinler & J. Postel (eds), NIC-7104-REV-1, Network Information Center,
    SRI International:  Menlo Park, Ca.  (1978) (NTIS AD-A0038901).

    and

    "Standard for the Format of Arpa Internet Text Messages", Revised
    by D. Crocker, RFC #822, in INTERNET PROTOCOL TRANSITION WORKBOOK,
    Feinler & J. Postel (eds), Network Information Center, SRI
    International:  Menlo Park, Ca.  (March 1982).

    A parsed address is normalized to have routing references placed
    into rfc822-type positioning.

    History:

    Fall 1977     Bruce Borden:  Initial Design & Coding
    Summer 1979   Dave Crocker:  Completed it & fixed bugs
				 Major reorganization & re-labeling
				 Minor changes to semantics
				 Documentation
    Sept 81       Andy Knutsen   case STPREND -> STPHRSE, to allow comments
				 afterwards
    Sept 81       Dave Crocker   generalized the use of STPHRSE, so that
				 STOKEND occurs only on comma or eof.
				 changed again, to cycle only accepting
				 comments
    Nov 82        Dave Crocker   Converted to accept 822 syntax, while
				 trying also to juggle 733...
    Aug 83        Steve Kille    Fix to STEPER

    This module is the real parser, tho it is intended to be co-routined
    with a caller who has something specific to do with ap_1adr's output.
    This is organized so as to make some attempt at limiting core
    consumption.  Fullparse(), however, simply causes a full parse tree to
    be built up in core.

    The implementation deviates somewhat from the above specification.
    Deviations and the use of the package are discussed in the companion
    documentation.

    The parser's behavior is fairly straightforward.  A singly-linked flat
    list of labelled (lexical) nodes is built up.  Ap_1adr() is used to get
    the next "address segment", which consists of all of the lexical nodes
    up to the end of the next host-phrase (i.e., usually that means up to
    the next comma, semi-colon, or right-angle bracket).

    The caller is responsible for initializing state variables, via
    ap_init(), and linking together or using ap_1adr's output segments.

    Note that ap_1adr does NOT interpret address text to cause re-directed
    file input.  The caller must do that.  Ap_pshfil() and ap_popfil() can
    be used to save and restore the parse state and acquire the named file.
    The provision for this stacking, given the co-routining, is the reason
    state information is chained through global variables, rather than
    being saved on local (stack) variables.

    The amount of input processed on a single call may seem strange.  The
    idea is to try to guess the most common use of the routine.  This is
    presumed to be for address checking, in which acquisition of the MBOX
    and DOMAIN text are most important, with the rest actually being thrown
    away.  It is, of course, possible for the core-limiting heuristic to
    lose if a ridiculous number of groups and personal lists are specified
    in a particular way.  I am assuming that won't happen.
/**/

#define STITER     0
#define STINIT     1
#define STECMNT    2
#define STEDONE    3    /* Returned when addresses were NOT found */
#define STEOK      4    /* Returned when addresses were found */
#define STEBAD     5
#define STPHRSE    6
#define STSTPER    7
#define STEPER     8
#define STEGRP     9
#define STDTYPE   10
#define STEDTYPE  11
#define STDOMAIN  12
#define STEDOMAIN 13

int  ap_intype = AP_733;          /* default to RFC #733 input           */
int  ap_outtype = AP_822;
				/* default to RFC #822 output          */
				/* with little endian domains          */


int ap_grplev = 0;                /* Group nesting depth                  */
int ap_perlev = 0;                /* <> nesting depth                     */
int ap_routing;                   /* parsing a route                    */

LOCFUN ap_7to8();

#ifdef DEBUG
#include "ll_log.h"

extern LLog *logptr;
extern AP_ptr ap_sqinsert ();
#endif
extern char *strdup();

#if DEBUG > 1
int	debug;                    /* True if in debug mode                */
char   *statnam[] =
{
    "Iterate", "Init", "CmntEnd", "DoneEnd", "OKEnd", "BadEnd", "Phrase",
    "Persstrt", "PersEnd", "GrpEnd", "DTypNam", "DTypeE", "Domain",
    "DomainE"
};

char   *ptrtab[] =
{
    "Nil", "Etc", "Nxt"
};

char   *typtab[] =
{
    "Nil", "Name", "MBox", "Domain", "DataType", "Comment", "Word",
    "Person", "NPersn", "EPersn", "Group", "NGroup", "EGroup", "DomainLit"
};
#endif
/**/

ap_1adr ()
{
    struct ap_node basenode;
    AP_ptr ap_sp;        /* Saved ap node ptr                    */
    AP_ptr r822ptr,
	   r733prefptr;
    int    got822;
    char    buf[LINESIZE];
    register int    state;

#if DEBUG > 1
    if (debug)
	(void) putchar ('\n');
#endif

    ap_routing = DONE;
    ap_ninit (&basenode);
    ap_sqinsert (&basenode, APP_ETC, ap_pstrt);
    for (state = STINIT, got822 = FALSE, r733prefptr = (AP_ptr) 0; ;){
#if DEBUG > 1
	if (debug)
	    printf ("=>%d (%s)", state,
		(state >= 0 && state <= 13) ? statnam[state] : "BOGUS!");
#endif
	switch (state)
	{
	    case STITER:          /* Iteration to get real address        */
		ap_palloc ();
		state = STINIT;   /* just DROP ON THROUGH                 */
	    case STINIT:          /* START of parse; empty node           */
		ap_sp = ap_pcur;
		switch (ap_lex (buf)){
		    case LV_WORD:
			ap_pfill (APV_WORD, buf);
			state = STPHRSE;
			break;
		    case LV_AT:
			if (!got822){
			    got822 = TRUE;
			    r822ptr = ap_pcur;
			}
			ap_routing = OK;
#if DEBUG > 1
			if (debug)
			    printf ("(routing)");
#endif
			state = STDOMAIN;
			break;
		    case LV_COLON:
				  /* DATA TYPE start                      */
			state = STDTYPE;
			break;
		    case LV_SEMI: /* GROUP LIST end                       */
			state = STEGRP;
			break;
		    case LV_GRTR: /* PERSONAL LIST end                    */
			state = STEPER;
			break;
		    case LV_LESS: /* allow one angle-bracket, here */
		    case LV_FROM: /* FILE source                          */
			ap_pfill (APV_DTYP, "Include");
			state = STITER;
			break;
		    case LV_COMMENT:
			ap_pfill (APV_CMNT, buf);
			state = STITER;
			break;
		    case LV_COMMA:
			break;    /* ignore null addresses                */
		    case LV_EOD:
			if (ap_perlev != 0 || ap_grplev != 0)
				state = STEBAD;
			else
				state = STEDONE;
			break;
		    default:
			state = STEBAD;
			break;
		}
		continue;


/* ***************************  ENDING  ********************************* */

	    case STECMNT:         /* accept comments until end            */
		switch (ap_lex (buf)) {
		    case LV_COMMENT:
			ap_pfill (APV_CMNT, buf);
			break;    /* just cycle, accepting comments     */
		    case LV_COMMA:
			state = STEOK;
			break;
		    case LV_EOD:
			state = STEOK;
			break;
		    default:
			state = STEBAD;
			break;
		}
		continue;

	    case STEDONE:         /* END clean; NO empty nodes?           */
#if DEBUG > 1
		if (debug)
		    (void) putchar ('\n');
#endif
		ap_7to8 (r733prefptr, r822ptr);
		return (DONE);

	    case STEOK:         /* END clean                            */
#if DEBUG > 1
		if (debug)
		    (void) putchar ('\n');
#endif
		ap_7to8 (r733prefptr, r822ptr);
		return (OK);

	    case STEBAD:         /* END error                            */
#if DEBUG > 1
		if (debug)
		    (void) putchar ('\n');
#endif
		ap_clear();		/* Experimental, DPK, 7 Aug 84 */
		return (NOTOK);
/* *********************  GATHER A PHRASE  **************************** */

	    case STPHRSE:         /* PHRASE continuation; empty node      */
		switch (ap_lex (buf)) {
		    case LV_WORD: /* append word to phrase, maybe         */
			ap_padd (APV_WORD, buf);
			break;
		    case LV_AT:   /* MAILBOX (name) end                 */
			if (!got822)
			    r822ptr = ap_pcur;
			ap_sqtfix (ap_sp, ap_pcur, APV_MBOX);
			ap_palloc ();
			state = STDOMAIN;
			break;
		    case LV_LESS: /* PERSON NAME end                      */
			state = STSTPER;
			break;
		    case LV_COLON:
				  /* GROUP NAME end                       */
			if (ap_grplev++ >= 1 && ap_intype == AP_822)
			{        /* may not be nested */
#if DEBUG > 1
			    if (debug)
				printf ("(intype=%d,ap_grplev=%d)",
					   ap_intype, ap_grplev);
#endif
			    state = STEBAD;
			    break;
			}
			ap_sqtfix (ap_sp, ap_pcur, APV_GRUP);
			ap_sp -> ap_obtype = APV_NGRP;
			state = STITER;
			break;
		    case LV_SEMI:
			ap_sqtfix (ap_sp, ap_pcur, APV_MBOX);
			ap_sp -> ap_obtype = APV_MBOX;
			state = STEGRP;
			break;
		    case LV_GRTR:
			state = STEPER;
			break;
		    case LV_COMMA:
			ap_sqtfix (ap_sp, ap_pcur, APV_MBOX);
			state = STEOK;
			break;
		    case LV_EOD:
			ap_sqtfix (ap_sp, ap_pcur, APV_MBOX);
			state = STEOK;
			break;
		    case LV_COMMENT:
			ap_pappend (APV_CMNT, buf);
			break;
		    default:
			state = STEBAD;
			break;
		}
		continue;

/* ***********************  ADDRESS LISTS  **************************** */

	    case STSTPER:         /* PERSONAL address list; NO empty node */
		if (ap_perlev++ > 0 && ap_intype == AP_822)
		{        /* may not be nested */
#if DEBUG > 1
		    if (debug)
			printf ("(intype=%d,ap_perlev=%d)",
				   ap_intype, ap_perlev);
#endif
		    state = STEBAD;
		    break;
		}
		ap_routing = OK;
		ap_sqtfix (ap_sp, ap_pcur, APV_PRSN);
		ap_sp -> ap_obtype = APV_NPER;
		state = STITER;
		continue;

	    case STEPER:
		if (--ap_perlev < 0) {
#if DEBUG > 1
		    if (debug)
			printf ("(ap_perlev=%d)", ap_perlev);
#endif
		    state = STEBAD;
		    break;
		}
		ap_pappend (APV_EPER, (char *) 0);
		ap_palloc ();   /* SEK add storage */
		state = STECMNT; /* allow comments, etc */
		continue;

	    case STEGRP:
		if (--ap_grplev < 0) {
#if DEBUG > 1
		    if (debug)
			printf ("(ap_grplev=%d)", ap_grplev);
#endif
		    state = STEBAD;
		    break;
		}
		ap_pappend (APV_EGRP, (char *) 0);
		state = STECMNT;
		continue;


/* **************************  DATA TYPE  ******************************* */

	    case STDTYPE:          /* DATA TYPE name; empty node           */
		if (ap_intype == AP_822)
		{        /* data types not legal in 822 */
#if DEBUG > 1
		    if (debug)
			printf ("(intype=%d)", ap_intype);
#endif
		    state = STEBAD;
		    break;
		}
		if (ap_lex (buf) != LV_WORD) {
		    state = STEBAD;
		    continue;
		}
		ap_pfill (APV_DTYP, buf);
		state = STEDTYPE;
				  /* Just drop on through                 */
	    case STEDTYPE:         /* DATA TYPE name end; empty node       */
		state = (ap_lex (buf) == LV_COLON)
		    ? STITER : STEBAD;
		continue;
/* *************************  DOMAIN  *********************************** */

	    case STDOMAIN:        /* DOMAIN/Host; NO empty parse node   */
		switch (ap_lex (buf)) {
		    default:
			state = STEBAD;
			continue;
		    case LV_COMMENT:
			ap_pappend (APV_CMNT, buf);
			continue;
		    case LV_DLIT:
			ap_pappend (APV_DLIT, buf);
			state = STEDOMAIN;
			continue;
		    case LV_WORD:
			ap_pfill (APV_DOMN, buf);
			state = STEDOMAIN;
		}                 /* just drop on through                 */

	    case STEDOMAIN:       /* DOMAIN end; NO empty parse node      */
		switch (ap_lex (buf)) {
		    case LV_AT:   /* sequence of HOST's => @ separation   */
				  /* SEK correct here              */
			if (r733prefptr == (AP_ptr) 0)
			    r733prefptr = ap_pcur;
			ap_palloc ();
				 /* node which points to first routing ref */
			state = STDOMAIN;
			break;
		    case LV_SEMI:
			state = STEGRP;
			break;
		    case LV_GRTR:
			state = STEPER;
			break;
		    case LV_COMMA:
			if (ap_routing != DONE)
			    state = STITER;
			else
			    state = STEOK;
			break;
		    case LV_EOD:
			if (ap_routing != DONE || 
			    ap_perlev != 0 || ap_grplev != 0)
			    state = STEBAD;
			else
			    state = STEOK;
			break;
		    case LV_COMMENT:
			ap_pappend (APV_CMNT, buf);
			break;
		    case LV_COLON:
			if (ap_routing != DONE) {
			    ap_routing = DONE;
			    state = STITER;
			    continue;
			}       /* else DROP ON THROUGH */
		    default:
			state = STEBAD;
			break;
		}
		continue;
	}
    }
}



LOCFUN
ap_7to8 (r733prefptr, r822ptr)
    AP_ptr r733prefptr,
	    r822ptr;
{
    AP_ptr routbase;
    AP_ptr ap;
    char  *perneeded;

    if (r733prefptr == (AP_ptr) 0)
	return;               /* don't have to move it to 822 style     */

#ifdef DEBUG
    ll_log (logptr, LLOGFTR, "733 ref");
#endif

   if (ap_pstrt -> ap_obtype == APV_MBOX) {
	perneeded = strdup (r822ptr -> ap_obvalue);
	ap_pappend (APV_EPER, (char *) 0);
   }
   else
	perneeded = (char *) 0;

    routbase = ap_alloc (); /* move the sequence to newroute        */
    while (r733prefptr -> ap_ptrtype != APP_NIL &&
	    r733prefptr -> ap_ptrtype != APP_NXT &&
	    r733prefptr -> ap_chain != (AP_ptr) 0)
   {
	switch (r733prefptr -> ap_chain -> ap_obtype)
	{
	    default:     /* only move domain info */
		goto endit;

	    case APV_CMNT:
		if (routbase -> ap_chain != (AP_ptr) 0)
		{           /* try to append, not prepend, comments */
#ifdef DEBUG
		    ll_log (logptr, LLOGFTR,
			"comment appended'%s'", r733prefptr -> ap_chain -> ap_obvalue);
#endif
		    ap_move (routbase -> ap_chain, r733prefptr);
		    continue;
		}           /* else DROP ON THROUGH */
	    case APV_DLIT:
	    case APV_DOMN:
#ifdef DEBUG
		ll_log (logptr, LLOGFTR,
			"val='%s'", r733prefptr -> ap_chain -> ap_obvalue);
#endif
		ap_move (routbase, r733prefptr);
		continue;
	}
    }

endit:
			/* treatment here depends on whether we have an */
			/*  822 route already.  Note that 822 pointer is*/
			/*  NOT easy,  as an easy pointer was too hard! */
			/* SEK need to copy first part of route */
			/* to r822ptr                       */
   if (r822ptr -> ap_obtype != APV_DOMN && r822ptr -> ap_obtype != APV_DLIT) {
	ap_insert (r822ptr, APP_ETC,
		   ap_new (r822ptr -> ap_obtype,
				 r822ptr -> ap_obvalue));
	free (r822ptr -> ap_obvalue);
	ap_fllnode (r822ptr, routbase -> ap_chain -> ap_obtype,
					routbase -> ap_chain -> ap_obvalue);
	ap_sqinsert (r822ptr, APP_ETC, routbase -> ap_chain -> ap_chain);
		       /* add it before local-part         */
	ap_free (routbase);
	ap_free (routbase -> ap_chain);
   } else {
	ap = r822ptr;
	while (ap -> ap_chain -> ap_obtype == APV_DOMN ||
		    ap -> ap_chain -> ap_obtype == APV_DLIT)
	   ap = ap -> ap_chain;
	ap_sqinsert (ap, APP_ETC, routbase -> ap_chain);
   }


   if (perneeded != 0)          /* SEK need to kludge person name       */
   {
	ap_insert (r822ptr, APP_ETC,
		   ap_new (r822ptr -> ap_obtype,
				 r822ptr -> ap_obvalue));
	free (r822ptr -> ap_obvalue);
	ap_fllnode (r822ptr, APV_NPER, perneeded);
	free (perneeded);
   }
}

