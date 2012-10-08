/*	@(#)devnullsvr.c	1.6	96/02/27 13:13:45 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Server for null device (/dev/null) */

#include <amtools.h>
#include <ampolicy.h>
#include <file.h>
#include <stdcom.h>
#include <stderr.h>
#include <ajax/mymonitor.h>
#include <module/rnd.h>

#include "session.h"

/* Policy decisions */
#define NTHREAD 2		/* Number of /dev/null server threads */
#define INFO	"null device"	/* std_info return string */

/* File-global variables */
static port privport;		/* Server's private port */
static capability pubcap;	/* Server's published capability */

/*ARGSUSED*/
static void
devnullsvr(p, s)
char *	p;
int	s;
{
	header h;
	bufsize n;
	errstat err;
	bufptr reply;
	
	for (;;) {
		h.h_port = privport;
		n = getreq(&h, NILBUF, 0);
		if (ERR_STATUS(n)) {
			err = ERR_CONVERT(n);
			if (err == RPC_ABORTED) {
				MON_EVENT("devnull: getreq aborted; continue");
			} else {
			    warning("devnull: getreq failed (%s)",
				    err_why(err));
			}
			continue;
		}
		
		reply = 0;
		n = 0;
		err = STD_OK;
		
		switch (h.h_command) {
		
		case STD_INFO:
			MON_EVENT("devnull: STD_INFO");
			reply = INFO;
			h.h_size = n = strlen(reply);
			break;
		
		case FSQ_READ:
			MON_EVENT("devnull: FSQ_READ");
			h.h_size = 0;
			h.h_extra = FSE_NOMOREDATA;
			break;
		
		case FSQ_WRITE:
			MON_EVENT("devnull: FSQ_WRITE");
			break;
		
		default:
			MON_NUMEVENT("devnull: bad command", h.h_command);
			err = STD_COMBAD;
			break;
		
		}
		
		h.h_status = err;
		putrep(&h, reply, n);
	}
}

errstat
start_devnullsvr()
{
	int i;
	
	uniqport(&privport);
	CAPZERO(&pubcap);
	priv2pub(&privport, &pubcap.cap_port);
	for (i = 1; i <= NTHREAD; ++i) {
		if (!thread_newthread(devnullsvr, 4000, NILBUF, 0)) {
			warning("cannot fork /dev/null server thread");
			if (i == 0)
				return STD_NOSPACE;
		}
	}
	return STD_OK;
}

errstat
publish_devnullcap()
{
	errstat err;
	
	(void) name_delete(DEF_NULLSVR);
	if ((err = name_append(DEF_NULLSVR, &pubcap)) != STD_OK)
		warning("cannot store null server cap %s (%s)",
			DEF_NULLSVR, err_why(err));
	return err;
}

errstat
unpublish_devnullcap()
{
	errstat err;
	
	if ((err = name_delete(DEF_NULLSVR)) != STD_OK)
		warning("cannot delete null server cap %s (%s)",
			DEF_NULLSVR, err_why(err));
	return err;
}
