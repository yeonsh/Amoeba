/*	@(#)diag.c	1.4	96/02/27 10:21:38 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "amoeba.h"
#include "stderr.h"
#include "capset.h"
#include "module/mutex.h"
#include "soap/soap.h"
#include "soap/soapsvr.h"

#include "global.h"
#include "caching.h"
#include "filesvr.h"
#include "superfile.h"
#include "diag.h"
#include "misc.h"
#include "seqno.h"


/* Identifier of this soap server (i.e. 0 or 1) */
static int Sp_whoami;

int
sp_whoami()
{
    return Sp_whoami;
}

void
sp_set_whoami(who)
int who;
{
    Sp_whoami = who;
}


static char *
buf_add_string(buf, end, str)
char *buf;
char *end;
char *str;
{
    int len = strlen(str);

    if (buf != NULL && end - buf > len) {
	(void) strcpy(buf, str);
	return buf + len;
    } else {
	return buf;
    }
}

/*
 * Load status information into a char buffer:
 */
char *
buf_put_status(buf, end)
char *buf;
char *end;
{
    char strbuf[256];
    capability filesvr[NBULLETS];
    objnum obj;

    /* statistics counters: */
    int in_use = 0;	  /* # directories				 */
    int in_cache = 0;	  /* # directories in cache			 */
    int unreplicated = 0; /* # directories with < NBULLETS file replicas */
    int fresh = 0;	  /* # directories with maximum  time-to-live	 */
    int expired = 0;	  /* # directories with <= 0 time-to-live	 */
    int bad_port = 0;	  /* # directories on wrong file server		 */

    if (buf == NULL || _sp_table == NULL) {
	return NULL;
    }

    fsvr_get_svrs(filesvr, NBULLETS);

    /* compute some statistics */
    for (obj = 1; obj < _sp_entries; obj++) {
	if (sp_in_use(&_sp_table[obj])) {
	    capability filecaps[NBULLETS];
	    int ttl, j;

	    ++in_use;
	    if (!out_of_cache(obj)) {
		++in_cache;
	    }

	    get_dir_caps(obj, filecaps);
	    if (caps_zero(filecaps, NBULLETS) > 0) {
		++unreplicated;
	    }

	    for (j = 0; j < NBULLETS; j++) {
		if (!NULLPORT(&filecaps[j].cap_port) &&
		    !PORTCMP(&filecaps[j].cap_port, &filesvr[j].cap_port))
		{
		    bad_port++;
		    break;
		}
	    }

	    if ((ttl = get_time_to_live(obj)) == SP_MAX_TIME2LIVE) {
		++fresh;
	    } else if (ttl == 0) {
		++expired;
	    }
	}
    }

    (void) sprintf(strbuf, "\nSOAP Server %d in 0x%x copy mode:\n",
		   sp_whoami(), get_copymode());
    buf = buf_add_string(buf, end, strbuf);
    
    (void) sprintf(strbuf, "                               In  Unrepli-\n");
    buf = buf_add_string(buf, end, strbuf);
    
    (void) sprintf(strbuf,
	"Directories   In Use   Free  Cache  cated   Fresh Expired Badport\n");
    buf = buf_add_string(buf, end, strbuf);
    
    (void) sprintf(strbuf, "              %6d %6d %6d %6d %7d %7d %7d\n",
		   in_use, _sp_entries - in_use, in_cache, unreplicated,
		   fresh, expired, bad_port);
    buf = buf_add_string(buf, end, strbuf);
    
    (void) sprintf(strbuf, "\nRows in cache: %6d\n", cached_rows());
    buf = buf_add_string(buf, end, strbuf);
    
#ifdef SOAP_DIR_SEQNR
    {
	sp_seqno_t seqno;

	get_global_seqno(&seqno);
	(void) sprintf(strbuf, "Global seqno: [%d,%d]\n",
		       hseq(&seqno), lseq(&seqno));
	buf = buf_add_string(buf, end, strbuf);
    }
#endif

    return buf + 1;
}


#ifdef __STDC__
#define va_dostart(ap, format)  va_start(ap, format)
#else
#define va_dostart(ap, format)  va_start(ap)
#endif

#ifdef __STDC__
void panic(char *format, ...)
#else
/*VARARGS1*/
void panic(format, va_alist) char *format; va_dcl
#endif
{
    va_list ap;

    va_dostart(ap, format);
    fprintf(stderr, "\nSOAP %d: ***** Panic ***** ", sp_whoami());
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);

#ifdef DEBUG
    abort();
#else
    exit(1);
#endif
}

static mutex print_mu;

void
start_printing()
{
    mu_lock(&print_mu);
}

void
stop_printing()
{
    mu_unlock(&print_mu);
}

#ifdef __STDC__
void scream(char *format, ...)
#else
/*VARARGS1*/
void scream(format, va_alist) char *format; va_dcl
#endif
{
    va_list ap;

    va_dostart(ap, format);
    start_printing();
    printf("SOAP %d: !!!! Warning !!!! ", sp_whoami());
    vfprintf(stdout, format, ap);
    printf("\n");
    stop_printing();
    va_end(ap);
}

#ifdef __STDC__
void message(char *format, ...)
#else
/*VARARGS1*/
void message(format, va_alist) char *format; va_dcl
#endif
{
    va_list ap;

    va_dostart(ap, format);
    start_printing();
    printf("SOAP %d: ", sp_whoami());
    vfprintf(stdout, format, ap);
    printf("\n");
    stop_printing();
    va_end(ap);
}
